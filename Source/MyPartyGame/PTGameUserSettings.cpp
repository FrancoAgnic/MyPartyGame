// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AudioDevice.h"
#include "Internationalization/Internationalization.h"

void UPTGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();
    MasterVolume = 1.0f;
    LanguageCode = TEXT("en");
}

UPTGameUserSettings* UPTGameUserSettings::Get()
{
    return Cast<UPTGameUserSettings>(GEngine->GetGameUserSettings());
}

void UPTGameUserSettings::SetMasterVolume(float InVolume)
{
    MasterVolume = FMath::Clamp(InVolume, 0.0f, 1.0f);

    if (GEngine)
    {
        if (FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice())
        {
            AudioDevice->SetTransientPrimaryVolume(MasterVolume);
        }
    }
}

void UPTGameUserSettings::SetLanguageCode(const FString& InLanguageCode)
{
    LanguageCode = InLanguageCode;
    FInternationalization::Get().SetCurrentCulture(LanguageCode);
}

int32 UPTGameUserSettings::GetGraphicsQuality() const
{
    return GetOverallScalabilityLevel();
}

void UPTGameUserSettings::SetGraphicsQuality(int32 InQuality)
{
    SetOverallScalabilityLevel(FMath::Clamp(InQuality, 0, 3));
    ApplySettings(false);
}

void UPTGameUserSettings::ApplyAudioAndLanguage(UWorld* World)
{
    if (GEngine)
    {
        if (FAudioDeviceHandle AudioDevice = GEngine->GetMainAudioDevice())
        {
            AudioDevice->SetTransientPrimaryVolume(MasterVolume);
        }
    }
    FInternationalization::Get().SetCurrentCulture(LanguageCode);
}
