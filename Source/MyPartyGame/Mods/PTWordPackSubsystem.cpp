// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWordPackSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Base64.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/PlatformProcess.h" // UserTempDir
#include "Async/Async.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/WeakObjectPtr.h"

#if PT_WITH_STEAM
#include "steam/steam_api.h"
#include "steam/isteamugc.h"
#include "steam/isteamremotestorage.h" // EWorkshopFileType (k_EWorkshopFileTypeCommunity)
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPTWordPacks, Log, All);

// ==========================================================================
// Helper de publicación al Workshop (async, fuera de UHT)
// ==========================================================================
#if PT_WITH_STEAM
struct FPTWorkshopPublish
{
    TFunction<void(bool, FString)> Cb;
    FString ContentFolder, PreviewPath, Title, Desc, Diag;
    PublishedFileId_t ItemId = 0;
    CCallResult<FPTWorkshopPublish, CreateItemResult_t>       CreateCR;
    CCallResult<FPTWorkshopPublish, SubmitItemUpdateResult_t> SubmitCR;

    void Start(const FString& InFolder, const FString& InPreview, const FString& InTitle,
               const FString& InDesc, TFunction<void(bool, FString)> InCb)
    {
        Cb = MoveTemp(InCb);
        // SetItemContent/SetItemPreview exigen rutas ABSOLUTAS y, en Windows, con separadores NATIVOS
        // (backslashes). Si van relativas o con '/', Steam devuelve InvalidParam=8.
        ContentFolder = FPaths::ConvertRelativePathToFull(InFolder);
        FPaths::MakePlatformFilename(ContentFolder);
        PreviewPath   = InPreview.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(InPreview);
        if (!PreviewPath.IsEmpty()) FPaths::MakePlatformFilename(PreviewPath);
        Title = InTitle; Desc = InDesc;
        if (!SteamUGC() || !SteamUtils()) { if (Cb) Cb(false, TEXT("Steam UGC no disponible")); return; }

        UE_LOG(LogPTWordPacks, Warning, TEXT("[Publish] AppID=%u ContentFolder='%s' existe=%d preview='%s'"),
            SteamUtils()->GetAppID(), *ContentFolder,
            IFileManager::Get().DirectoryExists(*ContentFolder) ? 1 : 0, *PreviewPath);

        const SteamAPICall_t h = SteamUGC()->CreateItem(SteamUtils()->GetAppID(), k_EWorkshopFileTypeCommunity);
        CreateCR.Set(h, this, &FPTWorkshopPublish::OnCreate);
    }

    void OnCreate(CreateItemResult_t* p, bool bIOFailure)
    {
        if (bIOFailure || !p || p->m_eResult != k_EResultOK)
        {
            // Los CallResult de Steam corren en el hilo de tareas del OSS, NO en el GameThread. El Cb
            // toca Slate (la UI del Workshop) → hay que marshalear al GameThread, si no crashea con
            // "Assertion failed: IsInGameThread()". (El OnSubmit ya lo hacía; este camino de error no.)
            const int32 Res = p ? (int32)p->m_eResult : -1;
            TFunction<void(bool, FString)> CB = MoveTemp(Cb);
            AsyncTask(ENamedThreads::GameThread, [CB = MoveTemp(CB), Res]() mutable
                { if (CB) CB(false, FString::Printf(TEXT("CreateItem falló (%d)"), Res)); });
            return;
        }
        ItemId = p->m_nPublishedFileId;

        UGCUpdateHandle_t U = SteamUGC()->StartItemUpdate(SteamUtils()->GetAppID(), ItemId);
        const bool bTitle   = SteamUGC()->SetItemTitle(U, TCHAR_TO_UTF8(*Title));
        const bool bDesc    = Desc.IsEmpty() || SteamUGC()->SetItemDescription(U, TCHAR_TO_UTF8(*Desc));
        const bool bContent = SteamUGC()->SetItemContent(U, TCHAR_TO_UTF8(*ContentFolder));
        const bool bPrev    = PreviewPath.IsEmpty() || SteamUGC()->SetItemPreview(U, TCHAR_TO_UTF8(*PreviewPath));
        // Item público por defecto (algunos flujos lo requieren explícito).
        const bool bVis     = SteamUGC()->SetItemVisibility(U, k_ERemoteStoragePublishedFileVisibilityPublic);
        // Tag para poder filtrar solo bancos de palabras en el buscador del juego (AddRequiredTag).
        const char* Tags[] = { "WordBank" };
        SteamParamStringArray_t TagArr; TagArr.m_ppStrings = Tags; TagArr.m_nNumStrings = 1;
        const bool bTags    = SteamUGC()->SetItemTags(U, &TagArr);

        Diag = FString::Printf(TEXT("U=%d t=%d d=%d c=%d p=%d v=%d tag=%d"),
            U != k_UGCUpdateHandleInvalid ? 1 : 0, bTitle, bDesc, bContent, bPrev, bVis, bTags);
        UE_LOG(LogPTWordPacks, Warning,
            TEXT("[Publish] ItemId=%llu %s content='%s' preview='%s'"),
            (uint64)ItemId, *Diag, *ContentFolder, *PreviewPath);

        const SteamAPICall_t h = SteamUGC()->SubmitItemUpdate(U, "Banco de palabras");
        SubmitCR.Set(h, this, &FPTWorkshopPublish::OnSubmit);
    }

