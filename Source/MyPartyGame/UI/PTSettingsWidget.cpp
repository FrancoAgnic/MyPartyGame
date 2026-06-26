// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSettingsWidget.h"
#include "PTGameUserSettings.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

bool UPTSettingsWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (VolumeSlider)
    {
        VolumeSlider->SetMinValue(0.0f);
        VolumeSlider->SetMaxValue(1.0f);
        VolumeSlider->OnValueChanged.AddDynamic(this, &UPTSettingsWidget::OnVolumeChanged);
    }

    if (EnglishButton) EnglishButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnEnglishClicked);
    if (SpanishButton) SpanishButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnSpanishClicked);
    if (LowButton)     LowButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnLowClicked);
    if (MediumButton)  MediumButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnMediumClicked);
    if (HighButton)    HighButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnHighClicked);
    if (ApplyButton)   ApplyButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnApplyClicked);
    if (BackButton)    BackButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnBackClicked);

    return true;
}

void UPTSettingsWidget::ShowPanel()
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        const float Volume = Settings->GetMasterVolume();
        if (VolumeSlider)    VolumeSlider->SetValue(Volume);
        if (VolumeValueText) VolumeValueText->SetText(FText::AsNumber(FMath::RoundToInt(Volume * 100.0f)));

        OnLanguageStateChanged(Settings->GetLanguageCode() != TEXT("es"));
        OnGraphicsStateChanged(FMath::Clamp(Settings->GetGraphicsQuality(), 0, 2));
    }

    SetVisibility(ESlateVisibility::Visible);
}

void UPTSettingsWidget::OnVolumeChanged(float NewValue)
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SetMasterVolume(NewValue);
    }
    if (VolumeValueText) VolumeValueText->SetText(FText::AsNumber(FMath::RoundToInt(NewValue * 100.0f)));
}

void UPTSettingsWidget::ApplyLanguage(const FString& Code)
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SetLanguageCode(Code);
    }
    OnLanguageStateChanged(Code != TEXT("es"));
}

void UPTSettingsWidget::ApplyGraphics(int32 Quality)
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SetGraphicsQuality(Quality);
    }
    OnGraphicsStateChanged(Quality);
}

void UPTSettingsWidget::OnEnglishClicked() { ApplyLanguage(TEXT("en")); }
void UPTSettingsWidget::OnSpanishClicked() { ApplyLanguage(TEXT("es")); }
void UPTSettingsWidget::OnLowClicked()     { ApplyGraphics(0); }
void UPTSettingsWidget::OnMediumClicked()  { ApplyGraphics(1); }
void UPTSettingsWidget::OnHighClicked()    { ApplyGraphics(2); }

void UPTSettingsWidget::OnApplyClicked()
{
    // Volumen/idioma/gráficos ya se aplicaron en caliente al tocarlos; esto solo persiste a disco.
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SaveSettings();
    }
}

void UPTSettingsWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
