// Copyright Epic Games, Inc. All Rights Reserved.
// "Level Creator": popup del menú principal para CREAR un mapa nuevo (con título + descripción) o
// EDITAR uno ya guardado (elegido de un combo). Entra al modo autoría (esculpido libre).
//
// En el WBP derivado (nombres EXACTOS; opcionales):
//   ── Nuevo ──   NewTitleBox (EditableTextBox) + NewDescBox (MultiLineEditableTextBox) + CreateButton (Button)
//   ── Editar ──  MapSelectCombo (ComboBoxString con tus mapas) + EditButton (Button)
//   CloseButton (Button) / StatusText (TextBlock, avisos)

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTLevelCreatorWidget.generated.h"

class UButton;
class UTextBlock;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UComboBoxString;
class UPTGameInstance;

UCLASS()
class MYPARTYGAME_API UPTLevelCreatorWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="MapMod") void ShowPanel();

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidgetOptional)) UEditableTextBox*          NewTitleBox;
    UPROPERTY(meta=(BindWidgetOptional)) UMultiLineEditableTextBox* NewDescBox;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*                   CreateButton;
    UPROPERTY(meta=(BindWidgetOptional)) UComboBoxString*           MapSelectCombo;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*                   EditButton;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*                   CloseButton;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*                StatusText;

    UFUNCTION() void OnCreateClicked();
    UFUNCTION() void OnEditClicked();
    UFUNCTION() void OnCloseClicked();

private:
    UPTGameInstance* GI() const;
    void RefreshList();
    TArray<FString> Slugs;
};
