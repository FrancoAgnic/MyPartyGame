// Copyright Epic Games, Inc. All Rights Reserved.
// SKIN WORKSHOP: popup que se abre desde el Locker (botón "Skin Workshop"). Es un Workshop Browser
// filtrado a SKINS (tag "Skin"): buscás, descargás (se importa a un slot libre del Locker) y la equipás
// o editás como cualquier skin propia. En el mismo popup, "Publicar skin" abre un panel para subir una
// de tus skins del Locker (cabeza o cuerpo) al Workshop.
//
// En el WBP derivado (nombres EXACTOS; casi todo opcional):
//   ── Browser ──
//   ResultsBox        (ScrollBox/Panel)   → filas de resultados (lo llena el código)        [obligatorio]
//   SearchBox         (EditableTextBox)   → texto de búsqueda (busca al Enter)
//   SearchButton      (Button)            → lanzar búsqueda
//   AllTabButton / HeadTabButton / BodyTabButton (Button) → filtro Todas / Cabezas / Cuerpos
//   CloseButton       (Button)            → cerrar el popup (también Esc)
//   TitleText / EmptyText / StatusText (TextBlock) → título / "sin resultados" / "Buscando..." y avisos
//   PublishSkinButton (Button)            → abre el panel de publicar
//   ── Panel de publicar (dentro del mismo WBP) ──
//   PublishPanel      (Border/Overlay)    → contenedor del panel; arranca oculto
//   SkinSelectCombo   (ComboBoxString)    → tus skins del Locker ("Cabeza 2", "Cuerpo 1"...)
//   SkinPreviewImage  (Image)             → miniatura de la skin elegida (o la imagen que cargues)
//   ThumbnailButton   (Button)            → cambiar la miniatura por una imagen tuya (opcional)
//   PublishTitleBox   (EditableTextBox) / PublishDescBox (MultiLineEditableTextBox)
//   ApplyPublishButton(Button)            → publicar
//   PublishCloseButton(Button)            → cerrar el panel
//   PublishStatusText (TextBlock)         → validación ("Faltan: ...") / "Subiendo..." / resultado
// En Details (categoría Workshop) asignar RowWidgetClass = WBP de la fila (deriva de PTSkinWorkshopRowWidget).

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "Mods/PTWordPackSubsystem.h" // FPTWorkshopItem
#include "PTSkinWorkshopWidget.generated.h"

class UPanelWidget;
class UButton;
class UTextBlock;
class UImage;
class UWidget;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UComboBoxString;
class UPTSkinWorkshopRowWidget;
class UPTLockerWidget;
class UPTLockerSubsystem;
class UPTWordPackSubsystem;
class APTLobbyPlayerController;

// Estado de una skin del Workshop para el jugador local (lo usa la fila para elegir su botón).
enum class EPTSkinRowState : uint8
{
    NotDownloaded, // ni suscrita ni en el Locker → "Descargar"
    Downloading,   // suscrita, Steam la está bajando → "Descargando 45%"
    Ready,         // descargada / ya en el Locker → "Equipar"
    Equipped,      // es la skin equipada → "Equipada"
};

// Una skin del Locker que se puede publicar (item del combo de publicar).
struct FPTSkinPublishOption
{
    bool  bHead = true;
    int32 Slot  = -1;
};

