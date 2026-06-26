// Copyright Epic Games, Inc. All Rights Reserved.
// Popup de Settings (sonido/idioma/gráficos). Reusable desde el Main Menu y, más adelante,
// desde el menú de Escape del Lobby — ver UPTGameUserSettings para la persistencia real.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTSettingsWidget.generated.h"

class USlider;
class UComboBoxString;
class UButton;

UCLASS()
class MYPARTYGAME_API UPTSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ShowPanel();

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget)) USlider*         VolumeSlider;
    UPROPERTY(meta = (BindWidget)) UComboBoxString* LanguageCombo;
    UPROPERTY(meta = (BindWidget)) UComboBoxString* GraphicsCombo;
    UPROPERTY(meta = (BindWidget)) UButton*         BackButton;

    UFUNCTION() void OnVolumeChanged(float NewValue);
    UFUNCTION() void OnLanguageChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnGraphicsChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
    UFUNCTION() void OnBackClicked();
};
