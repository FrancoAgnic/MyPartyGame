// Copyright Epic Games, Inc. All Rights Reserved.
// Fila de un item del catálogo del Workshop en el Browser (título + botón Añadir/suscribir).
//
// En el WBP derivado (nombres EXACTOS; varios opcionales):
//   TitleText      (TextBlock) → título del mod
//   AddButton      (Button)    → añadir (suscribir → se descarga)
//   AddButtonText  (TextBlock) → texto del botón (opcional; "Añadir"/"Añadido")
//   ThumbnailImage (Image)     → miniatura del mod (se baja del preview por HTTP) (opcional)
//   DescText       (TextBlock) → descripción del autor (opcional)
//   TypeTagText    (TextBlock) → tipo: "Banco de palabras" / "Mapa" (opcional)

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "Mods/PTWordPackSubsystem.h" // FPTWorkshopItem
#include "PTWorkshopItemRowWidget.generated.h"

class UTextBlock;
class UButton;
class UImage;
class UPTWorkshopBrowserWidget;

UCLASS()
class MYPARTYGAME_API UPTWorkshopItemRowWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    void Init(const FPTWorkshopItem& InItem, UPTWorkshopBrowserWidget* InOwner);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         UTextBlock* TitleText;
    UPROPERTY(meta = (BindWidget))         UButton*    AddButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* AddButtonText;
    UPROPERTY(meta = (BindWidgetOptional)) UImage*     ThumbnailImage;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* DescText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TypeTagText;

    UFUNCTION() void OnAddClicked();

private:
    FString ItemId;
    bool    bAdded = false;
    UPROPERTY() UPTWorkshopBrowserWidget* Owner = nullptr;

    // Baja la miniatura (preview URL) por HTTP y la pone en ThumbnailImage. Async, con guard de vida.
    void DownloadThumbnail(const FString& Url);
};