UCLASS()
class MYPARTYGAME_API UPTSkinWorkshopWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Mostrar el popup (arranca en "Todas" con las populares). OwnerLocker se refresca al equipar/importar. */
    void ShowPanel(UPTLockerWidget* InOwnerLocker);
    void ClosePanel();

    // ── Lo llaman las filas ──
    EPTSkinRowState GetSkinState(const FString& ItemId, bool bHead, float& OutProgress01) const;
    void DownloadSkin(const FString& ItemId);
    void EquipSkin(const FString& ItemId, bool bHead);
    void EditSkin(const FString& ItemId, bool bHead);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }

    // ── Browser ──
    UPROPERTY(meta = (BindWidget))         UPanelWidget*     ResultsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UEditableTextBox* SearchBox;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          SearchButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          AllTabButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          HeadTabButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          BodyTabButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          CloseButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*       TitleText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*       EmptyText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*       StatusText;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*          PublishSkinButton;

    // ── Panel de publicar ──
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*                   PublishPanel;
    UPROPERTY(meta = (BindWidgetOptional)) UComboBoxString*           SkinSelectCombo;
    UPROPERTY(meta = (BindWidgetOptional)) UImage*                    SkinPreviewImage;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*                   ThumbnailButton;
    UPROPERTY(meta = (BindWidgetOptional)) UEditableTextBox*          PublishTitleBox;
    UPROPERTY(meta = (BindWidgetOptional)) UMultiLineEditableTextBox* PublishDescBox;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*                   ApplyPublishButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*                   PublishCloseButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*                PublishStatusText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Workshop")
    TSubclassOf<UPTSkinWorkshopRowWidget> RowWidgetClass;

    UPROPERTY(EditAnywhere, Category="Workshop") FLinearColor TabActiveColor   = FLinearColor(0.95f, 0.25f, 0.55f, 1.f);
    UPROPERTY(EditAnywhere, Category="Workshop") FLinearColor TabInactiveColor = FLinearColor(0.20f, 0.45f, 0.75f, 1.f);

    UFUNCTION() void OnSearchClicked();
    UFUNCTION() void OnSearchCommitted(const FText& Text, ETextCommit::Type CommitType);
    UFUNCTION() void OnAllTabClicked();
    UFUNCTION() void OnHeadTabClicked();
    UFUNCTION() void OnBodyTabClicked();
    UFUNCTION() void OnCloseClicked();
    UFUNCTION() void OnPublishSkinClicked();
    UFUNCTION() void OnSkinSelected(FString SelectedItem, ESelectInfo::Type Type);
    UFUNCTION() void OnThumbnailClicked();
    UFUNCTION() void OnApplyPublishClicked();
    UFUNCTION() void OnPublishCloseClicked();
    UFUNCTION() void TickRefresh();       // progreso de descargas + importar las que terminaron
    UFUNCTION() void TickUploadingText(); // anima "Subiendo archivo..."
    UFUNCTION() void HideStatus();

private:
    UPTWordPackSubsystem*     Packs()   const;
    UPTLockerSubsystem*       Locker()  const;
    APTLobbyPlayerController* LobbyPC() const;

    void SwitchFilter(int32 Filter); // 0 = todas, 1 = cabezas, 2 = cuerpos
    void ApplyFilterVisual();
    void RunSearch();
    void OnSearchComplete(const TArray<FPTWorkshopItem>& Items, bool bOk);
    void RebuildRows();
    void RefreshRows();
    void OnPacksUpdated();      // Steam terminó una descarga (el subsistema re-escaneó)
    void OnPublished(bool bOk, const FString& Info);
    void ShowStatus(const FText& Msg, bool bInPublishPanel, float AutoHideSeconds = 3.f);
    void NotifyLockerChanged(); // refresca los tiles del Locker de fondo

    /** Se asegura de que el item esté importado en el Locker. Devuelve el slot (-1 si falló; avisa). */
    int32 EnsureImported(const FString& ItemId, bool& InOutHead);
    void ImportFinishedDownloads(); // descargas pedidas acá que ya terminaron → al Locker

    void ResetPublishForm();
    void RefreshPublishList();
    void ShowSlotPreview();

    TWeakObjectPtr<UPTLockerWidget> OwnerLocker;
    bool  bBound = false;
    int32 ActiveFilter = 0;
    bool  bSearching = false;                 // ignora resultados de búsquedas que no lanzó este popup
    UPROPERTY() TArray<UPTSkinWorkshopRowWidget*> Rows;
    TArray<FPTWorkshopItem> LastItems;        // resultados de la última búsqueda (se filtran por pestaña)
    TSet<FString> PendingDownloads;           // descargas pedidas desde acá (se importan al terminar)
    FTimerHandle RefreshTimer;
    FTimerHandle StatusHideTimer;

    // ── Publicar ──
    TArray<FPTSkinPublishOption> PublishOptions; // paralelo a los items del combo
    FString PendingImagePath;                 // miniatura elegida a mano (vacío = la del slot)
    bool    bUploading = false;
    int32   UploadDots = 0;
    FTimerHandle UploadAnimTimer;
};
