// Copyright Epic Games, Inc. All Rights Reserved.
// Panel in-game (editor de mapas) para editar el AMBIENTE: momento del día + colores del cielo + niebla.
// Mueve sliders → aplica en vivo (APTMapEnvironment::SetSkySettings). Los colores se editan con sliders
// R/G/B. Todo BindWidgetOptional: poné en el WBP solo los controles que quieras, con nombres EXACTOS.
//
// Scalars (USlider 0..1, se mapean a su rango):
//   Slider_TimeOfDay, Slider_SunYaw, Slider_SunIntensity, Slider_FogDensity, Slider_AmbientIntensity
// Colores: 5 BOTONES-swatch que abren la RUEDA de color (ColorWheel). Al elegir un color se aplica al que
//   estabas editando. Nombres EXACTOS:
//   Btn_SkyTop, Btn_SkyHorizon, Btn_SunColor, Btn_FogColor, Btn_AmbientColor  (Button)
//   Swatch_SkyTop, Swatch_SkyHorizon, Swatch_SunColor, Swatch_FogColor, Swatch_AmbientColor (Image, se tiñen)
//   ColorWheel (UPTColorWheelWidget)  → la rueda + 2 sliders (sat/valor); editás el color activo
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

// Qué color estás editando con la rueda.
UENUM()
enum class EPTSkyColorTarget : uint8 { None, SkyTop, SkyHorizon, Sun, Fog, Ambient };

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

    // Colores: botones-swatch que abren la rueda + swatches que muestran el color.
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_SkyTop;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_SkyHorizon;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_SunColor;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_FogColor;
    UPROPERTY(meta=(BindWidgetOptional)) UButton* Btn_AmbientColor;

    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_SkyTop;
    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_SkyHorizon;
    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_SunColor;
    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_FogColor;
    UPROPERTY(meta=(BindWidgetOptional)) UImage* Swatch_AmbientColor;

    UPROPERTY(meta=(BindWidgetOptional)) UPTColorWheelWidget* ColorWheel; // rueda + 2 sliders (sat/valor)
    UPROPERTY(meta=(BindWidgetOptional)) UButton* CloseButton;

    UFUNCTION() void OnAnyChanged(float Value);      // sliders escalares → reconstruir + aplicar
    UFUNCTION() void OnCloseClicked();
    UFUNCTION() void OnPickSkyTop();
    UFUNCTION() void OnPickSkyHorizon();
    UFUNCTION() void OnPickSun();
    UFUNCTION() void OnPickFog();
    UFUNCTION() void OnPickAmbient();
    UFUNCTION() void OnWheelColorChanged(FLinearColor Color); // la rueda cambió el color activo

private:
    class APTMapEnvironment* FindEnv() const;
    void PopulateFromEnv();     // carga los sliders/swatches desde los settings actuales
    void ApplyFromSliders();    // reconstruye los ESCALARES desde los sliders y aplica
    void UpdateSwatches(const FPTSkySettings& S);
    void OpenWheelFor(EPTSkyColorTarget Target); // abre la rueda para editar ese color
    bool bLoadingUI = false;
    EPTSkyColorTarget ActiveTarget = EPTSkyColorTarget::None;
};
