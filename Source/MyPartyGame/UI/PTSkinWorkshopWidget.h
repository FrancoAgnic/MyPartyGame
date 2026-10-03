// Copyright Epic Games, Inc. All Rights Reserved.
// Popup del Workshop de SKINS (se abre desde el Locker). Lista skins publicadas (tag "Skin"), permite
// Añadir (suscribir/descargar) + Equipar (importa al Locker y la pone), y Publicar tu skin EQUIPADA
// (cabeza + cuerpo) al Workshop.
//
// En el WBP derivado (nombres EXACTOS; varios opcionales):
//   ResultsBox        (ScrollBox/Panel)   → filas de resultados (lo llena el código)
//   SearchBox         (EditableTextBox)   → texto de búsqueda (opcional; busca al Enter)
//   SearchButton      (Button)            → lanzar búsqueda (opcional)
//   BackButton        (Button)            → cerrar el popup (opcional)
//   StatusText/EmptyText (TextBlock)      → "Buscando..." / "sin resultados" (opcionales)
//   PublishButton     (Button)            → abre el popup de publicar (opcional)
//   PublishPopup      (Border/Overlay)    → sección de publicar; arranca oculta (opcional)
//   PublishTitleBox   (EditableTextBox)   → título de la skin (opcional)
//   PublishDescBox    (MultiLineEditableTextBox) → descripción (opcional)
//   SkinPreviewImage  (Image)             → miniatura de la skin equipada que se va a publicar (opcional)
//   ApplyPublishButton(Button)            → publica la skin equipada
//   PopupCloseButton  (Button)            → cerrar la sección de publicar (opcional)
//   PublishStatusText (TextBlock)         → avisos del publish (opcional)
// En Details (categoría Workshop) asignar RowWidgetClass = WBP de la fila (deriva de PTSkinRowWidget).

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTSkinWorkshopWidget.generated.h"

class UPanelWidget;
class UButton;
class UTextBlock;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UImage;
class UWidget;
class UPTSkinRowWidget;
class UPTWordPackSubsystem;
class UPTLockerSubsystem;
struct FPTWorkshopItem;

UCLASS()
class MYPARTYGAME_API UPTSkinWorkshopWidget : public UPTUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Workshop") void ShowPanel();
    /** Suscribir (descargar) una skin — lo llama la fila. */
    void AddItem(const FString& ItemId);
    /** Importar al Locker + equipar una skin ya descargada — lo llama la fila. */
    void EquipItem(const FString& ItemId);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))         UPanelWidget*     ResultsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UEditableTextBox* SearchBox;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          SearchButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          BackButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*       StatusText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*       EmptyText;

    // Publicar.
    UPROPERTY(meta = (BindWidgetOptional)) UButton*                    PublishButton;
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*                    PublishPopup;
    UPROPERTY(meta = (BindWidgetOptional)) UEditableTextBox*           PublishTitleBox;
    UPROPERTY(meta = (BindWidgetOptional)) UMultiLineEditableTextBox*  PublishDescBox;
    UPROPERTY(meta = (BindWidgetOptional)) UImage*                     SkinPreviewImage;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*                    ApplyPublishButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*                    PopupCloseButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*                 PublishStatusText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Workshop")
    TSubclassOf<UPTSkinRowWidget> RowWidgetClass;

    UFUNCTION() void OnSearchClicked();
    UFUNCTION() void OnSearchCommitted(const FText& Text, ETextCommit::Type CommitType);
    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnPublishClicked();
    UFUNCTION() void OnApplyPublishClicked();
    UFUNCTION() void OnPopupCloseClicked();

private:
    void RunSearch();
    void OnSearchComplete(const TArray<FPTWorkshopItem>& Items, bool bOk);
    void OnPublished(bool bOk, const FString& Info);
    void SetStatus(const FText& Msg);
    void RefreshPublishPreview(); // muestra la miniatura de la skin equipada en SkinPreviewImage
    UPTWordPackSubsystem* Packs() const;
    UPTLockerSubsystem*   Locker() const;

    bool bBound = false;
};
