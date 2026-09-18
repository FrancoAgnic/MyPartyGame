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

    USlider* Scalars[] = { Slider_TimeOfDay, Slider_SunYaw, Slider_SunIntensity, Slider_FogDensity, Slider_AmbientIntensity,
                           Slider_Bands, Slider_HorizonExp, Slider_SunSize, Slider_SunGlow };
    for (USlider* S : Scalars)
        if (S) S->OnValueChanged.AddDynamic(this, &UPTSkySettingsWidget::OnAnyChanged);

    if (Btn_Dawn)   Btn_Dawn->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnDawn);
    if (Btn_Noon)   Btn_Noon->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnNoon);
    if (Btn_Sunset) Btn_Sunset->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnSunset);
    if (Btn_Night)  Btn_Night->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnNight);
    if (Btn_Reset)  Btn_Reset->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnReset);
    if (Btn_Random) Btn_Random->OnClicked.AddDynamic(this, &UPTSkySettingsWidget::OnRandom);

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
    SetSV(Slider_Bands,      MaxBands      > 0 ? S.Bands / MaxBands : 0.f);
    SetSV(Slider_HorizonExp, MaxHorizonExp > 0 ? S.HorizonExp / MaxHorizonExp : 0.f);
    SetSV(Slider_SunSize,    (1.f - SunSizeMin) > 0 ? (S.SunSize - SunSizeMin) / (1.f - SunSizeMin) : 0.f);
    SetSV(Slider_SunGlow,    MaxSunGlow    > 0 ? S.SunGlow / MaxSunGlow : 0.f);
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
    if (Slider_Bands)      S.Bands      = SVal(Slider_Bands, 0.f) * MaxBands;
    if (Slider_HorizonExp) S.HorizonExp = SVal(Slider_HorizonExp, 0.f) * MaxHorizonExp;
    if (Slider_SunSize)    S.SunSize    = SunSizeMin + SVal(Slider_SunSize, 0.f) * (1.f - SunSizeMin);
    if (Slider_SunGlow)    S.SunGlow    = SVal(Slider_SunGlow, 0.f) * MaxSunGlow;

    Env->SetSkySettings(S); // aplica en vivo
}

// ── Presets / utilidades ──
void UPTSkySettingsWidget::ApplyPreset(const FPTSkySettings& S)
{
    if (APTMapEnvironment* Env = FindEnv()) { Env->SetSkySettings(S); PopulateFromEnv(); }
}

void UPTSkySettingsWidget::OnDawn()
{
    FPTSkySettings S;
    S.TimeOfDay = 0.12f; S.SunYaw = 90.f;
    S.SkyTopColor = FLinearColor(0.20f, 0.30f, 0.60f);
    S.SkyHorizonColor = FLinearColor(1.00f, 0.60f, 0.45f);
    S.SunColor = FLinearColor(1.00f, 0.75f, 0.55f); S.SunIntensity = 2.5f;
    S.FogColor = FLinearColor(1.00f, 0.70f, 0.55f); S.FogDensity = 0.03f;
    S.AmbientColor = FLinearColor(0.45f, 0.45f, 0.60f); S.AmbientIntensity = 1.0f;
    ApplyPreset(S);
}

void UPTSkySettingsWidget::OnNoon()
{
    FPTSkySettings S; // los defaults del struct ya son un mediodía lindo
    S.TimeOfDay = 0.5f; S.SunIntensity = 4.0f;
    ApplyPreset(S);
}

void UPTSkySettingsWidget::OnSunset()
{
    FPTSkySettings S;
    S.TimeOfDay = 0.88f; S.SunYaw = 270.f;
    S.SkyTopColor = FLinearColor(0.25f, 0.20f, 0.45f);
    S.SkyHorizonColor = FLinearColor(1.00f, 0.45f, 0.30f);
    S.SunColor = FLinearColor(1.00f, 0.55f, 0.35f); S.SunIntensity = 2.5f;
    S.FogColor = FLinearColor(0.95f, 0.55f, 0.40f); S.FogDensity = 0.03f;
    S.AmbientColor = FLinearColor(0.45f, 0.35f, 0.45f); S.AmbientIntensity = 0.9f;
    ApplyPreset(S);
}

void UPTSkySettingsWidget::OnNight()
{
    FPTSkySettings S;
    S.TimeOfDay = 0.5f; S.SunYaw = 0.f;
    S.SkyTopColor = FLinearColor(0.02f, 0.03f, 0.10f);
    S.SkyHorizonColor = FLinearColor(0.06f, 0.09f, 0.20f);
    S.SunColor = FLinearColor(0.30f, 0.35f, 0.55f); S.SunIntensity = 0.4f;
    S.FogColor = FLinearColor(0.05f, 0.07f, 0.15f); S.FogDensity = 0.04f;
    S.AmbientColor = FLinearColor(0.10f, 0.13f, 0.25f); S.AmbientIntensity = 0.6f;
    ApplyPreset(S);
}

void UPTSkySettingsWidget::OnReset() { ApplyPreset(FPTSkySettings()); } // defaults del struct

void UPTSkySettingsWidget::OnRandom()
{
    FPTSkySettings S = FindEnv() ? FindEnv()->GetSkySettings() : FPTSkySettings();
    auto RandCol = [](float MinV) { return FLinearColor(FMath::FRand() * 360.f, FMath::FRandRange(0.4f, 1.f),
                                                        FMath::FRandRange(MinV, 1.f), 1.f).HSVToLinearRGB(); };
    S.TimeOfDay       = FMath::FRand();
    S.SunYaw          = FMath::FRand() * 360.f;
    S.SkyTopColor     = RandCol(0.4f);
    S.SkyHorizonColor = RandCol(0.6f);
    S.SunColor        = RandCol(0.7f);
    S.FogColor        = RandCol(0.5f);
    S.AmbientColor    = RandCol(0.4f);
    ApplyPreset(S);
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
