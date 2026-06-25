// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 2 — Panel para ingresar nombre, contraseña y máx. jugadores al crear sesión.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTCreateSessionWidget.generated.h"

class UButton;
class UEditableTextBox;
class USpinBox;
class UCheckBox;
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

    UPROPERTY(meta = (BindWidget)) UEditableTextBox* NameInput;
    // Fase 5 — reemplaza al viejo campo de contraseña libre: si está marcado, el subsistema
    // genera un código aleatorio (ver UMultiplayerSessionsSubsystem::GetGeneratedSessionCode).
    UPROPERTY(meta = (BindWidget)) UCheckBox*        PrivateCheckbox;
    UPROPERTY(meta = (BindWidget)) USpinBox*         MaxPlayersInput;
    UPROPERTY(meta = (BindWidget)) UButton*          ConfirmButton;
    UPROPERTY(meta = (BindWidget)) UButton*          BackButton;

    UFUNCTION() void OnConfirmClicked();
    UFUNCTION() void OnBackClicked();

private:
    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
};
