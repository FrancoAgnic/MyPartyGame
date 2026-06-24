// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyPlayerController.h"
#include "EnhancedInputSubsystems.h"

void APTLobbyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // Solo el controlador local necesita el contexto de input.
    if (IsLocalController())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            if (LobbyMappingContext)
                Subsystem->AddMappingContext(LobbyMappingContext, 0);
        }

        // La Fase 2 dejó el input en UIOnly al salir del menú; restaurar a GameOnly en el lobby.
        SetInputMode(FInputModeGameOnly());
        SetShowMouseCursor(false);
    }
}
