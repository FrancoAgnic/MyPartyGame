// Copyright Epic Games, Inc. All Rights Reserved.
// Formulario de GUARDAR mapa (se abre desde el botón Guardar del menú de pausa, en modo autoría).
// Autocompleta título/descripción del mapa actual (editables) y deja elegir una miniatura. Al confirmar,
// guarda el escenario (SaveSnapshot) + los metadatos (mod.json + preview.png).
//
// En el WBP derivado (nombres EXACTOS; opcionales):
//   TitleBox (EditableTextBox) + DescBox (MultiLineEditableTextBox)
//   ThumbnailButton (Button) + ThumbnailImage (Image)
//   ConfirmButton / CancelButton (Button) / StatusText (TextBlock)

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTSaveMapWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UPTGameInstance;

UCLASS()
class MYPARTYGAME_API UPTSaveMapWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="MapMod") void ShowPanel();

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidgetOptional)) UEditableTextBox*          TitleBox;
    UPROPERTY(meta=(BindWidgetOptional)) UMultiLineEditableTextBox* DescBox;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    ThumbnailButton;
    UPROPERTY(meta=(BindWidgetOptional)) UImage*     ThumbnailImage;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    ConfirmButton;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    CancelButton;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* StatusText;

    UFUNCTION() void OnThumbnailClicked();
    UFUNCTION() void OnConfirmClicked();
    UFUNCTION() void OnCancelClicked();

private:
    UPTGameInstance* GI() const;
    FString PendingThumb; // imagen elegida esta vez (vacío = mantener la que ya tenga el mapa)
};
