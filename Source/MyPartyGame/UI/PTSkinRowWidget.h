// Copyright Epic Games, Inc. All Rights Reserved.
// Fila de una SKIN del catálogo del Workshop en el popup de skins. Igual que la fila de bancos/mapas,
// pero con DOS acciones: Añadir (suscribir → descarga) y Equipar (importar al Locker + equipar).
//
// En el WBP derivado (nombres EXACTOS; varios opcionales):
//   TitleText      (TextBlock) → título de la skin
//   AddButton      (Button)    → añadir (suscribir → se descarga)
//   AddButtonText  (TextBlock) → texto del botón Añadir (opcional)
//   EquipButton    (Button)    → equipar (importa al Locker y la pone; funciona una vez descargada)
//   ThumbnailImage (Image)     → miniatura (se baja del preview por HTTP) (opcional)
//   DescText       (TextBlock) → descripción del autor (opcional)

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "Mods/PTWordPackSubsystem.h" // FPTWorkshopItem
#include "PTSkinRowWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UPTSkinWorkshopWidget;

UCLASS()
class MYPARTYGAME_API UPTSkinRowWidget : public UPTUserWidget
{
    GENERATED_BODY()
public:
    void Init(const FPTWorkshopItem& InItem, UPTSkinWorkshopWidget* InOwner);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         UTextBlock* TitleText;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    AddButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* AddButtonText;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    EquipButton;
    UPROPERTY(meta = (BindWidgetOptional)) UImage*     ThumbnailImage;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* DescText;

    UFUNCTION() void OnAddClicked();
    UFUNCTION() void OnEquipClicked();

private:
    FString ItemId;
    bool    bAdded = false;
    UPROPERTY() UPTSkinWorkshopWidget* Owner = nullptr;
    void DownloadThumbnail(const FString& Url);
};
