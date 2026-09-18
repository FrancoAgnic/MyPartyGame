// Copyright Epic Games, Inc. All Rights Reserved.
// Panel in-game (editor de mapas) para editar el AMBIENTE: momento del día + colores del cielo + niebla.
// Mueve sliders → aplica en vivo (APTMapEnvironment::SetSkySettings). Los colores se editan con sliders
// R/G/B. Todo BindWidgetOptional: poné en el WBP solo los controles que quieras, con nombres EXACTOS.
//
// Scalars (USlider 0..1, se mapean a su rango):
//   Slider_TimeOfDay, Slider_SunYaw, Slider_SunIntensity, Slider_FogDensity, Slider_AmbientIntensity
// Colores: cada uno tiene su PROPIA rueda embebida (UPTColorWheelWidget). Editás cada color directo.
//   Nombres EXACTOS: Wheel_SkyTop, Wheel_SkyHorizon, Wheel_SunColor, Wheel_FogColor, Wheel_AmbientColor
//   (cada uno es una instancia de tu WBP_ColorWheel colocada en el panel)
// Botones opcionales: CloseButton

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "../Mods/PTMapEnvironment.h" // FPTSkySettings
#include "PTSkySettingsWidget.generated.h"

class USlider;
class UImage;
class UButton;
class UPTColorWheelWidget;

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
    // Knobs cartoon: slider 0..1 → [Min..Max].
    UPROPERTY(EditAnywhere, Category="Sky") float MaxBands        = 8.f;
    UPROPERTY(EditAnywhere, Category="Sky") float MaxHorizonExp   = 3.f;
    UPROPERTY(EditAnywhere, Category="Sky") float SunSizeMin      = 0.9f;  // más chico = sol más grande
    UPROPERTY(EditAnywhere, Category="Sky") float MaxSunGlow      = 200.f;

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_TimeOfDay;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SunYaw;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SunIntensity;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_FogDensity;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_AmbientIntensity;
    // Knobs cartoon (opcionales).
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_Bands;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_HorizonExp;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SunSize;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* Slider_SunGlow;
    // Presets / utilidades (opcionales).
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_Dawn;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_Noon;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_Sunset;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_Night;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_Reset;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_Random;

    // Una rueda por color (cada una edita su color directo).
    UPROPERTY(meta=(BindWidgetOptional)) UPTColorWheelWidget* Wheel_SkyTop;
    UPROPERTY(meta=(BindWidgetOptional)) UPTColorWheelWidget* Wheel_SkyHorizon;
    UPROPERTY(meta=(BindWidgetOptional)) UPTColorWheelWidget* Wheel_SunColor;
    UPROPERTY(meta=(BindWidgetOptional)) UPTColorWheelWidget* Wheel_FogColor;
    UPROPERTY(meta=(BindWidgetOptional)) UPTColorWheelWidget* Wheel_AmbientColor;

    UPROPERTY(meta=(BindWidgetOptional)) UButton* CloseButton;

    UFUNCTION() void OnAnyChanged(float Value);      // sliders escalares → reconstruir + aplicar
    UFUNCTION() void OnCloseClicked();
    // Un handler por rueda (setea su color y aplica).
    UFUNCTION() void OnSkyTopColor(FLinearColor C);
    UFUNCTION() void OnSkyHorizonColor(FLinearColor C);
    UFUNCTION() void OnSunColorChanged(FLinearColor C);
    UFUNCTION() void OnFogColorChanged(FLinearColor C);
    UFUNCTION() void OnAmbientColorChanged(FLinearColor C);
    UFUNCTION() void OnDawn();
    UFUNCTION() void OnNoon();
    UFUNCTION() void OnSunset();
    UFUNCTION() void OnNight();
    UFUNCTION() void OnReset();
    UFUNCTION() void OnRandom();

private:
    class APTMapEnvironment* FindEnv() const;
    void PopulateFromEnv();     // carga sliders + inicializa cada rueda desde los settings
    void ApplyFromSliders();    // reconstruye los ESCALARES desde los sliders y aplica
    void ApplyPreset(const FPTSkySettings& S); // setea todo un preset + refresca la UI
    bool bLoadingUI = false;
};
