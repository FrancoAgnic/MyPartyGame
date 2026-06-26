// Copyright Epic Games, Inc. All Rights Reserved.
// Menú de Escape del lobby: Settings (reusa UPTSettingsWidget) y Leave Game (vuelve al Main Menu).

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTLobbyEscapeMenuWidget.generated.h"

class UButton;
class UPTSettingsWidget;

UCLASS()
class MYPARTYGAME_API UPTLobbyEscapeMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Alternar abierto/cerrado. Llamar desde el PlayerController al togglear el input de Escape. */
    UFUNCTION(BlueprintCallable, Category = "Lobby")
    void ToggleMenu();

    bool IsMenuOpen() const;

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         UButton* LeaveGameButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* SettingsButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* ResumeButton;

    UPROPERTY(meta = (BindWidgetOptional)) UPTSettingsWidget* SettingsPanel;

    UFUNCTION() void OnLeaveGameClicked();
    UFUNCTION() void OnSettingsClicked();
    UFUNCTION() void OnResumeClicked();
};
