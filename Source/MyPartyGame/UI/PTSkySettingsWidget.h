// Copyright Epic Games, Inc. All Rights Reserved.
// Panel in-game (editor de mapas) para editar el AMBIENTE: momento del día + colores del cielo + niebla.
// Mueve sliders → aplica en vivo (APTMapEnvironment::SetSkySettings). Los colores se editan con sliders
// R/G/B. Todo BindWidgetOptional: poné en el WBP solo los controles que quieras, con nombres EXACTOS.
//
// Scalars (USlider 0..1, se mapean a su rango):
//   Slider_TimeOfDay, Slider_SunYaw, Slider_SunIntensity, Slider_FogDensity, Slider_AmbientIntensity
// Colores (USlider 0..1 por canal):
//   Slider_SkyTop_R/G/B, Slider_SkyHorizon_R/G/B, Slider_SunColor_R/G/B,
//   Slider_FogColor_R/G/B, Slider_AmbientColor_R/G/B
// Swatches opcionales (UImage, se tiñen con el color): Swatch_SkyTop, Swatch_SkyHorizon, Swatch_SunColor,
//   Swatch_FogColor, Swatch_AmbientColor
// Botones opcionales: CloseButton

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "../Mods/PTMapEnvironment.h" // FPTSkySettings
#include "PTSkySettingsWidget.generated.h"

class USlider;
class UImage;
class UButton;

UCLASS()
class MYPARTYGAME_API UPTSkySettingsWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Abre el panel: busca el entorno, carga los valores actuales en los sliders y lo muestra. */
    void ShowPanel();
    void HidePanel();

    // Rangos (slider 0..1 → valor real). Editables en el WBP.
    UPROPERTY(EditAnywhere, Category="Sky") float MaxSunYaw       = 360.f;
    UPROPERTY(EditAnywhere, Category="Sky") float MaxSunIntensity = 10.f;
    UPROPERTY(EditAnywhere, Category="Sky") float MaxFogDensity   = 0.2f;
    UPROPERTY(EditAnywhere, Category="Sky") float MaxAmbient      = 5.f;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_TimeOfDay;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SunYaw;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SunIntensity;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_FogDensity;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_AmbientIntensity;

    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SkyTop_R;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SkyTop_G;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SkyTop_B;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SkyHorizon_R;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SkyHorizon_G;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SkyHorizon_B;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SunColor_R;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SunColor_G;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SunColor_B;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_FogColor_R;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_FogColor_G;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_FogColor_B;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_AmbientColor_R;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_AmbientColor_G;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_AmbientColor_B;

    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_SkyTop;
    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_SkyHorizon;
    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_SunColor;
    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_FogColor;
    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_AmbientColor;

    UPROPERTY(meta=(BindWidgetOptional)) UButton* CloseButton;

    UFUNCTION() void OnAnyChanged(float Value); // cualquier slider → reconstruir + aplicar
    UFUNCTION() void OnCloseClicked();

private:
    class APTMapEnvironment* FindEnv() const;
    void PopulateFromEnv();     // carga los sliders desde los settings actuales
    void ApplyFromSliders();    // reconstruye FPTSkySettings desde los sliders y lo aplica
    void UpdateSwatches(const FPTSkySettings& S);
    bool bLoadingUI = false;    // evita el loop mientras seteo los sliders al abrir
};