    void OnSubmit(SubmitItemUpdateResult_t* p, bool bIOFailure)
    {
        const bool bOk = !bIOFailure && p && p->m_eResult == k_EResultOK;
        // m_bUserNeedsToAcceptWorkshopLegalAgreement: si es true, Steam le muestra el acuerdo al usuario.
        FString Info = bOk
            ? FString::Printf(TEXT("%llu"), ItemId)
            : FString::Printf(TEXT("SubmitItemUpdate falló (%d) [%s]"),
                              p ? (int32)p->m_eResult : -1, *Diag);
        if (!bOk)
            UE_LOG(LogPTWordPacks, Error, TEXT("[Publish] %s (IOFailure=%d)"), *Info, bIOFailure ? 1 : 0);
        // Los callbacks de Steam corren en el hilo de tareas de OSS, no en el GameThread; la UI que
        // escucha el delegate toca Slate → marshalear al GameThread.
        TFunction<void(bool, FString)> CB = MoveTemp(Cb);
        AsyncTask(ENamedThreads::GameThread, [CB = MoveTemp(CB), bOk, Info]() mutable { if (CB) CB(bOk, Info); });
    }
};

// ===== Watcher de descargas (DownloadItemResult_t) ==========================
// Suscribirse a un item NO garantiza que Steam baje su contenido; hay que llamar
// SteamUGC()->DownloadItem() y esperar este callback. Cuando un item de ESTA app termina de
// descargarse, re-escaneamos para que aparezca en la pestaña de suscritos.
struct FPTWorkshopDownloadWatcher
{
    TWeakObjectPtr<UPTWordPackSubsystem> Owner;
    CCallback<FPTWorkshopDownloadWatcher, DownloadItemResult_t> DownloadedCb;

    explicit FPTWorkshopDownloadWatcher(UPTWordPackSubsystem* InOwner)
        : Owner(InOwner)
        , DownloadedCb(this, &FPTWorkshopDownloadWatcher::OnDownloaded) {}

    void OnDownloaded(DownloadItemResult_t* p)
    {
        if (!p) return;
        if (SteamUtils() && p->m_unAppID != SteamUtils()->GetAppID()) return; // otra app
        UE_LOG(LogPTWordPacks, Log, TEXT("[Workshop] Descarga terminada item=%llu res=%d → rescan"),
            (uint64)p->m_nPublishedFileId, (int32)p->m_eResult);
        // El callback puede correr fuera del GameThread → marshalear (RescanPacks toca UPROPERTY + UI).
        TWeakObjectPtr<UPTWordPackSubsystem> W = Owner;
        AsyncTask(ENamedThreads::GameThread, [W]() { if (UPTWordPackSubsystem* O = W.Get()) O->RescanPacks(); });
    }
};

// ===== Búsqueda del catálogo del Workshop ===================================
struct FPTWorkshopQuery
{
    TFunction<void(TArray<FPTWorkshopItem>&&, bool)> Cb;
    UGCQueryHandle_t Handle = k_UGCQueryHandleInvalid;
    CCallResult<FPTWorkshopQuery, SteamUGCQueryCompleted_t> QueryCR;

