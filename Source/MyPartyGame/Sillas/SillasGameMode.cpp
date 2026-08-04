// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasGameMode.h"
#include "SillasGameState.h"
#include "SillasPlayerController.h"
#include "SillasPlayerState.h"

ASillasGameMode::ASillasGameMode()
{
    GameStateClass        = ASillasGameState::StaticClass();
    PlayerControllerClass = ASillasPlayerController::StaticClass();
    PlayerStateClass      = ASillasPlayerState::StaticClass();

    // Simétrico con PTLobbyGameMode: el viaje lobby → arena (y arena → lobby al
    // terminar el match) es seamless para no tirar la sesión Steam.
    bUseSeamlessTravel = true;

    // Sin pawn por defecto todavía — los pawns Silla/Cazador se posesionan por rol
    // al iniciar ronda (Fase 1). El GameMode no debe spawnear un pawn genérico.
    DefaultPawnClass = nullptr;
}
