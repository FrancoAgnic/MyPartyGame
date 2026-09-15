// Copyright Epic Games, Inc. All Rights Reserved.
// Ventana de PUBLICAR al Workshop (widget aparte, se abre desde el Workshop Browser con "Publish").
// Dos secciones (pestañas): BANCO DE PALABRAS (elegir un CSV) y MAPAS (elegir uno de tus mapas creados
// in-game en un combo). Cada una: título + descripción + miniatura + Aplicar. Sube por ISteamUGC.
//
// En el WBP derivado (nombres EXACTOS; casi todo opcional):
//   BankTabButton / MapTabButton (Button)     → pestañas Banco / Mapas
//   BankPanel / MapPanel        (cualquier Widget) → contenedores de cada sección (se muestran/ocultan)
//   TitleBox   (EditableTextBox)               → título
//   DescBox    (MultiLineEditableTextBox)      → descripción
//   ThumbnailButton (Button) / ThumbnailImage (Image) → elegir/mostrar la miniatura
//   ApplyButton (Button)                       → publicar
//   CloseButton (Button)                       → cerrar
//   GuideButton (Button)                       → abrir la guía web
//   StatusText / PublishStatusText (TextBlock) → "Subiendo..." / "Faltan: ..."
//   ── Banco ──  UploadCsvButton (Button) + CsvButtonLabel (TextBlock dentro del botón)
//   ── Mapas ──  MapSelectCombo (ComboBoxString) + EditMapButton (Button)

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTPublishWidget.generated.h"

class UButton;
class UTextBlock;
class UImage;
class UWidget;
class UEditableTextBox;
class UMultiLineEditableTextBox;
class UComboBoxString;
class UPTWordPackSubsystem;
class UPTGameInstance;

UCLASS()
class MYPARTYGAME_API UPTPublishWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Mostrar la ventana (arranca en la pestaña Banco, formulario limpio). */
    UFUNCTION(BlueprintCallable, Category="Workshop") void ShowPanel();

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ── Pestañas ──
    UPROPERTY(meta=(BindWidgetOptional)) UButton* BankTabButton;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* MapTabButton;
    UPROPERTY(meta=(BindWidgetOptional)) UWidget* BankPanel;   // sección banco (CSV)
    UPROPERTY(meta=(BindWidgetOptional)) UWidget* MapPanel;    // sección mapas (combo)
    UPROPERTY(EditAnywhere, Category="Workshop") FLinearColor TabActiveColor   = FLinearColor(0.95f, 0.25f, 0.55f, 1.f);
    UPROPERTY(EditAnywhere, Category="Workshop") FLinearColor TabInactiveColor = FLinearColor(0.20f, 0.45f, 0.75f, 1.f);

    // ── Comunes ──
    UPROPERTY(meta=(BindWidgetOptional)) UEditableTextBox*          TitleBox;
    UPROPERTY(meta=(BindWidgetOptional)) UMultiLineEditableTextBox* DescBox;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    ThumbnailButton;
    UPROPERTY(meta=(BindWidgetOptional)) UImage*     ThumbnailImage;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    ApplyButton;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    CloseButton;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    GuideButton;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* StatusText;        // "Subiendo archivo..."
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* PublishStatusText; // validación ("Faltan: ...")

    // ── Banco ──
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    UploadCsvButton;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* CsvButtonLabel;

    // ── Mapas ── (solo PUBLICAR: elegís uno de tus mapas guardados; título/desc/miniatura se autocompletan)
    UPROPERTY(meta=(BindWidgetOptional)) UComboBoxString* MapSelectCombo;

    UPROPERTY(EditAnywhere, Category="Workshop") FString GuideUrl = TEXT("https://francoagnic.github.io/MyPartyGame/");

    UFUNCTION() void OnBankTabClicked();
    UFUNCTION() void OnMapTabClicked();
    UFUNCTION() void OnUploadCsvClicked();
    UFUNCTION() void OnThumbnailClicked();
    UFUNCTION() void OnApplyClicked();
    UFUNCTION() void OnCloseClicked();
    UFUNCTION() void OnGuideClicked();
    UFUNCTION() void OnMapSelected(FString SelectedItem, ESelectInfo::Type Type);
    UFUNCTION() void TickUploadingText();

private:
    UPTWordPackSubsystem* Packs() const;
    UPTGameInstance*      GI() const;
    void SwitchTab(int32 Tab);      // 0 = banco, 1 = mapas
    void ApplyTabVisual();
    void ResetForm();
    void RefreshMapList();
    void ShowMsg(const FText& Msg);
    UFUNCTION() void HideMsg();
    void OnPublished(bool bOk, const FString& Info);

    int32 ActiveTab = 0;            // 0 = banco, 1 = mapas
    bool  bBound = false;
    bool  bUploading = false;
    int32 UploadDots = 0;
    FTimerHandle UploadAnimTimer;
    FTimerHandle MsgHideTimer;

    FString PendingCsvPath;         // CSV elegido (banco)
    FString PendingImagePath;       // miniatura elegida
    TArray<FString> AuthoredMapSlugs; // slugs paralelos a los items del combo
};