    void Start(const FString& SearchText, const FString& Tag,
               TFunction<void(TArray<FPTWorkshopItem>&&, bool)> InCb)
    {
        Cb = MoveTemp(InCb);
        if (!SteamUGC() || !SteamUtils()) { if (Cb) Cb({}, false); return; }

        const AppId_t App = SteamUtils()->GetAppID();
        // Si hay texto de búsqueda, ordenar por relevancia; si no, por votos (populares primero).
        const EUGCQuery QueryType = SearchText.IsEmpty()
            ? k_EUGCQuery_RankedByVote : k_EUGCQuery_RankedByTextSearch;
        Handle = SteamUGC()->CreateQueryAllUGCRequest(QueryType, k_EUGCMatchingUGCType_Items, App, App, 1);
        if (!SearchText.IsEmpty()) SteamUGC()->SetSearchText(Handle, TCHAR_TO_UTF8(*SearchText));
        if (!Tag.IsEmpty())        SteamUGC()->AddRequiredTag(Handle, TCHAR_TO_UTF8(*Tag));
        // Que el resultado traiga la descripción completa (si no, m_rgchDescription viene vacío/cortado).
        SteamUGC()->SetReturnLongDescription(Handle, true);

        const SteamAPICall_t h = SteamUGC()->SendQueryUGCRequest(Handle);
        QueryCR.Set(h, this, &FPTWorkshopQuery::OnComplete);
    }

    void OnComplete(SteamUGCQueryCompleted_t* p, bool bIOFailure)
    {
        TArray<FPTWorkshopItem> Out;
        const bool bOk = !bIOFailure && p && p->m_eResult == k_EResultOK;
        if (bOk && SteamUGC())
        {
            for (uint32 i = 0; i < p->m_unNumResultsReturned; ++i)
            {
                SteamUGCDetails_t D;
                if (SteamUGC()->GetQueryUGCResult(Handle, i, &D))
                {
                    FPTWorkshopItem It;
                    It.Id          = FString::Printf(TEXT("%llu"), D.m_nPublishedFileId);
                    It.Title       = UTF8_TO_TCHAR(D.m_rgchTitle);
                    It.Description = UTF8_TO_TCHAR(D.m_rgchDescription);
                    It.Tags        = UTF8_TO_TCHAR(D.m_rgchTags);
                    // URL de la miniatura (se baja por HTTP en la fila del browser).
                    char PrevUrl[1024] = { 0 };
                    if (SteamUGC()->GetQueryUGCPreviewURL(Handle, i, PrevUrl, sizeof(PrevUrl)))
                        It.PreviewURL = UTF8_TO_TCHAR(PrevUrl);
                    const uint32 St = SteamUGC()->GetItemState(D.m_nPublishedFileId);
                    It.bSubscribed = (St & k_EItemStateSubscribed) != 0;
                    Out.Add(MoveTemp(It));
                }
            }
        }
        if (SteamUGC() && Handle != k_UGCQueryHandleInvalid)
            SteamUGC()->ReleaseQueryUGCRequest(Handle);

        TFunction<void(TArray<FPTWorkshopItem>&&, bool)> CB = MoveTemp(Cb);
        AsyncTask(ENamedThreads::GameThread,
            [CB = MoveTemp(CB), Out = MoveTemp(Out), bOk]() mutable { if (CB) CB(MoveTemp(Out), bOk); });
    }
};

// ===== Detalles de items concretos (por id) — para los suscritos de la Biblioteca ==============
// Los packs suscritos se arman de la carpeta descargada (sin descripción/preview/tags). Esta query
// (CreateQueryUGCDetailsRequest por ids) trae esos metadatos para que la lista de suscritos se vea
// IGUAL que el browser. Devuelve id → FPTWorkshopItem.
struct FPTWorkshopDetailsQuery
{
    TFunction<void(const TMap<uint64, FPTWorkshopItem>&)> Cb;
    UGCQueryHandle_t Handle = k_UGCQueryHandleInvalid;
    CCallResult<FPTWorkshopDetailsQuery, SteamUGCQueryCompleted_t> QueryCR;

