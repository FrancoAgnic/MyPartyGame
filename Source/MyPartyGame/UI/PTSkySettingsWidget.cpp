// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSkySettingsWidget.h"
#include "PTColorWheelWidget.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

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

    if (Wheel_SkyTop)      Wheel_SkyTop->OnColorChanged.AddDynamic(this, &UPTSkySettingsWidget::OnSkyTopColor);
    if (Wheel_SkyHorizon)  Wheel_SkyHorizon->OnColorChanged.AddDynamic(this, &UPTSkySettingsWidget::OnSkyHorizonColor);
    if (Wheel_SunColor)    Wheel_SunColor->OnColorChanged.AddDynamic(this, &UPTSkySettingsWidget::OnSunColorChanged);
    if (Wheel_FogColor)    Wheel_FogColor->OnColorChanged.AddDynamic(this, &UPTSkySettingsWidget::OnFogColorChanged);
    if (Wheel_AmbientColor)Wheel_AmbientColor->OnColorChanged.AddDynamic(this, &UPTSkySettingsWidget::OnAmbientColorChanged);

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
    // Dar foco + mouse al panel (al abrir con F el juego tenía el foco y no había cursor).
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI M;
        M.SetWidgetToFocus(TakeWidget());
        M.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(M);
        PC->SetShowMouseCursor(true);
    }
}

void UPTSkySettingsWidget::HidePanel()
{
    SetVisibility(ESlateVisibility::Collapsed);
    // Devolver el foco al juego (esculpido) y ocultar el cursor.
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->SetShowMouseCursor(false);
    }
}
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

    // Cada rueda arranca en su color actual (OpenWith no dispara OnColorChanged → sin loop).
    if (Wheel_SkyTop)      Wheel_SkyTop->OpenWith(S.SkyTopColor);
    if (Wheel_SkyHorizon)  Wheel_SkyHorizon->OpenWith(S.SkyHorizonColor);
    if (Wheel_SunColor)    Wheel_SunColor->OpenWith(S.SunColor);
    if (Wheel_FogColor)    Wheel_FogColor->OpenWith(S.FogColor);
    if (Wheel_AmbientColor)Wheel_AmbientColor->OpenWith(S.AmbientColor);
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

// ── Cada rueda edita su color directo ──
void UPTSkySettingsWidget::OnSkyTopColor(FLinearColor C)
{
    if (bLoadingUI) return;
    if (APTMapEnvironment* Env = FindEnv()) { FPTSkySettings S = Env->GetSkySettings(); S.SkyTopColor = C; Env->SetSkySettings(S); }
}
void UPTSkySettingsWidget::OnSkyHorizonColor(FLinearColor C)
{
    if (bLoadingUI) return;
    if (APTMapEnvironment* Env = FindEnv()) { FPTSkySettings S = Env->GetSkySettings(); S.SkyHorizonColor = C; Env->SetSkySettings(S); }
}
void UPTSkySettingsWidget::OnSunColorChanged(FLinearColor C)
{
    if (bLoadingUI) return;
    if (APTMapEnvironment* Env = FindEnv()) { FPTSkySettings S = Env->GetSkySettings(); S.SunColor = C; Env->SetSkySettings(S); }
}
void UPTSkySettingsWidget::OnFogColorChanged(FLinearColor C)
{
    if (bLoadingUI) return;
    if (APTMapEnvironment* Env = FindEnv()) { FPTSkySettings S = Env->GetSkySettings(); S.FogColor = C; Env->SetSkySettings(S); }
}
void UPTSkySettingsWidget::OnAmbientColorChanged(FLinearColor C)
{
    if (bLoadingUI) return;
    if (APTMapEnvironment* Env = FindEnv()) { FPTSkySettings S = Env->GetSkySettings(); S.AmbientColor = C; Env->SetSkySettings(S); }
}
