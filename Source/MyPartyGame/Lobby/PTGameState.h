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

    UPROPERTY(ReplicatedUsing=OnRep_LobbyState, BlueprintReadOnly, Category="Lobby")
    EPTLobbyState LobbyState = EPTLobbyState::WaitingForPlayers;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    FString SessionDisplayName;

    UFUNCTION()
    void OnRep_LobbyState();

    // La lista de jugadores vive en PlayerArray (heredado de AGameStateBase). No replicar aparte.
};
