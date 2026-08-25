// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWordPackSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Async/Async.h"

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
    FString ContentFolder, PreviewPath, Title, Desc;
    PublishedFileId_t ItemId = 0;
    CCallResult<FPTWorkshopPublish, CreateItemResult_t>       CreateCR;
    CCallResult<FPTWorkshopPublish, SubmitItemUpdateResult_t> SubmitCR;

    void Start(const FString& InFolder, const FString& InPreview, const FString& InTitle,
               const FString& InDesc, TFunction<void(bool, FString)> InCb)
    {
        Cb = MoveTemp(InCb);
        // SetItemContent/SetItemPreview exigen rutas ABSOLUTAS (si no, Steam devuelve InvalidParam=8).
        ContentFolder = FPaths::ConvertRelativePathToFull(InFolder);
        PreviewPath   = InPreview.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(InPreview);
        Title = InTitle; Desc = InDesc;
        if (!SteamUGC() || !SteamUtils()) { if (Cb) Cb(false, TEXT("Steam UGC no disponible")); return; }

        const SteamAPICall_t h = SteamUGC()->CreateItem(SteamUtils()->GetAppID(), k_EWorkshopFileTypeCommunity);
        CreateCR.Set(h, this, &FPTWorkshopPublish::OnCreate);
    }

    void OnCreate(CreateItemResult_t* p, bool bIOFailure)
    {
        if (bIOFailure || !p || p->m_eResult != k_EResultOK)
        {
            if (Cb) Cb(false, FString::Printf(TEXT("CreateItem falló (%d)"), p ? (int32)p->m_eResult : -1));
            return;
        }
        ItemId = p->m_nPublishedFileId;

        UGCUpdateHandle_t U = SteamUGC()->StartItemUpdate(SteamUtils()->GetAppID(), ItemId);
        SteamUGC()->SetItemTitle(U, TCHAR_TO_UTF8(*Title));
        if (!Desc.IsEmpty())        SteamUGC()->SetItemDescription(U, TCHAR_TO_UTF8(*Desc));
        SteamUGC()->SetItemContent(U, TCHAR_TO_UTF8(*ContentFolder));
        if (!PreviewPath.IsEmpty()) SteamUGC()->SetItemPreview(U, TCHAR_TO_UTF8(*PreviewPath));
        // Tag para poder filtrar solo bancos de palabras más adelante.
        const char* Tags[] = { "WordBank" };
        SteamParamStringArray_t TagArr; TagArr.m_ppStrings = Tags; TagArr.m_nNumStrings = 1;
        SteamUGC()->SetItemTags(U, &TagArr);

        const SteamAPICall_t h = SteamUGC()->SubmitItemUpdate(U, "Banco de palabras");
        SubmitCR.Set(h, this, &FPTWorkshopPublish::OnSubmit);
    }

    void OnSubmit(SubmitItemUpdateResult_t* p, bool bIOFailure)
    {
        const bool bOk = !bIOFailure && p && p->m_eResult == k_EResultOK;
        // m_bUserNeedsToAcceptWorkshopLegalAgreement: si es true, Steam le muestra el acuerdo al usuario.
        FString Info = bOk
            ? FString::Printf(TEXT("%llu"), ItemId)
            : FString::Printf(TEXT("SubmitItemUpdate falló (%d)"), p ? (int32)p->m_eResult : -1);
        // Los callbacks de Steam corren en el hilo de tareas de OSS, no en el GameThread; la UI que
        // escucha el delegate toca Slate → marshalear al GameThread.
        TFunction<void(bool, FString)> CB = MoveTemp(Cb);
        AsyncTask(ENamedThreads::GameThread, [CB = MoveTemp(CB), bOk, Info]() mutable { if (CB) CB(bOk, Info); });
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
                    It.Id    = FString::Printf(TEXT("%llu"), D.m_nPublishedFileId);
                    It.Title = UTF8_TO_TCHAR(D.m_rgchTitle);
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
#endif // PT_WITH_STEAM

// ==========================================================================
// Subsystem
// ==========================================================================

void UPTWordPackSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RescanPacks();
}

void UPTWordPackSubsystem::Deinitialize()
{
#if PT_WITH_STEAM
    delete Publisher; Publisher = nullptr;
    delete Query;     Query     = nullptr;
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
    if (FileId != 0) SteamUGC()->SubscribeItem(FileId); // Steam descarga el item; luego aparece en GetPacks
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

    for (uint32 i = 0; i < Got; ++i)
    {
        const PublishedFileId_t Id = Ids[i];
        const uint32 State = SteamUGC()->GetItemState(Id);
        if (!(State & k_EItemStateInstalled)) continue; // todavía descargando o sin instalar

        uint64 SizeOnDisk = 0; uint32 Timestamp = 0;
        char FolderBuf[2048] = { 0 };
        if (SteamUGC()->GetItemInstallInfo(Id, &SizeOnDisk, FolderBuf, sizeof(FolderBuf), &Timestamp))
        {
            const FString Folder = UTF8_TO_TCHAR(FolderBuf);
            AddPackFromFolder(Folder, FString::Printf(TEXT("%llu"), Id), /*bWorkshop=*/true);
        }
    }
#endif
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

    // ISteamUGC sube una CARPETA, no un archivo suelto: armamos un staging con words.csv (+ pack.txt).
    // OJO: SetItemContent exige ruta ABSOLUTA. En build empaquetada ProjectSavedDir() es relativa
    // (../../../…) → Steam la rechaza con InvalidParam (8). Por eso la convertimos a full path.
    const FString Staging = FPaths::ConvertRelativePathToFull(
        FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WorkshopStaging"),
                        FGuid::NewGuid().ToString(EGuidFormats::Short)));
    IFileManager& FM = IFileManager::Get();
    FM.MakeDirectory(*Staging, /*Tree=*/true);
    FM.Copy(*FPaths::Combine(Staging, TEXT("words.csv")), *CsvPath);

    // pack.txt con título/autor para que se muestre bien al que lo descargue.
    const FString PackTxt = Title + LINE_TERMINATOR;
    FFileHelper::SaveStringToFile(PackTxt, *FPaths::Combine(Staging, TEXT("pack.txt")));

    delete Publisher;
    Publisher = new FPTWorkshopPublish();
    Publisher->Start(Staging, PreviewPath, Title, Description,
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
