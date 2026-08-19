// Copyright Epic Games, Inc. All Rights Reserved.
// Pantalla de selección de idioma del PRIMER arranque. Solo aparece si el jugador todavía no
// eligió idioma (UPTGameUserSettings::HasChosenLanguage()). Se embebe en el WBP del MainMenu como
// un overlay a pantalla completa; el MainMenu la muestra/oculta según el flag.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTLanguageSelectWidget.generated.h"

class UComboBoxString;
class UButton;
class UTextBlock;

UCLASS()
class MYPARTYGAME_API UPTLanguageSelectWidget : public UPTUserWidget
{
    GENERATED_BODY()
public:
    /** Se dispara cuando el jugador confirma el idioma (por si el MainMenu quiere reaccionar). */
    DECLARE_MULTICAST_DELEGATE(FOnLanguageChosen);
    FOnLanguageChosen OnLanguageChosen;

    /** Llena el combo con los idiomas del CSV y selecciona el actual. */
    void Refresh();

protected:
    virtual void NativeConstruct() override;

    // ComboBox (String) con los idiomas del CSV. OBLIGATORIO: nombrarlo "LanguageCombo" en el WBP.
    UPROPERTY(meta = (BindWidget))         UComboBoxString* LanguageCombo = nullptr;
    // Botón "Continuar": confirma y cierra. Opcional (si no está, se confirma al elegir del combo).
    UPROPERTY(meta = (BindWidgetOptional)) UButton*         ConfirmButton = nullptr;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*      TitleText     = nullptr;

    UFUNCTION() void OnComboChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnConfirm();

private:
    bool bUpdatingCombo = false;                 // ignora el OnSelectionChanged del llenado inicial
    void ApplyLanguageByDisplay(const FString& Display);
};
