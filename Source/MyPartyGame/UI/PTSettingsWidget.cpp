// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSettingsWidget.h"
#include "PTGameUserSettings.h"
#include "../PTGameInstance.h"
#include "Engine/World.h"
#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GenericPlatform/GenericApplication.h"
#include "GameFramework/PlayerController.h"
#include "../Lobby/PTPlayerState.h"
#include "../Sculpt/PTSculptGameState.h"
#include "../PTTextTable.h"

bool UPTSettingsWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (VolumeSlider)
    {
        VolumeSlider->SetMinValue(0.0f);
        VolumeSlider->SetMaxValue(1.0f);
        VolumeSlider->OnValueChanged.AddDynamic(this, &UPTSettingsWidget::OnVolumeChanged);
    }
    if (MusicSlider)
    {
        MusicSlider->SetMinValue(0.0f); MusicSlider->SetMaxValue(1.0f);
        MusicSlider->OnValueChanged.AddDynamic(this, &UPTSettingsWidget::OnMusicChanged);
    }
    if (SFXSlider)
    {
        SFXSlider->SetMinValue(0.0f); SFXSlider->SetMaxValue(1.0f);
        SFXSlider->OnValueChanged.AddDynamic(this, &UPTSettingsWidget::OnSFXChanged);
    }

    // Botones viejos (opcionales): siguen andando si el WBP todavía los tiene.
    if (EnglishButton) EnglishButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnEnglishClicked);
    if (SpanishButton) SpanishButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnSpanishClicked);
    if (LowButton)     LowButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnLowClicked);
    if (MediumButton)  MediumButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnMediumClicked);
    if (HighButton)    HighButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnHighClicked);
    if (ApplyButton)   ApplyButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnApplyClicked);
    if (BackButton)    BackButton->OnClicked.AddDynamic(this, &UPTSettingsWidget::OnBackClicked);
    if (VSyncCheckBox) VSyncCheckBox->OnCheckStateChanged.AddDynamic(this, &UPTSettingsWidget::OnVSyncChanged);
    if (TypingSoundCheckBox) TypingSoundCheckBox->OnCheckStateChanged.AddDynamic(this, &UPTSettingsWidget::OnTypingSoundChanged);

    if (LanguageCombo) LanguageCombo->OnSelectionChanged.AddDynamic(this, &UPTSettingsWidget::OnLanguageComboChanged);
    if (ResolutionCombo) ResolutionCombo->OnSelectionChanged.AddDynamic(this, &UPTSettingsWidget::OnResolutionComboChanged);

    return true;
}

void UPTSettingsWidget::ShowPanel()
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        const float Volume = Settings->GetMasterVolume();
        if (VolumeSlider)    VolumeSlider->SetValue(Volume);
        if (VolumeValueText) VolumeValueText->SetText(FText::AsNumber(FMath::RoundToInt(Volume * 100.0f)));

        const float MusicV = Settings->GetMusicVolume();
        const float SFXV   = Settings->GetSFXVolume();
        if (MusicSlider)    MusicSlider->SetValue(MusicV);
        if (MusicValueText) MusicValueText->SetText(FText::AsNumber(FMath::RoundToInt(MusicV * 100.0f)));
        if (SFXSlider)      SFXSlider->SetValue(SFXV);
        if (SFXValueText)   SFXValueText->SetText(FText::AsNumber(FMath::RoundToInt(SFXV * 100.0f)));

        OnLanguageStateChanged(Settings->GetLanguageCode() != TEXT("es"));
        OnGraphicsStateChanged(FMath::Clamp(Settings->GetGraphicsQuality(), 0, 2));
        if (VSyncCheckBox) VSyncCheckBox->SetIsChecked(Settings->IsVSyncEnabled());
        if (TypingSoundCheckBox) TypingSoundCheckBox->SetIsChecked(Settings->IsTypingSoundEnabled());
    }

    BuildLanguageCombo();               // llena el desplegable con los idiomas del CSV
    BuildResolutionCombo();             // llena el desplegable de resolución (Auto + soportadas)
    ApplyLanguageSectionAvailability(); // colapsa la sección si no estamos en el menú
    SetVisibility(ESlateVisibility::Visible);
    PlayPopIn();
}

void UPTSettingsWidget::BuildLanguageCombo()
{
    if (!LanguageCombo) return;

    FString Current = TEXT("es");
    if (const UPTGameUserSettings* S = UPTGameUserSettings::Get()) Current = S->GetLanguageCode();

    // Se rellena por código: mientras dura, se ignora el OnSelectionChanged que dispara
    // SetSelectedOption (si no, re-aplicaría el idioma en bucle al abrir el panel).
    bUpdatingCombo = true;

    LanguageCombo->ClearOptions();
    FString CurrentDisplay;
    // Una opción por idioma del CSV, mostrando su nombre lindo ("Español", "Deutsch"...). Agregar
    // un idioma nuevo aparece acá solo, sin tocar este código ni el Blueprint.
    for (const FPTLanguage& Lang : PTText::GetAvailableLanguages())
    {
        LanguageCombo->AddOption(Lang.DisplayName.ToString());
        if (Lang.Code == Current) CurrentDisplay = Lang.DisplayName.ToString();
    }
    if (!CurrentDisplay.IsEmpty()) LanguageCombo->SetSelectedOption(CurrentDisplay);

    bUpdatingCombo = false;
}

