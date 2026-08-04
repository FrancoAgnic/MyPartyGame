// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasGameMode.h"
#include "SillasGameState.h"
#include "SillasPlayerController.h"
#include "SillasPlayerState.h"
#include "UObject/ConstructorHelpers.h"

ASillasGameMode::ASillasGameMode()
{
    GameStateClass        = ASillasGameState::StaticClass();
    PlayerControllerClass = ASillasPlayerController::StaticClass();
    PlayerStateClass      = ASillasPlayerState::StaticClass();

    // Simétrico con PTLobbyGameMode: el viaje lobby → arena (y arena → lobby al
    // terminar el match) es seamless para no tirar la sesión Steam.
    bUseSeamlessTravel = true;

    // FASE 0 — pawn placeholder para el criterio de hecho (3 clientes caminando
    // por el greybox). En Fase 1 se reemplaza por posesión de pawns Silla/Cazador
    // según rol y esto pasa a nullptr.
    static ConstructorHelpers::FClassFinder<APawn> PlaceholderPawn(
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    if (PlaceholderPawn.Succeeded())
    {
        DefaultPawnClass = PlaceholderPawn.Class;
    }
}
