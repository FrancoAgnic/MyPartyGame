// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSkySettingsWidget.h"
#include "PTColorWheelWidget.h"
#include "Components/Slider.h"
#include "Components/Image.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

namespace
{
    float SVal(USlider* S, float Def) { return S ? S->GetValue() : Def; }
    void  SetSV(USlider* S, float V)  { if (S) S->SetValue(V); }
}

void UPTSkySettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    USlider* Scalars[] = { Slider_TimeOfDay, Slider_SunYaw, Slider_SunIntensity, Slider_FogDensity, Slider_AmbientIntensity };
    for (USlider* S : Scalars)
        if (S) S->OnValueChanged.AddDynamic(this, &UPTSkySettingsWidget::OnAnyChanged);

    if (Btn_SkyTop)      Btn_SkyTop->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnPickSkyTop);
    if (Btn_SkyHorizon)  Btn_SkyHorizon->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnPickSkyHorizon);
    if (Btn_SunColor)    Btn_SunColor->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnPickSun);
    if (Btn_FogColor)    Btn_FogColor->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnPickFog);
    if (Btn_AmbientColor)Btn_AmbientColor->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnPickAmbient);

    if (ColorWheel) ColorWheel->OnColorChanged.AddDynamic(this, &UPTSkySettingsWidget::OnWheelColorChanged);
    if (ColorWheel) ColorWheel->SetVisibility(ESlateVisibility::Collapsed); // se muestra al elegir un color

    if (CloseButton) CloseButton->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnCloseClicked);
    SetVisibility(ESlateVisibility::Collapsed);
}

APTMapEnvironment* UPTSkySettingsWidget::FindEnv() const
{
    return GetWorld() ? Cast<APTMapEnvironment>(
        UGameplayStatics::GetActorOfClass(GetWorld(), APTMapEnvironment::StaticClass())) : nullptr;
}

void UPTSkySettingsWidget::ShowPanel()
{
    ActiveTarget = EPTSkyColorTarget::None;
    if (ColorWheel) ColorWheel->SetVisibility(ESlateVisibility::Collapsed);
    PopulateFromEnv();
    SetVisibility(ESlateVisibility::Visible);
    PlayPopIn();
}

void UPTSkySettingsWidget::HidePanel() { SetVisibility(ESlateVisibility::Collapsed); }
void UPTSkySettingsWidget::OnCloseClicked() { HidePanel(); }

void UPTSkySettingsWidget::PopulateFromEnv()
{
    const APTMapEnvironment* Env = FindEnv();
    if (!Env) return;
    const FPTSkySettings S = Env->GetSkySettings();

    bLoadingUI = true;
    SetSV(Slider_TimeOfDay,        S.TimeOfDay);
    SetSV(Slider_SunYaw,           MaxSunYaw       > 0 ? S.SunYaw           / MaxSunYaw       : 0.f);
    SetSV(Slider_SunIntensity,     MaxSunIntensity > 0 ? S.SunIntensity     / MaxSunIntensity : 0.f);
    SetSV(Slider_FogDensity,       MaxFogDensity   > 0 ? S.FogDensity       / MaxFogDensity   : 0.f);
    SetSV(Slider_AmbientIntensity, MaxAmbient      > 0 ? S.AmbientIntensity / MaxAmbient      : 0.f);
    bLoadingUI = false;

    UpdateSwatches(S);
}

void UPTSkySettingsWidget::OnAnyChanged(float /*Value*/)
{
    if (bLoadingUI) return;
    ApplyFromSliders();
}

void UPTSkySettingsWidget::ApplyFromSliders()
{
    APTMapEnvironment* Env = FindEnv();
    if (!Env) return;
    FPTSkySettings S = Env->GetSkySettings(); // partir de lo actual (colores intactos)

    S.TimeOfDay        = SVal(Slider_TimeOfDay, S.TimeOfDay);
    S.SunYaw           = SVal(Slider_SunYaw, 0.f) * MaxSunYaw;
    S.SunIntensity     = SVal(Slider_SunIntensity, 0.f) * MaxSunIntensity;
    S.FogDensity       = SVal(Slider_FogDensity, 0.f) * MaxFogDensity;
    S.AmbientIntensity = SVal(Slider_AmbientIntensity, 0.f) * MaxAmbient;

    Env->SetSkySettings(S); // aplica en vivo
}

void UPTSkySettingsWidget::UpdateSwatches(const FPTSkySettings& S)
{
    if (Swatch_SkyTop)      Swatch_SkyTop->SetColorAndOpacity(S.SkyTopColor);
    if (Swatch_SkyHorizon)  Swatch_SkyHorizon->SetColorAndOpacity(S.SkyHorizonColor);
    if (Swatch_SunColor)    Swatch_SunColor->SetColorAndOpacity(S.SunColor);
    if (Swatch_FogColor)    Swatch_FogColor->SetColorAndOpacity(S.FogColor);
    if (Swatch_AmbientColor)Swatch_AmbientColor->SetColorAndOpacity(S.AmbientColor);
}

// ── Elegir qué color se edita con la rueda ──
void UPTSkySettingsWidget::OnPickSkyTop()     { OpenWheelFor(EPTSkyColorTarget::SkyTop); }
void UPTSkySettingsWidget::OnPickSkyHorizon() { OpenWheelFor(EPTSkyColorTarget::SkyHorizon); }
void UPTSkySettingsWidget::OnPickSun()        { OpenWheelFor(EPTSkyColorTarget::Sun); }
void UPTSkySettingsWidget::OnPickFog()        { OpenWheelFor(EPTSkyColorTarget::Fog); }
void UPTSkySettingsWidget::OnPickAmbient()    { OpenWheelFor(EPTSkyColorTarget::Ambient); }

void UPTSkySettingsWidget::OpenWheelFor(EPTSkyColorTarget Target)
{
    ActiveTarget = Target;
    const APTMapEnvironment* Env = FindEnv();
    if (!Env || !ColorWheel) return;
    const FPTSkySettings S = Env->GetSkySettings();
    FLinearColor Cur = FLinearColor::White;
    switch (Target)
    {
    case EPTSkyColorTarget::SkyTop:     Cur = S.SkyTopColor;     break;
    case EPTSkyColorTarget::SkyHorizon: Cur = S.SkyHorizonColor; break;
    case EPTSkyColorTarget::Sun:        Cur = S.SunColor;        break;
    case EPTSkyColorTarget::Fog:        Cur = S.FogColor;        break;
    case EPTSkyColorTarget::Ambient:    Cur = S.AmbientColor;    break;
    default: break;
    }
    ColorWheel->SetVisibility(ESlateVisibility::Visible);
    ColorWheel->OpenWith(Cur);
}

void UPTSkySettingsWidget::OnWheelColorChanged(FLinearColor Color)
{
    if (ActiveTarget == EPTSkyColorTarget::None) return;
    APTMapEnvironment* Env = FindEnv();
    if (!Env) return;
    FPTSkySettings S = Env->GetSkySettings();
    switch (ActiveTarget)
    {
    case EPTSkyColorTarget::SkyTop:     S.SkyTopColor     = Color; break;
    case EPTSkyColorTarget::SkyHorizon: S.SkyHorizonColor = Color; break;
    case EPTSkyColorTarget::Sun:        S.SunColor        = Color; break;
    case EPTSkyColorTarget::Fog:        S.FogColor        = Color; break;
    case EPTSkyColorTarget::Ambient:    S.AmbientColor    = Color; break;
    default: return;
    }
    Env->SetSkySettings(S); // aplica en vivo
    UpdateSwatches(S);
}
