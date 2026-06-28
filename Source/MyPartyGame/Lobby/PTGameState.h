// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — Estado compartido replicado del lobby.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "PTGameState.generated.h"

UENUM(BlueprintType)
enum class EPTLobbyState : uint8
{
    WaitingForPlayers UMETA(DisplayName="Waiting"),
    Starting          UMETA(DisplayName="Starting"),
    InGame            UMETA(DisplayName="InGame")
};

UCLASS()
class MYPARTYGAME_API APTGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    EPTLobbyState LobbyState = EPTLobbyState::WaitingForPlayers;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    FString SessionDisplayName;

    // Vacío si la sala es pública. Replicado para que cualquier jugador (no solo el host)
    // pueda verlo y compartirlo con quien todavía no se unió.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    FString SessionCode;

    // Tope de jugadores elegido al crear la sesión (para el "4/8" del HUD del lobby).
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    int32 MaxPlayers = 0;

    // La lista de jugadores vive en PlayerArray (heredado de AGameStateBase). No replicar aparte.
};
