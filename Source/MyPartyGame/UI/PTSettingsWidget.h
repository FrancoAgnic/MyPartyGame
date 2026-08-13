// Copyright Epic Games, Inc. All Rights Reserved.
// Popup de Settings (sonido/idioma/gráficos). Reusable desde el Main Menu y el menú de
// Escape del Lobby — ver UPTGameUserSettings para la persistencia real.
// Idioma y gráficos son botones tipo pill (English/Español, Low/Medium/High), no dropdowns,
// para matchear el mockup final. El resaltado visual del botón activo es trabajo de Blueprint
// (vía los eventos OnLanguageStateChanged/OnGraphicsStateChanged) — el C++ solo decide el estado.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTSettingsWidget.generated.h"

class USlider;
class UButton;
class UTextBlock;
class UCheckBox;
class UPanelWidget;
class UComboBoxString;

UCLASS()
class MYPARTYGAME_API UPTSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ShowPanel();

protected:
    virtual bool Initialize() override;

    // Volumen general viejo (opcional, master). Se mantiene por compatibilidad; los nuevos son Música/Efectos.
    UPROPERTY(meta = (BindWidgetOptional)) USlider*   VolumeSlider;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* VolumeValueText;

    // ── Volúmenes separados: Música y Efectos (crear en el WBP con estos nombres) ──
    UPROPERTY(meta = (BindWidgetOptional)) USlider*   MusicSlider;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* MusicValueText;
    UPROPERTY(meta = (BindWidgetOptional)) USlider*   SFXSlider;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* SFXValueText;

    // ── Idioma: DESPLEGABLE (ComboBox) que se llena SOLO con los idiomas del CSV ──
    // Muestra solo el idioma actual; al tocarlo se abre la lista (popup con scroll). Sumar un
    // idioma = agregar la columna al CSV, no se toca este widget ni el Blueprint.

    /** Desplegable de idiomas. Crear un ComboBox (String) llamado "LanguageCombo". */
    UPROPERTY(meta = (BindWidgetOptional)) UComboBoxString* LanguageCombo;

    /** Sección entera del idioma (título + combo): se COLAPSA en partida (solo se ve en el menú
     *  principal). Crear un contenedor llamado "LanguagePanel" que envuelva todo lo del idioma. */
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* LanguagePanel;

    /** Aviso opcional "el idioma se cambia desde el menú principal". */
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* LanguageHintText;

    // Botones viejos English/Español: quedan OPCIONALES por compatibilidad. Si el WBP ya no los
    // tiene, no pasa nada.
    UPROPERTY(meta = (BindWidgetOptional)) UButton* EnglishButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* SpanishButton;

    UPROPERTY(meta = (BindWidget)) UButton* LowButton;
    UPROPERTY(meta = (BindWidget)) UButton* MediumButton;
    UPROPERTY(meta = (BindWidget)) UButton* HighButton;

    UPROPERTY(meta = (BindWidget)) UButton* ApplyButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* BackButton; // opcional: se cierra con Esc

    // Checkbox de sincronización vertical (VSync). Crear un CheckBox llamado "VSyncCheckBox" en el WBP.
    UPROPERTY(meta = (BindWidgetOptional)) UCheckBox* VSyncCheckBox;

    UFUNCTION() void OnVolumeChanged(float NewValue);
    UFUNCTION() void OnMusicChanged(float NewValue);
    UFUNCTION() void OnSFXChanged(float NewValue);
    UFUNCTION() void OnEnglishClicked();
    UFUNCTION() void OnSpanishClicked();
    UFUNCTION() void OnLowClicked();
    UFUNCTION() void OnMediumClicked();
    UFUNCTION() void OnHighClicked();
    UFUNCTION() void OnApplyClicked();
    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnVSyncChanged(bool bIsChecked);

    /** Blueprint resalta el botón activo (estilo visual, no lógica) — llamado en ShowPanel y tras cada cambio. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
    void OnLanguageStateChanged(bool bIsEnglish);

    /** QualityIndex: 0=Low, 1=Medium, 2=High. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
    void OnGraphicsStateChanged(int32 QualityIndex);

    /** bAllowed=false cuando el panel se abre en partida (el idioma solo se cambia en el menú). */
    UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
    void OnLanguageAvailabilityChanged(bool bAllowed);

    /** true solo en el mapa MainMenu. */
    UFUNCTION(BlueprintPure, Category = "Settings")
    bool IsInMainMenu() const;

    /** El jugador eligió un idioma del desplegable. */
    UFUNCTION() void OnLanguageComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

private:
    void ApplyLanguage(const FString& Code);
    void ApplyGraphics(int32 Quality);
    void ApplyLanguageSectionAvailability();

    /** Llena el desplegable con los idiomas del CSV y selecciona el activo. */
    void BuildLanguageCombo();

    /** true mientras rellenamos el combo por código, para ignorar el OnSelectionChanged que dispara
     *  SetSelectedOption (si no, se re-aplicaría el idioma en un bucle al abrir el panel). */
    bool bUpdatingCombo = false;
};
