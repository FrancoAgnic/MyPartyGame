// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

ASillasPlayerController::ASillasPlayerController()
{
    // Assets de input del template (en /Game/Input, NO /Game/ThirdPerson/Input).
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCDefault(
        TEXT("/Game/Input/IMC_Default.IMC_Default"));
    if (IMCDefault.Succeeded()) DefaultMappingContexts.Add(IMCDefault.Object);

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCMouse(
        TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
    if (IMCMouse.Succeeded()) DefaultMappingContexts.Add(IMCMouse.Object);
}

void ASillasPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController()) return;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        for (int32 i = 0; i < DefaultMappingContexts.Num(); ++i)
        {
            if (DefaultMappingContexts[i])
            {
                Subsystem->AddMappingContext(DefaultMappingContexts[i], i);
            }
        }
    }

    // El menú deja el input en UIOnly; asegurar juego puro en la arena
    // (mismo criterio que PTLobbyPlayerController en el lobby).
    SetInputMode(FInputModeGameOnly());
    SetShowMouseCursor(false);
}
