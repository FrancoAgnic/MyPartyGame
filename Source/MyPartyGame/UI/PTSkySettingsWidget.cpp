// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSkySettingsWidget.h"
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

    // Enganchar TODOS los sliders al mismo handler.
    USlider* All[] = {
        Slider_TimeOfDay, Slider_SunYaw, Slider_SunIntensity, Slider_FogDensity, Slider_AmbientIntensity,
        Slider_SkyTop_R, Slider_SkyTop_G, Slider_SkyTop_B,
        Slider_SkyHorizon_R, Slider_SkyHorizon_G, Slider_SkyHorizon_B,
        Slider_SunColor_R, Slider_SunColor_G, Slider_SunColor_B,
        Slider_FogColor_R, Slider_FogColor_G, Slider_FogColor_B,
        Slider_AmbientColor_R, Slider_AmbientColor_G, Slider_AmbientColor_B
    };
    for (USlider* S : All)
        if (S) S->OnValueChanged.AddDynamic(this, &UPTSkySettingsWidget::OnAnyChanged);

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
    PopulateFromEnv();
    SetVisibility(ESlateVisibility::Visible);
    PlayPopIn();
}

void UPTSkySettingsWidget::HidePanel()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTSkySettingsWidget::OnCloseClicked() { HidePanel(); }

void UPTSkySettingsWidget::PopulateFromEnv()
{
    const APTMapEnvironment* Env = FindEnv();
    if (!Env) return;
    const FPTSkySettings S = Env->GetSkySettings();

    bLoadingUI = true; // no re-aplicar mientras seteo los sliders
    SetSV(Slider_TimeOfDay,       S.TimeOfDay);
    SetSV(Slider_SunYaw,          MaxSunYaw       > 0 ? S.SunYaw       / MaxSunYaw       : 0.f);
    SetSV(Slider_SunIntensity,    MaxSunIntensity > 0 ? S.SunIntensity / MaxSunIntensity : 0.f);
    SetSV(Slider_FogDensity,      MaxFogDensity   > 0 ? S.FogDensity   / MaxFogDensity   : 0.f);
    SetSV(Slider_AmbientIntensity,MaxAmbient      > 0 ? S.AmbientIntensity / MaxAmbient  : 0.f);

    SetSV(Slider_SkyTop_R, S.SkyTopColor.R);       SetSV(Slider_SkyTop_G, S.SkyTopColor.G);       SetSV(Slider_SkyTop_B, S.SkyTopColor.B);
    SetSV(Slider_SkyHorizon_R, S.SkyHorizonColor.R); SetSV(Slider_SkyHorizon_G, S.SkyHorizonColor.G); SetSV(Slider_SkyHorizon_B, S.SkyHorizonColor.B);
    SetSV(Slider_SunColor_R, S.SunColor.R);         SetSV(Slider_SunColor_G, S.SunColor.G);         SetSV(Slider_SunColor_B, S.SunColor.B);
    SetSV(Slider_FogColor_R, S.FogColor.R);         SetSV(Slider_FogColor_G, S.FogColor.G);         SetSV(Slider_FogColor_B, S.FogColor.B);
    SetSV(Slider_AmbientColor_R, S.AmbientColor.R); SetSV(Slider_AmbientColor_G, S.AmbientColor.G); SetSV(Slider_AmbientColor_B, S.AmbientColor.B);
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
    FPTSkySettings S = Env->GetSkySettings(); // partir de lo actual → canales de color sin slider quedan igual

    S.TimeOfDay        = SVal(Slider_TimeOfDay, S.TimeOfDay);
    S.SunYaw           = SVal(Slider_SunYaw, S.SunYaw / (MaxSunYaw > 0 ? MaxSunYaw : 1.f)) * MaxSunYaw;
    S.SunIntensity     = SVal(Slider_SunIntensity, 0.f) * MaxSunIntensity;
    S.FogDensity       = SVal(Slider_FogDensity, 0.f) * MaxFogDensity;
    S.AmbientIntensity = SVal(Slider_AmbientIntensity, 0.f) * MaxAmbient;

    auto Col = [](USlider* R, USlider* G, USlider* B, FLinearColor Cur)
    {
        return FLinearColor(SVal(R, Cur.R), SVal(G, Cur.G), SVal(B, Cur.B), 1.f);
    };
    S.SkyTopColor     = Col(Slider_SkyTop_R,     Slider_SkyTop_G,     Slider_SkyTop_B,     S.SkyTopColor);
    S.SkyHorizonColor = Col(Slider_SkyHorizon_R, Slider_SkyHorizon_G, Slider_SkyHorizon_B, S.SkyHorizonColor);
    S.SunColor        = Col(Slider_SunColor_R,   Slider_SunColor_G,   Slider_SunColor_B,   S.SunColor);
    S.FogColor        = Col(Slider_FogColor_R,   Slider_FogColor_G,   Slider_FogColor_B,   S.FogColor);
    S.AmbientColor    = Col(Slider_AmbientColor_R,Slider_AmbientColor_G,Slider_AmbientColor_B,S.AmbientColor);

    Env->SetSkySettings(S); // aplica en vivo
    UpdateSwatches(S);
}

void UPTSkySettingsWidget::UpdateSwatches(const FPTSkySettings& S)
{
    if (Swatch_SkyTop)      Swatch_SkyTop->SetColorAndOpacity(S.SkyTopColor);
    if (Swatch_SkyHorizon)  Swatch_SkyHorizon->SetColorAndOpacity(S.SkyHorizonColor);
    if (Swatch_SunColor)    Swatch_SunColor->SetColorAndOpacity(S.SunColor);
    if (Swatch_FogColor)    Swatch_FogColor->SetColorAndOpacity(S.FogColor);
    if (Swatch_AmbientColor)Swatch_AmbientColor->SetColorAndOpacity(S.AmbientColor);
}