void UPTSettingsWidget::OnLanguageComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bUpdatingCombo) return;                          // cambio programático (al abrir), no del usuario
    if (SelectionType == ESelectInfo::Direct) return;    // idem: no vino de un click

    // El combo maneja nombres visibles; hay que volver del nombre al código ("Español" → "es").
    for (const FPTLanguage& Lang : PTText::GetAvailableLanguages())
    {
        if (Lang.DisplayName.ToString() == SelectedItem)
        {
            ApplyLanguage(Lang.Code);
            break;
        }
    }
}

FIntPoint UPTSettingsWidget::GetDesktopResolution() const
{
    FDisplayMetrics Metrics;
    FDisplayMetrics::RebuildDisplayMetrics(Metrics);
    FIntPoint Desktop(Metrics.PrimaryDisplayWidth, Metrics.PrimaryDisplayHeight);
    if (Desktop.X <= 0 || Desktop.Y <= 0) Desktop = FIntPoint(1920, 1080); // fallback defensivo
    return Desktop;
}

void UPTSettingsWidget::BuildResolutionCombo()
{
    if (!ResolutionCombo) return;

    UPTGameUserSettings* S = UPTGameUserSettings::Get();
    if (!S) return;

    const FIntPoint Desktop = GetDesktopResolution();

    // Resoluciones soportadas por el monitor (solo hasta la del escritorio: no ofrecemos más grande).
    ResolutionOptions.Reset();
    TArray<FIntPoint> Supported;
    UKismetSystemLibrary::GetSupportedFullscreenResolutions(Supported);
    for (const FIntPoint& R : Supported)
        if (R.X > 0 && R.Y > 0 && R.X <= Desktop.X && R.Y <= Desktop.Y)
            ResolutionOptions.AddUnique(R);

    // Aseguramos que la actual esté en la lista aunque el driver no la reporte.
    const FIntPoint CurrentRes = S->GetScreenResolution();
    if (CurrentRes.X > 0 && CurrentRes.Y > 0) ResolutionOptions.AddUnique(CurrentRes);
    if (ResolutionOptions.Num() == 0) ResolutionOptions.Add(Desktop);
    ResolutionOptions.Sort([](const FIntPoint& A, const FIntPoint& B){ return A.X * A.Y > B.X * B.Y; });

    const FString AutoLabel = PTText::Get(TEXT("RES_AUTO")).ToString();

    bUpdatingResCombo = true;
    ResolutionCombo->ClearOptions();
    ResolutionCombo->AddOption(AutoLabel); // opción 0 = seguir la resolución del escritorio
    for (const FIntPoint& R : ResolutionOptions)
        ResolutionCombo->AddOption(FString::Printf(TEXT("%d x %d"), R.X, R.Y));

    // Seleccionar lo que corresponde: Auto si el usuario nunca fijó una, o la resolución actual.
    if (S->IsAutoResolution())
        ResolutionCombo->SetSelectedOption(AutoLabel);
    else
        ResolutionCombo->SetSelectedOption(FString::Printf(TEXT("%d x %d"), CurrentRes.X, CurrentRes.Y));
    bUpdatingResCombo = false;
}

void UPTSettingsWidget::OnResolutionComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bUpdatingResCombo) return;                    // cambio programático (al abrir), no del usuario
    if (SelectionType == ESelectInfo::Direct) return; // idem: no vino de un click

    UPTGameUserSettings* S = UPTGameUserSettings::Get();
    if (!S) return;

    const FString AutoLabel = PTText::Get(TEXT("RES_AUTO")).ToString();

    FIntPoint TargetRes;
    if (SelectedItem == AutoLabel)
    {
        S->SetAutoResolution(true);
        TargetRes = GetDesktopResolution();
    }
    else
    {
        // Parsear "W x H".
        FString L, R;
        if (!SelectedItem.Split(TEXT("x"), &L, &R)) return;
        TargetRes = FIntPoint(FCString::Atoi(*L.TrimStartAndEnd()), FCString::Atoi(*R.TrimStartAndEnd()));
        if (TargetRes.X <= 0 || TargetRes.Y <= 0) return;
        S->SetAutoResolution(false);
    }

    S->SetScreenResolution(TargetRes);     // conserva el modo de pantalla actual
    S->ApplyResolutionSettings(false);     // aplica en caliente
    S->SaveSettings();                     // persiste (ResolutionSizeX/Y + bAutoResolution)
}

