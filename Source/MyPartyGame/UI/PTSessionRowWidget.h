// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 2 — Fila de resultado de búsqueda de sesiones.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "PTSessionRowWidget.generated.h"

class UTextBlock;
class UImage;
class UButton;
class UMultiplayerSessionsSubsystem;
class UPTPasswordPromptWidget;

UCLASS()
class MYPARTYGAME_API UPTSessionRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Inicializar la fila con los datos de la sesión. Llamar desde PopulateResults. */
    void Init(const FOnlineSessionSearchResult& InResult,
              const FString& InName,
              int32 InCurrent,
              int32 InMax,
              bool  bInHasPassword);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget))         UTextBlock* NameText;
    UPROPERTY(meta = (BindWidget))         UTextBlock* PlayersText;
    UPROPERTY(meta = (BindWidget))         UImage*     LockIcon;
    UPROPERTY(meta = (BindWidget))         UButton*    JoinButton;

    /** Prompt de contraseña incrustado en la fila (opcional: puede ser widget hijo en WBP). */
    UPROPERTY(meta = (BindWidgetOptional)) UPTPasswordPromptWidget* PasswordPrompt;

    UFUNCTION() void OnJoinClicked();
    UFUNCTION() void OnPasswordConfirmedCallback(const FString& Password);

private:
    FOnlineSessionSearchResult StoredResult;
    bool bHasPassword = false;

    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
};