    void Start(const TArray<PublishedFileId_t>& Ids, TFunction<void(const TMap<uint64, FPTWorkshopItem>&)> InCb)
    {
        Cb = MoveTemp(InCb);
        if (!SteamUGC() || Ids.Num() == 0) { if (Cb) Cb({}); return; }
        Handle = SteamUGC()->CreateQueryUGCDetailsRequest(const_cast<PublishedFileId_t*>(Ids.GetData()), Ids.Num());
        SteamUGC()->SetReturnLongDescription(Handle, true);
        const SteamAPICall_t h = SteamUGC()->SendQueryUGCRequest(Handle);
        QueryCR.Set(h, this, &FPTWorkshopDetailsQuery::OnComplete);
    }

    void OnComplete(SteamUGCQueryCompleted_t* p, bool bIOFailure)
    {
        TMap<uint64, FPTWorkshopItem> Out;
        const bool bOk = !bIOFailure && p && p->m_eResult == k_EResultOK;
        if (bOk && SteamUGC())
        {
            for (uint32 i = 0; i < p->m_unNumResultsReturned; ++i)
            {
                SteamUGCDetails_t D;
                if (SteamUGC()->GetQueryUGCResult(Handle, i, &D))
                {
                    FPTWorkshopItem It;
                    It.Id          = FString::Printf(TEXT("%llu"), D.m_nPublishedFileId);
                    It.Title       = UTF8_TO_TCHAR(D.m_rgchTitle);
                    It.Description = UTF8_TO_TCHAR(D.m_rgchDescription);
                    It.Tags        = UTF8_TO_TCHAR(D.m_rgchTags);
                    char PrevUrl[1024] = { 0 };
                    if (SteamUGC()->GetQueryUGCPreviewURL(Handle, i, PrevUrl, sizeof(PrevUrl)))
                        It.PreviewURL = UTF8_TO_TCHAR(PrevUrl);
                    Out.Add((uint64)D.m_nPublishedFileId, MoveTemp(It));
                }
            }
        }
        if (SteamUGC() && Handle != k_UGCQueryHandleInvalid)
            SteamUGC()->ReleaseQueryUGCRequest(Handle);

        TFunction<void(const TMap<uint64, FPTWorkshopItem>&)> CB = MoveTemp(Cb);
        AsyncTask(ENamedThreads::GameThread,
            [CB = MoveTemp(CB), Out = MoveTemp(Out)]() mutable { if (CB) CB(Out); });
    }
};
#endif // PT_WITH_STEAM

// ==========================================================================
// Subsystem
// ==========================================================================

void UPTWordPackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if PT_WITH_STEAM
    if (SteamUGC()) DownloadWatcher = new FPTWorkshopDownloadWatcher(this);
#endif
    RescanPacks();
}

void UPTWordPackSubsystem::Deinitialize()
{
#if PT_WITH_STEAM
    delete Publisher;       Publisher       = nullptr;
    delete Query;           Query           = nullptr;
    delete Details;         Details         = nullptr;
    delete DownloadWatcher; DownloadWatcher = nullptr;
#endif
    Super::Deinitialize();
}

void UPTWordPackSubsystem::SearchWorkshop(const FString& SearchText, const FString& Tag)
{
#if PT_WITH_STEAM
    if (!SteamUGC())
    {
        OnWorkshopSearchComplete.Broadcast(TArray<FPTWorkshopItem>(), false);
        return;
    }
    delete Query;
    Query = new FPTWorkshopQuery();
    Query->Start(SearchText, Tag, [this](TArray<FPTWorkshopItem>&& Items, bool bOk)
    {
        delete Query; Query = nullptr;
        OnWorkshopSearchComplete.Broadcast(Items, bOk);
    });
#else
    OnWorkshopSearchComplete.Broadcast(TArray<FPTWorkshopItem>(), false);
#endif
}

void UPTWordPackSubsystem::SubscribeItem(const FString& Id)
{
#if PT_WITH_STEAM
    if (!SteamUGC()) return;
    const uint64 FileId = FCString::Strtoui64(*Id, nullptr, 10);
    if (FileId != 0)
    {
        SteamUGC()->SubscribeItem(FileId);
        // Suscribirse NO baja el contenido solo → forzamos la descarga (alta prioridad). Cuando
        // termina, el DownloadItemResult_t re-escanea y el banco aparece en la pestaña de suscritos.
        SteamUGC()->DownloadItem(FileId, /*bHighPriority=*/true);
    }
#endif
}

