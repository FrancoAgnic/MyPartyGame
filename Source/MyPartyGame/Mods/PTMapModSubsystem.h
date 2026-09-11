// Copyright Epic Games, Inc. All Rights Reserved.
// Mapas de la comunidad (Steam Workshop) — M1: LOADER DE PAKS LOCAL.
// Un mapa-mod = una CARPETA con:
//   map.pak   (obligatorio) → el mapa COCINADO a .pak (mismo engine 5.8, ver M3/modding kit)
//   mod.json  (obligatorio) → { "MapName": "...", "Title": "...", "Author": "..." }
//                             MapName = ruta de paquete del mapa para viajar (ej "/Game/MapMods/MiMapa/MiMapa")
//   preview.png (opcional)  → miniatura
//
// Fuentes que escanea (M1 solo LOCAL; el Workshop se suma en M4):
//   - LOCAL: <ProjectDir>/MapMods/*   (para testear sin Steam, con un .pak cocinado a mano)
//
// MONTAJE: MountMod() monta el .pak en runtime con FCoreDelegates::MountPak. Una vez montado, el mapa
// (MapName) queda disponible para OpenLevel/ServerTravel. Ojo: en el EDITOR el montaje de paks suele no
// estar disponible (MountPak sin bindear) → se prueba en build empaquetada o con paks activos.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PTMapModSubsystem.generated.h"

// Un mapa-mod encontrado (local o —a futuro— del Workshop), para la UI y el viaje.
USTRUCT(BlueprintType)
struct FPTMapMod
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="MapMod") FString Id;         // "local:<carpeta>" o (a futuro) workshop id
    UPROPERTY(BlueprintReadOnly, Category="MapMod") FString Title;
    UPROPERTY(BlueprintReadOnly, Category="MapMod") FString Author;
    UPROPERTY(BlueprintReadOnly, Category="MapMod") FString MapName;    // ruta de paquete a la que se viaja
    UPROPERTY(BlueprintReadOnly, Category="MapMod") FString PakPath;    // ruta absoluta al map.pak
    UPROPERTY(BlueprintReadOnly, Category="MapMod") FString PreviewPath;// preview.png local (si hay)
    UPROPERTY(BlueprintReadOnly, Category="MapMod") bool    bFromWorkshop = false;
    UPROPERTY(BlueprintReadOnly, Category="MapMod") bool    bMounted = false;
};

DECLARE_MULTICAST_DELEGATE(FPTOnMapModsUpdated);

UCLASS()
class MYPARTYGAME_API UPTMapModSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Re-escanea los mapas-mod locales. Al terminar dispara OnMapModsUpdated. */
    UFUNCTION(BlueprintCallable, Category="MapMod") void RescanMods();
    const TArray<FPTMapMod>& GetMods() const { return Mods; }
    const FPTMapMod* FindMod(const FString& Id) const;

    /** Monta el .pak del mod (si no lo estaba) para que su mapa quede disponible. Devuelve true si quedó
     *  montado. Idempotente. */
    UFUNCTION(BlueprintCallable, Category="MapMod") bool MountMod(const FString& Id);
    /** ¿Está el pak de este mod montado en este proceso? */
    UFUNCTION(BlueprintCallable, Category="MapMod") bool IsMounted(const FString& Id) const;
    /** Ruta de paquete del mapa (MapName) para viajar; vacío si no existe el mod. */
    UFUNCTION(BlueprintCallable, Category="MapMod") FString GetTravelMap(const FString& Id) const;
    /** (Cliente) Se asegura de tener el item del Workshop: si el Id es numérico (workshop) y no está,
     *  lo SUSCRIBE + DownloadItem (el watcher rescanea al terminar). Para que un cliente pueda montar un
     *  mapa que el host eligió pero que él no tenía. No-op para mods locales o sin Steam. */
    UFUNCTION(BlueprintCallable, Category="MapMod") void RequestWorkshopDownload(const FString& WorkshopIdStr);

    FPTOnMapModsUpdated OnMapModsUpdated;

private:
    void ScanLocalMapMods();
    void ScanWorkshopMaps(); // items suscritos del Workshop cuyo contenido es un map.pak (mapas de mod)
    void AddModFromFolder(const FString& Folder, const FString& Id, bool bWorkshop);

    UPROPERTY() TArray<FPTMapMod> Mods;
    TSet<FString> MountedPakPaths; // paks ya montados en este proceso (para no re-montar)

#if PT_WITH_STEAM
    // Cuando Steam termina de bajar un item suscrito, re-escanea (suscribir NO baja el contenido solo).
    struct FPTMapDownloadWatcher* DownloadWatcher = nullptr;
#endif
};
