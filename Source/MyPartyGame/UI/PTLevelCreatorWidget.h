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
    UPROPERTY(meta=(BindWidgetOptional)) UButton*                   DeleteButton;      // abre el popup de confirmación
    // Popup de confirmación de borrado (nombres EXACTOS en el WBP, opcionales): panel + 2 botones + texto.
    UPROPERTY(meta=(BindWidgetOptional)) class UWidget*             DeleteConfirmPanel; // contenedor (oculto por defecto)
    UPROPERTY(meta=(BindWidgetOptional)) UButton*                   DeleteConfirmYes;   // "Borrar"
    UPROPERTY(meta=(BindWidgetOptional)) UButton*                   DeleteConfirmNo;    // "Cancelar"
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*                DeleteConfirmText;  // "¿Seguro que querés borrar «X»?"
    // Miniatura del mapa elegido en el combo (la que guardaste en el form de Save). Se autocompleta.
    UPROPERTY(meta=(BindWidgetOptional)) class UImage*              ThumbnailImage;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*                   CloseButton;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*                StatusText;

    UFUNCTION() void OnCreateClicked();
    UFUNCTION() void OnEditClicked();
    UFUNCTION() void OnDeleteClicked();      // abre el popup
    UFUNCTION() void OnDeleteConfirmYes();   // borra + cierra el popup
    UFUNCTION() void OnDeleteConfirmNo();    // cierra el popup
    UFUNCTION() void OnCloseClicked();
    UFUNCTION() void OnMapSelected(FString SelectedItem, ESelectInfo::Type Type); // autocompleta la miniatura

private:
    UPTGameInstance* GI() const;
    void RefreshList();
    TArray<FString> Slugs;
    FString PendingDeleteSlug; // slug esperando confirmación (2do click de Borrar)
};
