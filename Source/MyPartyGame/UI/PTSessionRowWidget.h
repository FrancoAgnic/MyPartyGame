// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 2 — Fila de resultado de búsqueda de sesiones.
// Solo se usa para la lista pública (Find Game); las privadas se unen por código, sin lista.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "PTSessionRowWidget.generated.h"

class UTextBlock;
class UButton;
class UMultiplayerSessionsSubsystem;

UCLASS()
class MYPARTYGAME_API UPTSessionRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Inicializar la fila con los datos de la sesión. Llamar desde PopulateResults. */
    void Init(const FOnlineSessionSearchResult& InResult, const FString& InName, int32 InCurrent, int32 InMax);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget)) UTextBlock* NameText;
    UPROPERTY(meta = (BindWidget)) UTextBlock* PlayersText;
    UPROPERTY(meta = (BindWidget)) UButton*    JoinButton;

    UFUNCTION() void OnJoinClicked();

private:
    FOnlineSessionSearchResult StoredResult;

    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
};
