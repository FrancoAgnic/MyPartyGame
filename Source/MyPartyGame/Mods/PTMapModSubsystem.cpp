// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMapModSubsystem.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Misc/CoreDelegates.h"        // FCoreDelegates::MountPak
#include "IPlatformFilePak.h"          // IPakFile
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Async/Async.h"
#include "UObject/WeakObjectPtr.h"

#if PT_WITH_STEAM
#include "steam/steam_api.h"
#include "steam/isteamugc.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPTMapMods, Log, All);

#if PT_WITH_STEAM
// Watcher de descargas: suscribirse NO baja el contenido; hay que DownloadItem y esperar este callback.
// Al terminar la descarga de un item de ESTA app, re-escaneamos para que el mapa aparezca montable.
struct FPTMapDownloadWatcher
{
    TWeakObjectPtr<UPTMapModSubsystem> Owner;
    CCallback<FPTMapDownloadWatcher, DownloadItemResult_t> DownloadedCb;
    explicit FPTMapDownloadWatcher(UPTMapModSubsystem* InOwner)
        : Owner(InOwner), DownloadedCb(this, &FPTMapDownloadWatcher::OnDownloaded) {}
    void OnDownloaded(DownloadItemResult_t* p)
    {
        if (!p) return;
        if (SteamUtils() && p->m_unAppID != SteamUtils()->GetAppID()) return;
        const uint64 Fid = (uint64)p->m_nPublishedFileId;
        const bool bOk = (p->m_eResult == k_EResultOK);
        UE_LOG(LogPTMapMods, Log, TEXT("[Workshop] Descarga terminada item=%llu res=%d%s"),
            Fid, (int32)p->m_eResult, bOk ? TEXT(" → rescan") : TEXT(" (fallo, ignorado)"));
        // Solo re-escanear si bajó BIEN. Si falló (FileNotFound de un item sin contenido) → ignorar, para no
        // disparar el bucle de descargas + re-broadcast.
        if (!bOk) return;
        TWeakObjectPtr<UPTMapModSubsystem> W = Owner;
        AsyncTask(ENamedThreads::GameThread, [W, Fid]()
        {
            if (UPTMapModSubsystem* O = W.Get()) { O->NotifyDownloadFinished(Fid); O->RescanMods(); }
        });
    }
};
#endif

void UPTMapModSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
#if PT_WITH_STEAM
    DownloadWatcher = new FPTMapDownloadWatcher(this);
#endif
    RescanMods();
}

void UPTMapModSubsystem::Deinitialize()
{
#if PT_WITH_STEAM
    if (DownloadWatcher) { delete DownloadWatcher; DownloadWatcher = nullptr; }
#endif
    Super::Deinitialize();
}

const FPTMapMod* UPTMapModSubsystem::FindMod(const FString& Id) const
{
    return Mods.FindByPredicate([&Id](const FPTMapMod& M){ return M.Id == Id; });
}

void UPTMapModSubsystem::RescanMods()
{
    Mods.Reset();
    ScanLocalMapMods();
    ScanCacheMapMods();
    ScanWorkshopMaps();
    UE_LOG(LogPTMapMods, Log, TEXT("RescanMods: %d mapa(s)-mod."), Mods.Num());
    OnMapModsUpdated.Broadcast();
}

