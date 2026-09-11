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

DEFINE_LOG_CATEGORY_STATIC(LogPTMapMods, Log, All);

void UPTMapModSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    RescanMods();
}

void UPTMapModSubsystem::Deinitialize()
{
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
    UE_LOG(LogPTMapMods, Log, TEXT("RescanMods: %d mapa(s)-mod."), Mods.Num());
    OnMapModsUpdated.Broadcast();
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

void UPTMapModSubsystem::AddModFromFolder(const FString& Folder, const FString& Id, bool bWorkshop)
{
    const FString PakPath  = FPaths::Combine(Folder, TEXT("map.pak"));
    const FString JsonPath = FPaths::Combine(Folder, TEXT("mod.json"));
    if (!FPaths::FileExists(PakPath))  return; // sin .pak no es un mapa-mod válido
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
    Mod.Id            = Id;
    Mod.PakPath       = PakPath;
    Mod.bFromWorkshop = bWorkshop;
    Mod.MapName       = Obj->GetStringField(TEXT("MapName"));
    Mod.Title         = Obj->HasField(TEXT("Title"))  ? Obj->GetStringField(TEXT("Title"))  : FPaths::GetCleanFilename(Folder);
    Mod.Author        = Obj->HasField(TEXT("Author")) ? Obj->GetStringField(TEXT("Author")) : FString();
    Mod.bMounted      = MountedPakPaths.Contains(PakPath);

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
    return Mod && MountedPakPaths.Contains(Mod->PakPath);
}

FString UPTMapModSubsystem::GetTravelMap(const FString& Id) const
{
    const FPTMapMod* Mod = FindMod(Id);
    return Mod ? Mod->MapName : FString();
}
