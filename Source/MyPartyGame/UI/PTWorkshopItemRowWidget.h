// Copyright Epic Games, Inc. All Rights Reserved.
// Fila de un item del catálogo del Workshop en el Browser (título + botón Añadir/suscribir).
//
// En el WBP derivado (nombres EXACTOS):
//   TitleText     (TextBlock) → título del mod
//   AddButton     (Button)    → añadir (suscribir → se descarga)
//   AddButtonText (TextBlock) → texto del botón (opcional; "Añadir"/"Añadido")

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "Mods/PTWordPackSubsystem.h" // FPTWorkshopItem
#include "PTWorkshopItemRowWidget.generated.h"

class UTextBlock;
class UButton;
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

    UFUNCTION() void OnAddClicked();

private:
    FString ItemId;
    bool    bAdded = false;
    UPROPERTY() UPTWorkshopBrowserWidget* Owner = nullptr;
};
