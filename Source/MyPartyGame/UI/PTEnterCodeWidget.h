// Copyright Epic Games, Inc. All Rights Reserved.
// Pantalla propia "Enter Code" (separada de Find Game): pegar un código de invitación y unirse.
// El error ("código no válido o sala no encontrada") se muestra en el ErrorText del Main Menu,
// igual que el resto de los sub-paneles — ver UPTMainMenuWidget::OnJoinSession.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTEnterCodeWidget.generated.h"

class UButton;
class UEditableTextBox;
class UMultiplayerSessionsSubsystem;

UCLASS()
class MYPARTYGAME_API UPTEnterCodeWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "Sessions")
    void ShowPanel();

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget)) UEditableTextBox* CodeInput;
    UPROPERTY(meta = (BindWidget)) UButton*          JoinButton;
    UPROPERTY(meta = (BindWidget)) UButton*          BackButton;

    UFUNCTION() void OnJoinClicked();
    UFUNCTION() void OnBackClicked();

private:
    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
};
