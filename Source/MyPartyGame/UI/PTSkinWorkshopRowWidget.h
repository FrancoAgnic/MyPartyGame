// Copyright Epic Games, Inc. All Rights Reserved.
// Fila de una SKIN del Workshop en el Skin Workshop del Locker. Un solo botón de acción que cambia según
// el estado: "Descargar" → "Descargando 45%" → "Equipar" → "Equipada". Más un botón "Editar" opcional
// (importa al Locker, equipa y entra a editarla).
//
// En el WBP derivado (nombres EXACTOS; varios opcionales):
//   TitleText        (TextBlock) → título de la skin
//   ActionButton     (Button)    → descargar / equipar
//   ActionButtonText (TextBlock) → texto del botón de acción (opcional)
//   EditButton       (Button)    → editar (se muestra solo cuando ya está descargada) (opcional)
//   ThumbnailImage   (Image)     → miniatura (se baja del preview por HTTP) (opcional)
//   DescText         (TextBlock) → descripción del autor (opcional)
//   TypeTagText      (TextBlock) → "Cabeza" / "Cuerpo" (opcional)

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "Mods/PTWordPackSubsystem.h" // FPTWorkshopItem
#include "PTSkinWorkshopRowWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UPTSkinWorkshopWidget;

UCLASS()
class MYPARTYGAME_API UPTSkinWorkshopRowWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    void Init(const FPTWorkshopItem& InItem, UPTSkinWorkshopWidget* InOwner);
    /** Re-lee el estado (suscrita / descargando / en el Locker / equipada) y actualiza los botones. */
    void RefreshState();

    const FString& GetItemId() const { return ItemId; }
    bool IsHeadSkin() const { return bHead; }

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         UTextBlock* TitleText;
    UPROPERTY(meta = (BindWidget))         UButton*    ActionButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* ActionButtonText;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    EditButton;
    UPROPERTY(meta = (BindWidgetOptional)) UImage*     ThumbnailImage;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* DescText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TypeTagText;

    UFUNCTION() void OnActionClicked();
    UFUNCTION() void OnEditClicked();

private:
    FString ItemId;
    bool    bHead = true;
    UPROPERTY() UPTSkinWorkshopWidget* Owner = nullptr;

    void DownloadThumbnail(const FString& Url);
};
