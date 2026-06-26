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

    UPROPERTY(meta = (BindWidget)) UButton* LowButton;
    UPROPERTY(meta = (BindWidget)) UButton* MediumButton;
    UPROPERTY(meta = (BindWidget)) UButton* HighButton;

    UPROPERTY(meta = (BindWidget)) UButton* BackButton;

    UFUNCTION() void OnVolumeChanged(float NewValue);
    UFUNCTION() void OnEnglishClicked();
    UFUNCTION() void OnSpanishClicked();
    UFUNCTION() void OnLowClicked();
    UFUNCTION() void OnMediumClicked();
    UFUNCTION() void OnHighClicked();
    UFUNCTION() void OnBackClicked();

    /** Blueprint resalta el botón activo (estilo visual, no lógica) — llamado en ShowPanel y tras cada cambio. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
    void OnLanguageStateChanged(bool bIsEnglish);

    /** QualityIndex: 0=Low, 1=Medium, 2=High. */
    UFUNCTION(BlueprintImplementableEvent, Category = "Settings")
    void OnGraphicsStateChanged(int32 QualityIndex);

private:
    void ApplyLanguage(const FString& Code);
    void ApplyGraphics(int32 Quality);
};
