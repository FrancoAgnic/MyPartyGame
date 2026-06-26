// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 2/5/Mockup — Panel para elegir privacidad y máx. jugadores al crear sesión.
// El nombre de la sala NO se pide aquí: se autogenera del nombre de Steam del host.
// Max Players es un stepper [-] N [+] (no un SpinBox) para matchear el diseño final.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTCreateSessionWidget.generated.h"

class UButton;
class UCheckBox;
class UTextBlock;
class UMultiplayerSessionsSubsystem;

UCLASS()
class MYPARTYGAME_API UPTCreateSessionWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Mostrar el panel con el valor por defecto de jugadores. */
    UFUNCTION(BlueprintCallable, Category = "Sessions")
    void ShowPanel(int32 DefaultMaxPlayers = 4);

protected:
    virtual bool Initialize() override;

    // Fase 5 — si está marcado, el subsistema genera un código aleatorio
    // (ver UMultiplayerSessionsSubsystem::GetGeneratedSessionCode).
    UPROPERTY(meta = (BindWidget)) UCheckBox*  PrivateCheckbox;
    UPROPERTY(meta = (BindWidget)) UButton*    MinusButton;
    UPROPERTY(meta = (BindWidget)) UButton*    PlusButton;
    UPROPERTY(meta = (BindWidget)) UTextBlock* MaxPlayersText;
    UPROPERTY(meta = (BindWidget)) UButton*    ConfirmButton;
    UPROPERTY(meta = (BindWidget)) UButton*    BackButton;

    UFUNCTION() void OnMinusClicked();
    UFUNCTION() void OnPlusClicked();
    UFUNCTION() void OnConfirmClicked();
    UFUNCTION() void OnBackClicked();

private:
    void RefreshMaxPlayersText();

    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
    int32 CurrentMaxPlayers = 4;
};
