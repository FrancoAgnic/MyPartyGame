// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLanguageSelectWidget.h"
#include "../PTGameUserSettings.h"
#include "../PTTextTable.h"
#include "Components/ComboBoxString.h"
#include "Components/Button.h"

void UPTLanguageSelectWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (LanguageCombo) LanguageCombo->OnSelectionChanged.AddDynamic(this, &UPTLanguageSelectWidget::OnComboChanged);
    if (ConfirmButton) ConfirmButton->OnClicked.AddDynamic(this, &UPTLanguageSelectWidget::OnConfirm);
    Refresh();
}

void UPTLanguageSelectWidget::Refresh()
{
    if (!LanguageCombo) return;

    FString Current = TEXT("en");
    if (const UPTGameUserSettings* S = UPTGameUserSettings::Get()) Current = S->GetLanguageCode();

    // Se rellena por código; mientras dura se ignora el OnSelectionChanged que dispara SetSelectedOption.
    bUpdatingCombo = true;
    LanguageCombo->ClearOptions();
    FString CurrentDisplay;
    for (const FPTLanguage& Lang : PTText::GetAvailableLanguages())
    {
        LanguageCombo->AddOption(Lang.DisplayName.ToString());
        if (Lang.Code == Current) CurrentDisplay = Lang.DisplayName.ToString();
    }
    if (!CurrentDisplay.IsEmpty()) LanguageCombo->SetSelectedOption(CurrentDisplay);
    bUpdatingCombo = false;
}

void UPTLanguageSelectWidget::OnComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    if (bUpdatingCombo) return;                       // cambio programático (llenado inicial)
    if (SelectionType == ESelectInfo::Direct) return; // no vino de un click del usuario
    // Preview EN VIVO: al elegir del combo se aplica la cultura de una (la UI se refresca en ese idioma).
    ApplyLanguageByDisplay(SelectedItem);
}

void UPTLanguageSelectWidget::ApplyLanguageByDisplay(const FString& Display)
{
    // El combo maneja nombres visibles; hay que volver del nombre al código ("Español" → "es").
    for (const FPTLanguage& Lang : PTText::GetAvailableLanguages())
    {
        if (Lang.DisplayName.ToString() == Display)
        {
            if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
                S->SetLanguageCode(Lang.Code); // aplica la cultura + guarda + refresca la UI
            break;
        }
    }
}

void UPTLanguageSelectWidget::OnConfirm()
{
    // Asegurar que quede aplicado el idioma seleccionado (por si nunca cambió la selección).
    if (LanguageCombo)
        ApplyLanguageByDisplay(LanguageCombo->GetSelectedOption());

    // Marcar que ya eligió: la pantalla NO vuelve a aparecer en próximos arranques.
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
        S->MarkLanguageChosen();

    OnLanguageChosen.Broadcast();
    SetVisibility(ESlateVisibility::Collapsed); // cerrar el overlay → queda el menú
}