void UPTWordPackSubsystem::UnsubscribeItem(const FString& Id)
{
#if PT_WITH_STEAM
    if (!SteamUGC()) return;
    const uint64 FileId = FCString::Strtoui64(*Id, nullptr, 10);
    if (FileId != 0) SteamUGC()->UnsubscribeItem(FileId);
#endif
}

const FPTWordPack* UPTWordPackSubsystem::FindPack(const FString& Id) const
{
    return Packs.FindByPredicate([&Id](const FPTWordPack& P) { return P.Id == Id; });
}

void UPTWordPackSubsystem::RescanPacks()
{
    Packs.Reset();
    ScanLocalPacks();
    ScanWorkshopPacks();
    UE_LOG(LogPTWordPacks, Log, TEXT("RescanPacks: %d banco(s) de palabras."), Packs.Num());
    OnWordPacksUpdated.Broadcast();
    // Traer descripción/preview/tags de los packs del Workshop (async → re-broadcast al llegar).
    FetchWorkshopDetails();
}

void UPTWordPackSubsystem::FetchWorkshopDetails()
{
#if PT_WITH_STEAM
    if (!SteamUGC()) return;
    // Ids de los packs del Workshop a los que aún les falta el preview (evita re-pedir lo ya traído).
    TArray<PublishedFileId_t> Ids;
    for (const FPTWordPack& P : Packs)
        if (P.bFromWorkshop && P.PreviewURL.IsEmpty())
        {
            const uint64 Fid = FCString::Strtoui64(*P.Id, nullptr, 10);
            if (Fid != 0) Ids.Add((PublishedFileId_t)Fid);
        }
    if (Ids.Num() == 0) return;

    delete Details;
    Details = new FPTWorkshopDetailsQuery();
    Details->Start(Ids, [this](const TMap<uint64, FPTWorkshopItem>& M)
    {
        delete Details; Details = nullptr;
        bool bAny = false;
        for (FPTWordPack& P : Packs)
        {
            if (!P.bFromWorkshop) continue;
            const uint64 Fid = FCString::Strtoui64(*P.Id, nullptr, 10);
            if (const FPTWorkshopItem* It = M.Find(Fid))
            {
                P.Description = It->Description;
                P.PreviewURL  = It->PreviewURL;
                P.Tags        = It->Tags;
                bAny = true;
            }
        }
        if (bAny) OnWordPacksUpdated.Broadcast(); // refresca la lista con miniatura/desc/tag
    });
#endif
}

void UPTWordPackSubsystem::AddPackFromFolder(const FString& Folder, const FString& Id, bool bWorkshop)
{
    const FString CsvPath = FPaths::Combine(Folder, TEXT("words.csv"));
    if (!FPaths::FileExists(CsvPath)) return; // sin words.csv no es un banco válido

    FPTWordPack Pack;
    Pack.Id           = Id;
    Pack.CsvPath      = CsvPath;
    Pack.bFromWorkshop = bWorkshop;

    // Título/autor: pack.txt (línea 1 = título, línea 2 = autor). Fallback: nombre de la carpeta.
    const FString PackTxt = FPaths::Combine(Folder, TEXT("pack.txt"));
    TArray<FString> Lines;
    if (FFileHelper::LoadFileToStringArray(Lines, *PackTxt))
    {
        if (Lines.Num() > 0) Pack.Title  = Lines[0].TrimStartAndEnd();
        if (Lines.Num() > 1) Pack.Author = Lines[1].TrimStartAndEnd();
    }
    if (Pack.Title.IsEmpty()) Pack.Title = FPaths::GetCleanFilename(Folder);

    Packs.Add(MoveTemp(Pack));
}

void UPTWordPackSubsystem::ScanLocalPacks()
{
    // <ProjectDir>/WordPacks/*  (para testear sin Steam)
    const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("WordPacks"));
    if (!FPaths::DirectoryExists(Root)) return;

    TArray<FString> SubDirs;
    IFileManager::Get().FindFiles(SubDirs, *(Root / TEXT("*")), /*Files=*/false, /*Directories=*/true);
    for (const FString& Name : SubDirs)
    {
        if (Name == TEXT(".") || Name == TEXT("..")) continue;
        AddPackFromFolder(FPaths::Combine(Root, Name), TEXT("local:") + Name, /*bWorkshop=*/false);
    }
}

