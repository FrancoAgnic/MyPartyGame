// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyGameMode.h"
#include "PTLobbyCharacter.h"
#include "PTLobbyPlayerController.h"
#include "PTPlayerState.h"
#include "PTGameState.h"

APTLobbyGameMode::APTLobbyGameMode()
{
    DefaultPawnClass      = APTLobbyCharacter::StaticClass();
    PlayerControllerClass = APTLobbyPlayerController::StaticClass();
    PlayerStateClass      = APTPlayerState::StaticClass();
    GameStateClass        = APTGameState::StaticClass();
    bUseSeamlessTravel    = true; // Preparado para el viaje lobby→minijuego (Fase 6).
}

void APTLobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    if (APTPlayerState* PS = NewPlayer->GetPlayerState<APTPlayerState>())
    {
        // En listen server, el controlador del host es local en el servidor.
        const bool bIsHost = NewPlayer->IsLocalController();
        PS->Server_SetHost(bIsHost);

        const FString Name = bIsHost
            ? TEXT("Host")
            : FString::Printf(TEXT("Player_%d"), PlayersJoined + 1);
        PS->Server_SetDisplayName(Name);
    }

    ++PlayersJoined;
    UE_LOG(LogTemp, Log, TEXT("[Lobby] PostLogin. Jugadores conectados: %d"), PlayersJoined);
}

void APTLobbyGameMode::Logout(AController* Exiting)
{
    --PlayersJoined;
    UE_LOG(LogTemp, Log, TEXT("[Lobby] Logout. Jugadores conectados: %d"), PlayersJoined);
    Super::Logout(Exiting);
}