void UPTMapModSubsystem::ScanWorkshopMaps()
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

        // Suscrito pero sin instalar (o con update pendiente) → disparar descarga UNA vez por item (guard, para
        // no re-pedir en cada rescan y entrar en bucle). Al terminar bien, el watcher rescanea.
        if (!(State & k_EItemStateInstalled) || (State & k_EItemStateNeedsUpdate))
        {
            if (!RequestedDownloads.Contains((uint64)Id))
            {
                RequestedDownloads.Add((uint64)Id);
                SteamUGC()->DownloadItem(Id, /*bHighPriority=*/true);
                ++Pending;
            }
            if (!(State & k_EItemStateInstalled)) continue; // aún sin carpeta en disco
        }

        uint64 SizeOnDisk = 0; uint32 Timestamp = 0;
        char FolderBuf[2048] = { 0 };
        if (SteamUGC()->GetItemInstallInfo(Id, &SizeOnDisk, FolderBuf, sizeof(FolderBuf), &Timestamp))
        {
            // AddModFromFolder exige map.pak+mod.json → los items que son BANCOS (words.csv) se ignoran solos.
            const FString Folder = UTF8_TO_TCHAR(FolderBuf);
            AddModFromFolder(Folder, FString::Printf(TEXT("%llu"), Id), /*bWorkshop=*/true);
        }
    }
    if (Pending > 0)
        UE_LOG(LogPTMapMods, Log, TEXT("[Workshop] %d item(s) suscritos descargándose; aparecerán al terminar."), Pending);
#endif
}

void UPTMapModSubsystem::ScanLocalMapMods()
{
    // <ProjectDir>/MapMods/*  (para testear sin Steam, con un .pak cocinado a mano)
    const FString Root = FPaths::Combine(FPaths::ProjectDir(), TEXT("MapMods"));
    if (!FPaths::DirectoryExists(Root)) return;

    TArray<FString> SubDirs;
    IFileManager::Get().FindFiles(SubDirs, *(Root / TEXT("*")), /*Files=*/false, /*Directories=*/true);
    SubDirs.Sort();
    for (const FString& Name : SubDirs)
    {
        if (Name == TEXT(".") || Name == TEXT("..")) continue;
        AddModFromFolder(FPaths::Combine(Root, Name), TEXT("local:") + Name, /*bWorkshop=*/false);
    }
}

FString UPTMapModSubsystem::CacheRootDir()
{
    return FPaths::Combine(FPaths::ProjectDir(), TEXT("MapModsCache"));
}

FString UPTMapModSubsystem::SanitizeIdForFolder(const FString& Id)
{
    FString S = Id;
    S.ReplaceInline(TEXT(":"), TEXT("_"));
    S.ReplaceInline(TEXT("/"), TEXT("_"));
    S.ReplaceInline(TEXT("\\"), TEXT("_"));
    return S;
}

void UPTMapModSubsystem::ScanCacheMapMods()
{
    // Mapas de props que el host nos transfirió por chunks (guardados en MapModsCache/<id>/).
    const FString Root = CacheRootDir();
    if (!FPaths::DirectoryExists(Root)) return;

    TArray<FString> SubDirs;
    IFileManager::Get().FindFiles(SubDirs, *(Root / TEXT("*")), /*Files=*/false, /*Directories=*/true);
    SubDirs.Sort();
    for (const FString& Name : SubDirs)
    {
        if (Name == TEXT(".") || Name == TEXT("..")) continue;
        // El Id real viene del "Id" del mod.json (AddModFromFolder lo respeta); el nombre de carpeta es
        // solo el id saneado, así que pasamos ese como fallback.
        AddModFromFolder(FPaths::Combine(Root, Name), Name, /*bWorkshop=*/false);
    }
}

bool UPTMapModSubsystem::HasModContent(const FString& Id) const
{
    const FPTMapMod* Mod = FindMod(Id);
    if (!Mod) return false;
    if (Mod->bPropMap) return !Mod->BlobPath.IsEmpty() && FPaths::FileExists(Mod->BlobPath);
    return !Mod->PakPath.IsEmpty() && FPaths::FileExists(Mod->PakPath);
}

