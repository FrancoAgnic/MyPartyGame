// Copyright Epic Games, Inc. All Rights Reserved.
// Bancos de palabras de la comunidad (Steam Workshop) + carpeta local para testear.
// Un "word pack" = una CARPETA con:
//   words.csv   (obligatorio) → "Palabra,Categoría,Dificultad" (mismo formato que el CSV del host)
//   pack.txt    (opcional)    → línea 1 = título, línea 2 = autor. Si falta, el título es el de la carpeta.
//   preview.png (opcional)    → imagen para la página del Workshop (solo se usa al publicar)
//
// Fuentes que escanea:
//   - LOCAL:    <ProjectDir>/WordPacks/*   (para probar sin Steam)
//   - WORKSHOP: los items suscritos del usuario (ISteamUGC → carpeta de instalación)
//
// El host elige un pack en el lobby → se cargan sus palabras en PendingMatchSettings.CustomWords
// (reusa UPTGameInstance::LoadCustomWordsFromCSVFile). Los clientes NO necesitan las palabras
// (el server elige y manda la enmascarada); solo se les replica el TÍTULO del pack para mostrarlo.

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PTWordPackSubsystem.generated.h"

// Un banco de palabras encontrado (local o del Workshop), para la UI.
USTRUCT(BlueprintType)
struct FPTWordPack
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="WordPack") FString Id;       // workshop item id (string) o "local:<carpeta>"
    UPROPERTY(BlueprintReadOnly, Category="WordPack") FString Title;
    UPROPERTY(BlueprintReadOnly, Category="WordPack") FString Author;
    UPROPERTY(BlueprintReadOnly, Category="WordPack") FString CsvPath;  // ruta absoluta a words.csv
    UPROPERTY(BlueprintReadOnly, Category="WordPack") bool    bFromWorkshop = false;
};

DECLARE_MULTICAST_DELEGATE(FPTOnWordPacksUpdated);
// Resultado de publicar: (ok, mensaje/itemId o error).
DECLARE_MULTICAST_DELEGATE_TwoParams(FPTOnWordPackPublished, bool /*bOk*/, const FString& /*Info*/);

UCLASS()
class MYPARTYGAME_API UPTWordPackSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Re-escanea packs locales + del Workshop. Al terminar dispara OnWordPacksUpdated. */
    UFUNCTION(BlueprintCallable, Category="WordPack") void RescanPacks();
    const TArray<FPTWordPack>& GetPacks() const { return Packs; }
    /** Busca un pack por Id (para que el host lo seleccione). Devuelve nullptr si no está. */
    const FPTWordPack* FindPack(const FString& Id) const;

    /** Publica un banco al Workshop: sube una carpeta con words.csv (+ preview opcional). Async →
     *  OnWordPackPublished. Requiere Steam + Workshop habilitado para la app. */
    UFUNCTION(BlueprintCallable, Category="WordPack")
    void PublishWordPack(const FString& CsvPath, const FString& Title, const FString& Description, const FString& PreviewPath);

    FPTOnWordPacksUpdated  OnWordPacksUpdated;
    FPTOnWordPackPublished OnWordPackPublished;

private:
    // Agrega un pack desde una carpeta si tiene words.csv válido. bWorkshop marca el origen.
    void AddPackFromFolder(const FString& Folder, const FString& Id, bool bWorkshop);
    void ScanLocalPacks();
    void ScanWorkshopPacks();

    UPROPERTY() TArray<FPTWordPack> Packs;

#if PT_WITH_STEAM
    struct FPTWorkshopPublish* Publisher = nullptr; // puntero opaco (steam headers fuera de UHT)
#endif
};
