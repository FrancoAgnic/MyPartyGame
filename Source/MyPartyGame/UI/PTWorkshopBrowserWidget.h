// Copyright Epic Games, Inc. All Rights Reserved.
// Browser del Workshop (se abre desde el menú principal). Dos secciones con su buscador:
// BANCOS DE PALABRAS (funcional) y MAPAS (bloqueada, "Próximamente"). Cada item tiene botón "Añadir"
// que lo suscribe en Steam → se descarga y queda disponible en la Biblioteca del lobby.
//
// En el WBP derivado (nombres EXACTOS; varios opcionales):
//   ResultsBox        (ScrollBox/Panel)   → filas de resultados (lo llena el código)
//   SearchBox         (EditableTextBox)   → texto de búsqueda (opcional)
//   SearchButton      (Button)            → lanzar búsqueda (opcional; también busca al Enter en SearchBox)
//   WordBanksTabButton / MapsTabButton (Button) → pestañas (opcionales)
//   MapsLockedPanel   (cualquier Widget)  → overlay "Próximamente" que se muestra en la pestaña Mapas (opcional)
//   BackButton        (Button)            → cerrar (opcional)
//   PublishButton     (Button)            → abre el popup de publicar (opcional)
//   PublishPopup      (Border/Overlay)    → popup con Upload/Template; arranca oculto (opcional)
//   UploadCsvButton   (Button)            → dentro del popup: subir .csv (texto "Upload CSV")
//   GuideButton       (Button)            → dentro del popup: abre la guía web ("Guide")
//   PopupCloseButton  (Button)            → cerrar el popup (opcional)
//   TitleText / EmptyText / StatusText (TextBlock) → título / "sin resultados" / "Buscando..." (opcionales)
// En Details (categoría Workshop) asignar RowWidgetClass = WBP de la fila (deriva de PTWorkshopItemRowWidget).

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTWorkshopBrowserWidget.generated.h"

class UPanelWidget;
class UButton;
class UTextBlock;
class UEditableTextBox;
class UWidget;
class UPTWorkshopItemRowWidget;
class UPTWordPackSubsystem;
struct FPTWorkshopItem;

UCLASS()
class MYPARTYGAME_API UPTWorkshopBrowserWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="Workshop") void ShowPanel();
    /** Suscribir (descargar) un item — lo llama la fila. */
    void AddItem(const FString& ItemId);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))         UPanelWidget*     ResultsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UEditableTextBox* SearchBox;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          SearchButton;
    // "Publish" abre un popup chico con dos botones: subir CSV y descargar plantilla.
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          PublishButton;
    // Popup (Border/Overlay) que agrupa Upload/Template. Arranca oculto; lo abre PublishButton.
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*          PublishPopup;
    // Dentro del popup: campos para el TÍTULO (obligatorio-ish) y la DESCRIPCIÓN (opcional) del banco.
    // Si faltan o van vacíos, el título cae al nombre del CSV prettificado. Nombrarlos así en el WBP.
    UPROPERTY(meta = (BindWidgetOptional)) UEditableTextBox* PublishTitleBox;
    UPROPERTY(meta = (BindWidgetOptional)) UEditableTextBox* PublishDescBox;
    // Dentro del popup: subir tu .csv al Workshop (resultado en StatusText).
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          UploadCsvButton;
    // Abre la guía (página web) de cómo crear/subir el CSV, en el navegador.
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          GuideButton;
    // URL de la guía. Editable por si cambia el repo/usuario.
    UPROPERTY(EditAnywhere, Category="Workshop") FString     GuideUrl = TEXT("https://francoagnic.github.io/MyPartyGame/");
    // Cerrar el popup (opcional).
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          PopupCloseButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          WordBanksTabButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          MapsTabButton;
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*          MapsLockedPanel;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          BackButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*       TitleText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*       EmptyText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*       StatusText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Workshop")
    TSubclassOf<UPTWorkshopItemRowWidget> RowWidgetClass;

    UPROPERTY(EditAnywhere, Category="Workshop") FLinearColor TabActiveColor   = FLinearColor(0.95f, 0.25f, 0.55f, 1.f);
    UPROPERTY(EditAnywhere, Category="Workshop") FLinearColor TabInactiveColor = FLinearColor(0.20f, 0.45f, 0.75f, 1.f);

    UFUNCTION() void OnSearchClicked();
    UFUNCTION() void OnSearchCommitted(const FText& Text, ETextCommit::Type CommitType);
    UFUNCTION() void OnWordBanksTabClicked();
    UFUNCTION() void OnMapsTabClicked();
    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnPublishClicked();     // abre el popup
    UFUNCTION() void OnUploadCsvClicked();   // sube el .csv (dentro del popup)
    UFUNCTION() void OnGuideClicked();       // abre la guía web
    UFUNCTION() void OnPopupCloseClicked();

private:
    void SwitchTab(int32 Tab);           // 0 = bancos (funcional), 1 = mapas (bloqueado)
    void ApplyTabVisual();
    void RunSearch();
    void OnSearchComplete(const TArray<FPTWorkshopItem>& Items, bool bOk);
    void OnPublished(bool bOk, const FString& Info);
    UPTWordPackSubsystem* Packs() const;

    int32 ActiveTab = 0;
    bool  bBound = false;
};