bool UPTMapModSubsystem::SaveReceivedPropMap(const FString& ModId, const FString& Title, const TArray<uint8>& Blob)
{
    if (ModId.IsEmpty() || Blob.Num() == 0) return false;
    const FString Dir = FPaths::Combine(CacheRootDir(), SanitizeIdForFolder(ModId));
    IFileManager& FM = IFileManager::Get();
    FM.MakeDirectory(*Dir, /*Tree=*/true);

    const FString BlobFile = FPaths::Combine(Dir, TEXT("sculpt.bin"));
    if (!FFileHelper::SaveArrayToFile(Blob, *BlobFile)) return false;

    // mod.json con el Id ORIGINAL (para que FindMod(ModId) matchee) + la plantilla del juego.
    FString EscTitle = Title, EscId = ModId;
    EscTitle.ReplaceInline(TEXT("\\"), TEXT("\\\\")); EscTitle.ReplaceInline(TEXT("\""), TEXT("\\\""));
    EscId.ReplaceInline(TEXT("\\"), TEXT("\\\\"));    EscId.ReplaceInline(TEXT("\""), TEXT("\\\""));
    const FString Json = FString::Printf(
        TEXT("{ \"Id\": \"%s\", \"Title\": \"%s\", \"MapName\": \"/MapKit/Mapa_Plantilla\", \"Author\": \"\" }"),
        *EscId, *EscTitle);
    FFileHelper::SaveStringToFile(Json, *FPaths::Combine(Dir, TEXT("mod.json")));

    RescanMods();
    UE_LOG(LogPTMapMods, Log, TEXT("[MapMod] Mapa de props recibido y guardado en cache: '%s' (%d bytes)."), *ModId, Blob.Num());
    return true;
}

void UPTMapModSubsystem::AddModFromFolder(const FString& Folder, const FString& Id, bool bWorkshop)
{
    const FString PakPath  = FPaths::Combine(Folder, TEXT("map.pak"));
    const FString BlobPath = FPaths::Combine(Folder, TEXT("sculpt.bin"));
    const FString JsonPath = FPaths::Combine(Folder, TEXT("mod.json"));
    // Dos formatos: mapa .pak (map.pak, cocinado) o mapa de PROPS (sculpt.bin, in-game). Sin ninguno de
    // los dos no es un mapa-mod (ej: un banco de palabras con words.csv se ignora solo).
    const bool bHasPak  = FPaths::FileExists(PakPath);
    const bool bHasBlob = FPaths::FileExists(BlobPath);
    if (!bHasPak && !bHasBlob) return;
    if (!FPaths::FileExists(JsonPath)) { UE_LOG(LogPTMapMods, Warning, TEXT("[MapMod] '%s' sin mod.json → ignorado."), *Folder); return; }

    // mod.json → { MapName, Title, Author }
    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *JsonPath)) return;
    TSharedPtr<FJsonObject> Obj;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid())
    {
        UE_LOG(LogPTMapMods, Warning, TEXT("[MapMod] mod.json inválido en '%s'."), *Folder);
        return;
    }

    FPTMapMod Mod;
    // Id: si el mod.json trae uno explícito (mapas de props recibidos por chunks), ese manda — así el
    // cache queda indexado por el MISMO Id que usó el host. Si no, el pasado por el escaneo.
    Mod.Id            = (Obj->HasField(TEXT("Id")) && !Obj->GetStringField(TEXT("Id")).IsEmpty())
                        ? Obj->GetStringField(TEXT("Id")) : Id;
    Mod.bFromWorkshop = bWorkshop;
    Mod.bPropMap      = !bHasPak && bHasBlob; // props si no hay pak pero sí sculpt.bin
    Mod.PakPath       = bHasPak  ? PakPath  : FString();
    Mod.BlobPath      = bHasBlob ? BlobPath : FString();
    Mod.MapName       = Obj->GetStringField(TEXT("MapName"));
    Mod.Title         = Obj->HasField(TEXT("Title"))  ? Obj->GetStringField(TEXT("Title"))  : FPaths::GetCleanFilename(Folder);
    Mod.Author        = Obj->HasField(TEXT("Author")) ? Obj->GetStringField(TEXT("Author")) : FString();
    // Los mapas de props no montan pak → siempre "listos"; los .pak, según estén montados.
    Mod.bMounted      = Mod.bPropMap || MountedPakPaths.Contains(PakPath);

    // Los mapas de props viajan a la plantilla que viene con el juego; si el mod.json no la trae, la ponemos.
    if (Mod.MapName.IsEmpty() && Mod.bPropMap) Mod.MapName = TEXT("/MapKit/Mapa_Plantilla");
    if (Mod.MapName.IsEmpty())
    {
        UE_LOG(LogPTMapMods, Warning, TEXT("[MapMod] mod.json sin 'MapName' en '%s' → ignorado."), *Folder);
        return;
    }

    const FString Prev = FPaths::Combine(Folder, TEXT("preview.png"));
    if (FPaths::FileExists(Prev)) Mod.PreviewPath = Prev;

    Mods.Add(MoveTemp(Mod));
}

