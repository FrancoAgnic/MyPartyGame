// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "AudioDevice.h"
#include "Internationalization/Internationalization.h"
#include "HAL/IConsoleManager.h"
#include "PTTextTable.h"

// DEV: comando de consola para volver a probar la pantalla de idioma sin cerrar el editor.
// Abrir la consola con ~ y tipear:  PT.ResetLanguage
// Resetea el flag y, en el próximo arranque del boot (BootLVl), vuelve a pedir idioma.
static FAutoConsoleCommand GPTResetLanguageCmd(
    TEXT("PT.ResetLanguage"),
    TEXT("Resetea el flag de idioma elegido (bLanguageChosen=false) para volver a ver la pantalla de selección de idioma en el próximo arranque del boot."),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
        {
            S->ResetLanguageChosen();
            UE_LOG(LogTemp, Log, TEXT("[PT.ResetLanguage] bLanguageChosen reseteado. Reabrí/volvé a Play el BootLVl para ver la selección de idioma."));
        }
    }));

void UPTGameUserSettings::SetToDefaults()
{
    Super::SetToDefaults();
    MasterVolume = 1.0f;
    MusicVolume  = 1.0f;
    SFXVolume    = 1.0f;
    bTypingSoundEnabled = true;
    LanguageCode = TEXT("en");
    // VSync OFF por defecto: con VSync ON hay caída/relentización de FPS en algunos equipos. El que
    // lo necesite (p.ej. jugando en una TV) lo activa en Configuración.
    SetVSyncEnabled(false);
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

void UPTGameUserSettings::MarkLanguageChosen()
{
    bLanguageChosen = true;
    SaveSettings();
}

void UPTGameUserSettings::ResetLanguageChosen()
{
    bLanguageChosen = false;
    SaveSettings();
}

void UPTGameUserSettings::SetLanguageCode(const FString& InLanguageCode)
{
    LanguageCode = InLanguageCode;
    // Ojo: esto puede devolver false si todavía no hay datos de localización para ese idioma.
    // El idioma de la PALABRA a adivinar NO depende de esto: se lee LanguageCode directamente.
    const bool bOk = FInternationalization::Get().SetCurrentCulture(LanguageCode);
    UE_LOG(LogTemp, Log, TEXT("[Lang] LanguageCode=%s  SetCurrentCulture=%s"),
           *LanguageCode, bOk ? TEXT("OK") : TEXT("FALLÓ (falta localización)"));
    SaveSettings();
    PTText::OnLanguageChanged().Broadcast(); // refrescar la UI ya construida
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
