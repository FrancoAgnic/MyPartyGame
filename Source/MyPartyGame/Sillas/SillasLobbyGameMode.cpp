// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasLobbyGameMode.h"
#include "PTGameState.h"
#include "TimerManager.h"

void ASillasLobbyGameMode::BeginPlay()
{
    Super::BeginPlay();

    GetWorldTimerManager().SetTimer(
        StartCheckHandle, this, &ASillasLobbyGameMode::CheckStartRequested,
        0.25f, /*bLoop=*/true);
}

void ASillasLobbyGameMode::CheckStartRequested()
{
    if (bTravelStarted) return;

    const APTGameState* PTGS = GetGameState<APTGameState>();
    if (!PTGS || PTGS->LobbyState != EPTLobbyState::Starting) return;

    bTravelStarted = true;
    GetWorldTimerManager().ClearTimer(StartCheckHandle);

    // Seamless (bUseSeamlessTravel viene del padre): los clientes viajan con el
    // servidor sin reconectar, la sesión Steam sigue viva. Sin "?listen": eso
    // solo hace falta en el primer viaje que LEVANTA el listen server (menú→lobby).
    UE_LOG(LogTemp, Log, TEXT("[SillasLobby] Start del host: viajando a %s"), *ArenaMapPath);
    GetWorld()->ServerTravel(ArenaMapPath);
}
