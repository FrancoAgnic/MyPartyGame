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

UCLASS()
class MYPARTYGAME_API UPTSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ShowPanel();

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         USlider*   VolumeSlider;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* VolumeValueText;

    UPROPERTY(meta = (BindWidget)) UButton* EnglishButton;
    UPROPERTY(meta = (BindWidget)) UButton* SpanishButton;

    /** Botones de idiomas EXTRA (portugués y los que vengan). Se llenan desde Blueprint con
     *  PT Get Languages + PT Set Language: sumar un idioma no toca C++. */
    UPROPERTY(BlueprintReadWrite, Category = "Settings") TArray<UButton*> LanguageButtons;

    /** Contenedor de la sección de idioma (opcional): se deshabilita entero fuera del menú. */
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* LanguagePanel;
    /** Aviso "el idioma se cambia desde el menú principal" (opcional). */
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* LanguageHintText;

    UPROPERTY(meta = (BindWidget)) UButton* LowButton;
    UPROPERTY(meta = (BindWidget)) UButton* MediumButton;
    UPROPERTY(meta = (BindWidget)) UButton* HighButton;

    UPROPERTY(meta = (BindWidget)) UButton* ApplyButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* BackButton; // opcional: se cierra con Esc

    // Checkbox de sincronización vertical (VSync). Crear un CheckBox llamado "VSyncCheckBox" en el WBP.
    UPROPERTY(meta = (BindWidgetOptional)) UCheckBox* VSyncCheckBox;

    UFUNCTION() void OnVolumeChanged(float NewValue);
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

private:
    void ApplyLanguage(const FString& Code);
    void ApplyGraphics(int32 Quality);
    void ApplyLanguageSectionAvailability();
};
