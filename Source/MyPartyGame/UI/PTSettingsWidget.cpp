// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSettingsWidget.h"
#include "PTGameUserSettings.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"

namespace
{
    const TArray<FString> GraphicsLabels = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Epic") };

    FString LanguageCodeToLabel(const FString& Code) { return Code == TEXT("es") ? TEXT("Español") : TEXT("English"); }
    FString LanguageLabelToCode(const FString& Label) { return Label == TEXT("Español") ? TEXT("es") : TEXT("en"); }
}

bool UPTSettingsWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (VolumeSlider)
    {
        VolumeSlider->SetMinValue(0.0f);
        VolumeSlider->SetMaxValue(1.0f);
        VolumeSlider->OnValueChanged.AddDynamic(this, &UPTSettingsWidget::OnVolumeChanged);
    }

    if (LanguageCombo)
    {
        LanguageCombo->ClearOptions();
        LanguageCombo->AddOption(TEXT("English"));
        LanguageCombo->AddOption(TEXT("Español"));
        LanguageCombo->OnSelectionChanged.AddDynamic(this, &UPTSettingsWidget::OnLanguageChanged);
    }

    if (GraphicsCombo)
    {
        GraphicsCombo->ClearOptions();
        for (const FString& Label : GraphicsLabels) GraphicsCombo->AddOption(Label);
        GraphicsCombo->OnSelectionChanged.AddDynamic(this, &UPTSettingsWidget::OnGraphicsChanged);
    }

    if (BackButton) BackButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnBackClicked);

    return true;
}

void UPTSettingsWidget::ShowPanel()
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        if (VolumeSlider)   VolumeSlider->SetValue(Settings->GetMasterVolume());
        if (LanguageCombo)  LanguageCombo->SetSelectedOption(LanguageCodeToLabel(Settings->GetLanguageCode()));
        if (GraphicsCombo)
        {
            const int32 Quality = FMath::Clamp(Settings->GetGraphicsQuality(), 0, GraphicsLabels.Num() - 1);
            GraphicsCombo->SetSelectedOption(GraphicsLabels[Quality]);
        }
    }

    SetVisibility(ESlateVisibility::Visible);
}

void UPTSettingsWidget::OnVolumeChanged(float NewValue)
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SetMasterVolume(NewValue);
    }
}

void UPTSettingsWidget::OnLanguageChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SetLanguageCode(LanguageLabelToCode(SelectedItem));
    }
}

void UPTSettingsWidget::OnGraphicsChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        const int32 Quality = GraphicsLabels.IndexOfByKey(SelectedItem);
        if (Quality != INDEX_NONE) Settings->SetGraphicsQuality(Quality);
    }
}

void UPTSettingsWidget::OnBackClicked()
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SaveSettings();
    }

    SetVisibility(ESlateVisibility::Collapsed);
}