void UPTWordPackSubsystem::ScanWorkshopPacks()
{
#if PT_WITH_STEAM
    if (!SteamUGC()) return;

    const uint32 Num = SteamUGC()->GetNumSubscribedItems();
    if (Num == 0) return;

    TArray<PublishedFileId_t> Ids;
    Ids.SetNumZeroed(Num);
    const uint32 Got = SteamUGC()->GetSubscribedItems(Ids.GetData(), Num);

    int32 Pending = 0;
    for (uint32 i = 0; i < Got; ++i)
    {
        const PublishedFileId_t Id = Ids[i];
        const uint32 State = SteamUGC()->GetItemState(Id);

        // Si está suscrito pero NO instalado (o necesita update), disparamos la descarga. Cuando
        // termine, el DownloadItemResult_t vuelve a llamar RescanPacks y acá ya entrará como instalado.
        if (!(State & k_EItemStateInstalled) || (State & k_EItemStateNeedsUpdate))
        {
            SteamUGC()->DownloadItem(Id, /*bHighPriority=*/true);
            ++Pending;
            if (!(State & k_EItemStateInstalled)) continue; // aún sin carpeta en disco
        }

        uint64 SizeOnDisk = 0; uint32 Timestamp = 0;
        char FolderBuf[2048] = { 0 };
        if (SteamUGC()->GetItemInstallInfo(Id, &SizeOnDisk, FolderBuf, sizeof(FolderBuf), &Timestamp))
        {
            const FString Folder = UTF8_TO_TCHAR(FolderBuf);
            AddPackFromFolder(Folder, FString::Printf(TEXT("%llu"), Id), /*bWorkshop=*/true);
        }
    }
    if (Pending > 0)
        UE_LOG(LogPTWordPacks, Log, TEXT("[Workshop] %d item(s) suscritos descargándose; aparecerán al terminar."), Pending);
#endif
}

// ── Procesado de la imagen de preview (foto elegida por el jugador) ───────────────────────────────
// Steam exige preview VÁLIDO y < 1 MB, si no rebota con InvalidParam(8). Una foto grande cruda no
// entra, así que la achicamos (box-filter) a máx 512px y la re-encodeamos a PNG en %TEMP%. Reintenta
// a 256 si el PNG saliera grande. Sin dependencias externas (resize manual) para no romper el build.
namespace
{
    // "titulos_peliculas" → "Titulos Peliculas" (título por defecto si el jugador no escribió uno).
    FString PT_PrettifyName(const FString& In)
    {
        FString S = In;
        S.ReplaceInline(TEXT("_"), TEXT(" "));
        S.ReplaceInline(TEXT("-"), TEXT(" "));
        S.TrimStartAndEndInline();
        bool bStart = true;
        for (int32 i = 0; i < S.Len(); ++i)
        {
            if (FChar::IsWhitespace(S[i])) { bStart = true; continue; }
            if (bStart) { S[i] = FChar::ToUpper(S[i]); bStart = false; }
        }
        return S;
    }

    void PT_DownscaleBGRA(const TArray<FColor>& Src, int32 SW, int32 SH,
                          TArray<FColor>& Dst, int32 DW, int32 DH)
    {
        Dst.SetNumUninitialized(DW * DH);
        for (int32 y = 0; y < DH; ++y)
            for (int32 x = 0; x < DW; ++x)
            {
                const int32 sx0 = (x * SW) / DW;
                const int32 sy0 = (y * SH) / DH;
                int32 sx1 = ((x + 1) * SW) / DW; if (sx1 <= sx0) sx1 = sx0 + 1; sx1 = FMath::Min(sx1, SW);
                int32 sy1 = ((y + 1) * SH) / DH; if (sy1 <= sy0) sy1 = sy0 + 1; sy1 = FMath::Min(sy1, SH);
                uint32 r = 0, g = 0, b = 0, a = 0, n = 0;
                for (int32 sy = sy0; sy < sy1; ++sy)
                    for (int32 sx = sx0; sx < sx1; ++sx)
                    { const FColor& c = Src[sy * SW + sx]; r += c.R; g += c.G; b += c.B; a += c.A; ++n; }
                if (n == 0) n = 1;
                Dst[y * DW + x] = FColor((uint8)(r / n), (uint8)(g / n), (uint8)(b / n), (uint8)(a / n));
            }
    }