bool UPTMapModSubsystem::MountMod(const FString& Id)
{
    const FPTMapMod* Mod = FindMod(Id);
    if (!Mod) return false;
    if (Mod->bPropMap) return true; // mapa de props: no hay pak que montar (usa la plantilla del juego)
    if (MountedPakPaths.Contains(Mod->PakPath)) return true; // ya montado

    if (!FCoreDelegates::MountPak.IsBound())
    {
        // Suele pasar en el editor sin -pak: no hay soporte de montaje. En build empaquetada sí está.
        UE_LOG(LogPTMapMods, Warning, TEXT("[MapMod] MountPak no está disponible (¿editor sin paks?). No se montó '%s'."), *Id);
        return false;
    }

    // Orden alto (4) para que el contenido del mod gane sobre el base si hubiera colisión de rutas.
    IPakFile* Mounted = FCoreDelegates::MountPak.Execute(Mod->PakPath, 4);
    if (!Mounted)
    {
        UE_LOG(LogPTMapMods, Error, TEXT("[MapMod] Falló el montaje del pak '%s'."), *Mod->PakPath);
        return false;
    }

    MountedPakPaths.Add(Mod->PakPath);
    // Reflejar el estado en la entrada de la lista.
    if (FPTMapMod* M = Mods.FindByPredicate([&Id](const FPTMapMod& X){ return X.Id == Id; }))
        M->bMounted = true;
    UE_LOG(LogPTMapMods, Log, TEXT("[MapMod] Montado '%s' (mapa '%s')."), *Mod->PakPath, *Mod->MapName);
    OnMapModsUpdated.Broadcast();
    return true;
}

bool UPTMapModSubsystem::IsMounted(const FString& Id) const
{
    const FPTMapMod* Mod = FindMod(Id);
    if (!Mod) return false;
    return Mod->bPropMap || MountedPakPaths.Contains(Mod->PakPath);
}

FString UPTMapModSubsystem::GetTravelMap(const FString& Id) const
{
    const FPTMapMod* Mod = FindMod(Id);
    return Mod ? Mod->MapName : FString();
}

FString UPTMapModSubsystem::GetModBlobPath(const FString& Id) const
{
    const FPTMapMod* Mod = FindMod(Id);
    return (Mod && Mod->bPropMap) ? Mod->BlobPath : FString();
}

void UPTMapModSubsystem::RequestWorkshopDownload(const FString& WorkshopIdStr)
{
#if PT_WITH_STEAM
    if (!SteamUGC()) return;
    // Los ids de mods locales son "local:<carpeta>" → no se pueden descargar. Solo los numéricos (workshop).
    if (WorkshopIdStr.StartsWith(TEXT("local:"))) return;
    uint64 Id64 = 0;
    if (!LexTryParseString(Id64, *WorkshopIdStr) || Id64 == 0) return;
    const PublishedFileId_t Id = (PublishedFileId_t)Id64;
    SteamUGC()->SubscribeItem(Id);
    SteamUGC()->DownloadItem(Id, /*bHighPriority=*/true);
    UE_LOG(LogPTMapMods, Log, TEXT("[MapMod] Suscribiendo+descargando item %llu (el cliente no lo tenía)."), Id64);
#endif
}