bool UPTSettingsWidget::IsInMainMenu() const
{
    // El idioma se cambia SOLO en el menú principal: adentro de una partida cambiarlo obligaría a
    // renegociar la palabra del turno con el servidor y dejaría el HUD a mitad de camino.
    const UWorld* W = GetWorld();
    if (!W) return false;
    return W->GetMapName().Contains(TEXT("MainMenu"));
}

void UPTSettingsWidget::ApplyLanguageSectionAvailability()
{
    const bool bAllowed = IsInMainMenu();

    // En partida el idioma NO se puede cambiar → la sección entera se COLAPSA (desaparece del
    // layout, no ocupa lugar). Solo se ve en el menú principal.
    const ESlateVisibility SectionVis = bAllowed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;

    if (HB_LanguagePanel)
    {
        HB_LanguagePanel->SetVisibility(SectionVis);
    }
    else
    {
        // Sin un contenedor "HB_LanguagePanel", colapsar al menos el combo y los botones viejos.
        if (LanguageCombo) LanguageCombo->SetVisibility(SectionVis);
        if (EnglishButton) EnglishButton->SetVisibility(SectionVis);
        if (SpanishButton) SpanishButton->SetVisibility(SectionVis);
    }

    // El aviso solo tendría sentido si mostráramos la sección deshabilitada; como ahora se colapsa,
    // queda oculto siempre (se deja el binding por si el BP lo quiere usar de otra forma).
    if (LanguageHintText)
        LanguageHintText->SetVisibility(ESlateVisibility::Collapsed);

    OnLanguageAvailabilityChanged(bAllowed);
}

void UPTSettingsWidget::OnVolumeChanged(float NewValue)
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SetMasterVolume(NewValue);
    }
    if (VolumeValueText) VolumeValueText->SetText(FText::AsNumber(FMath::RoundToInt(NewValue * 100.0f)));
}

void UPTSettingsWidget::OnTypingSoundChanged(bool bIsChecked)
{
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get()) { S->SetTypingSoundEnabled(bIsChecked); S->SaveSettings(); }
}

void UPTSettingsWidget::OnMusicChanged(float NewValue)
{
    if (UPTGameInstance* GI = GetWorld() ? Cast<UPTGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
        GI->SetMusicVolume(NewValue); // aplica al mix + guarda
    if (MusicValueText) MusicValueText->SetText(FText::AsNumber(FMath::RoundToInt(NewValue * 100.0f)));
}

void UPTSettingsWidget::OnSFXChanged(float NewValue)
{
    if (UPTGameInstance* GI = GetWorld() ? Cast<UPTGameInstance>(GetWorld()->GetGameInstance()) : nullptr)
        GI->SetSFXVolume(NewValue); // aplica al mix + guarda
    if (SFXValueText) SFXValueText->SetText(FText::AsNumber(FMath::RoundToInt(NewValue * 100.0f)));
}

void UPTSettingsWidget::ApplyLanguage(const FString& Code)
{
    // Doble control: los botones ya están deshabilitados fuera del menú, pero el idioma no debe
    // poder cambiarse en partida ni aunque llegue por otro camino (un BP, un rebind...).
    if (!IsInMainMenu())
    {
        UE_LOG(LogTemp, Warning, TEXT("[Settings] El idioma solo se cambia desde el menú principal."));
        return;
    }

    // Los idiomas salen del CSV: si alguien pide uno que ya no está (por ejemplo se sacó la
    // columna), no se guarda una preferencia rota — se ignora y queda el que estaba.
    if (!PTText::IsLanguageAvailable(Code))
    {
        UE_LOG(LogTemp, Warning, TEXT("[Settings] Idioma '%s' no está en UITexts.csv — se ignora."), *Code);
        return;
    }

    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SetLanguageCode(Code);
    }
    OnLanguageStateChanged(Code != TEXT("es"));

    // Si estamos en una sesión: avisar al servidor el nuevo idioma (palabra por idioma) y refrescar
    // ya la máscara local para que el HUD cambie sin esperar el próximo revelado.
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (APTPlayerState* PS = PC->GetPlayerState<APTPlayerState>())
            PS->Server_SetLanguage(Code);
        if (UWorld* W = GetWorld())
            if (APTSculptGameState* GS = W->GetGameState<APTSculptGameState>())
                GS->RefreshLocalMasked();
    }
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
    // Cerrar el panel al aplicar (antes solo se cerraba con Escape / Back).
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTSettingsWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTSettingsWidget::OnVSyncChanged(bool bIsChecked)
{
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->SetVSyncEnabled(bIsChecked);
        Settings->ApplyNonResolutionSettings(); // aplica VSync en caliente (se persiste en Apply)
    }
}