    bool PT_ProcessPreviewImage(const FString& InPath, const FString& OutDir, FString& OutFile)
    {
        TArray<uint8> Raw;
        if (!FFileHelper::LoadFileToArray(Raw, *InPath) || Raw.Num() == 0) return false;

        IImageWrapperModule& IW = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        const EImageFormat Fmt = IW.DetectImageFormat(Raw.GetData(), Raw.Num());
        if (Fmt == EImageFormat::Invalid) return false;
        TSharedPtr<IImageWrapper> Dec = IW.CreateImageWrapper(Fmt);
        if (!Dec.IsValid() || !Dec->SetCompressed(Raw.GetData(), Raw.Num())) return false;

        TArray64<uint8> RawBGRA;
        if (!Dec->GetRaw(ERGBFormat::BGRA, 8, RawBGRA)) return false;
        const int32 SW = Dec->GetWidth();
        const int32 SH = Dec->GetHeight();
        if (SW <= 0 || SH <= 0 || RawBGRA.Num() < (int64)SW * SH * 4) return false;

        TArray<FColor> Src; Src.SetNumUninitialized(SW * SH);
        FMemory::Memcpy(Src.GetData(), RawBGRA.GetData(), (SIZE_T)SW * SH * sizeof(FColor));

        auto EncodeAt = [&](int32 MaxDim) -> bool
        {
            int32 DW = SW, DH = SH;
            const int32 MaxSide = FMath::Max(SW, SH);
            if (MaxSide > MaxDim)
            {
                const float S = (float)MaxDim / (float)MaxSide;
                DW = FMath::Max(1, FMath::RoundToInt(SW * S));
                DH = FMath::Max(1, FMath::RoundToInt(SH * S));
            }
            TArray<FColor> Dst;
            if (DW == SW && DH == SH) Dst = Src; else PT_DownscaleBGRA(Src, SW, SH, Dst, DW, DH);

            TSharedPtr<IImageWrapper> Enc = IW.CreateImageWrapper(EImageFormat::PNG);
            if (!Enc.IsValid() ||
                !Enc->SetRaw(Dst.GetData(), (int64)Dst.Num() * sizeof(FColor), DW, DH, ERGBFormat::BGRA, 8))
                return false;
            const TArray64<uint8>& Png = Enc->GetCompressed(100);
            if (Png.Num() == 0 || Png.Num() > 950 * 1024) return false; // grande: probar más chico
            const FString Path = FPaths::Combine(OutDir, TEXT("preview.png"));
            if (!FFileHelper::SaveArrayToFile(TArray<uint8>(Png), *Path)) return false;
            OutFile = Path;
            return true;
        };

        return EncodeAt(512) || EncodeAt(256);
    }
}

void UPTWordPackSubsystem::PublishWordPack(const FString& CsvPath, const FString& Title,
                                           const FString& Description, const FString& PreviewPath)
{
#if PT_WITH_STEAM
    if (!SteamUGC() || !SteamUtils())
    {
        OnWordPackPublished.Broadcast(false, TEXT("Steam no disponible"));
        return;
    }
    if (!FPaths::FileExists(CsvPath))
    {
        OnWordPackPublished.Broadcast(false, TEXT("No existe el CSV"));
        return;
    }

    // ISteamUGC sube una CARPETA, no un archivo suelto. Estructura del staging (en %TEMP%, siempre
    // escribible — NO bajo ProjectSavedDir(), que si el juego está en Program Files no es escribible):
    //   <base>/content/   → words.csv + pack.txt   → SetItemContent (ruta ABSOLUTA + nativa)
    //   <base>/preview.png → imagen de preview FUERA de content (Steam la quiere separada; adentro da (8))
    FString Base = FPaths::Combine(FString(FPlatformProcess::UserTempDir()),
                                   TEXT("SculpturilloWorkshop"),
                                   FGuid::NewGuid().ToString(EGuidFormats::Short));
    Base = FPaths::ConvertRelativePathToFull(Base);
    FPaths::MakePlatformFilename(Base);
    FString Content = FPaths::Combine(Base, TEXT("content"));
    FPaths::MakePlatformFilename(Content);

    IFileManager& FM = IFileManager::Get();
    const bool bDir  = FM.MakeDirectory(*Content, /*Tree=*/true);
    const bool bCopy = (FM.Copy(*FPaths::Combine(Content, TEXT("words.csv")), *CsvPath) == COPY_OK);

    // pack.txt con título/autor para que se muestre bien al que lo descargue.
    const FString PackTxt = Title + LINE_TERMINATOR;
    const bool bTxt = FFileHelper::SaveStringToFile(PackTxt, *FPaths::Combine(Content, TEXT("pack.txt")));

    // Preview (foto grande + miniatura): Steam RECHAZA el primer SubmitItemUpdate sin imagen válida
    // (InvalidParam=8) y exige < 1 MB. Prioridad:
    //   1) la foto que eligió el jugador → se achica a ≤512 y se re-encoda a PNG (garantiza <1MB);
    //   2) imagen branded por defecto (opcional): Content/UI/Workshop/DefaultWorkshopPreview.png;
    //   3) último recurso: PNG sólido de relleno (dimensiones válidas), FUERA de content.
    FString UsePreview;
    if (!PreviewPath.IsEmpty())
    {
        FString Processed;
        if (PT_ProcessPreviewImage(PreviewPath, Base, Processed)) UsePreview = Processed;
    }
    if (UsePreview.IsEmpty())
    {
        const FString Branded = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("UI/Workshop/DefaultWordBankThumbnail.png"));
        if (FPaths::FileExists(Branded))
        {
            FString Processed;
            if (PT_ProcessPreviewImage(Branded, Base, Processed)) UsePreview = Processed;
        }
    }
    if (UsePreview.IsEmpty())
    {
        const int32 Sz = 256;
        TArray<FColor> Pixels; Pixels.Init(FColor(150, 90, 200, 255), Sz * Sz);
        IImageWrapperModule& IW = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
        TSharedPtr<IImageWrapper> Wrapper = IW.CreateImageWrapper(EImageFormat::PNG);
        if (Wrapper.IsValid() &&
            Wrapper->SetRaw(Pixels.GetData(), Pixels.Num() * sizeof(FColor), Sz, Sz, ERGBFormat::BGRA, 8))
        {
            const TArray64<uint8>& Png = Wrapper->GetCompressed(100);
            const FString PrevFile = FPaths::Combine(Base, TEXT("preview.png"));
            if (FFileHelper::SaveArrayToFile(TArray<uint8>(Png), *PrevFile)) UsePreview = PrevFile;
        }
    }
    if (!UsePreview.IsEmpty()) FPaths::MakePlatformFilename(UsePreview);

    UE_LOG(LogPTWordPacks, Warning, TEXT("[Publish] Content='%s' dir=%d copy=%d txt=%d preview='%s'"),
        *Content, bDir ? 1 : 0, bCopy ? 1 : 0, bTxt ? 1 : 0, *UsePreview);
    if (!bCopy)
    {
        OnWordPackPublished.Broadcast(false, FString::Printf(TEXT("No se pudo escribir el staging: %s"), *Content));
        return;
    }

    // Título: el que pasó el caller; si va vacío, se deriva del nombre del CSV prettificado.
    FString EffectiveTitle = Title;
    EffectiveTitle.TrimStartAndEndInline();
    if (EffectiveTitle.IsEmpty()) EffectiveTitle = PT_PrettifyName(FPaths::GetBaseFilename(CsvPath));

    delete Publisher;
    Publisher = new FPTWorkshopPublish();
    Publisher->Start(Content, UsePreview, EffectiveTitle, Description,
        [this](bool bOk, FString Info)
        {
            UE_LOG(LogPTWordPacks, Log, TEXT("PublishWordPack: %s (%s)"),
                bOk ? TEXT("OK") : TEXT("FALLÓ"), *Info);
            OnWordPackPublished.Broadcast(bOk, Info);
            if (bOk) RescanPacks();
        });
#else
    OnWordPackPublished.Broadcast(false, TEXT("Steamworks no disponible en esta plataforma"));
#endif
}
