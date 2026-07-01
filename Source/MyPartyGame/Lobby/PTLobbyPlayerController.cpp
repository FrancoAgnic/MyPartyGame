// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyPlayerController.h"
#include "PTLobbyEscapeMenuWidget.h"
#include "PTGameState.h"
#include "PTPlayerState.h"
#include "PTLobbyGameMode.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

void APTLobbyPlayerController::Server_RequestStartGame_Implementation()
{
    const APTPlayerState* PS = GetPlayerState<APTPlayerState>();
    if (!PS || !PS->bIsHost) return;

    if (APTGameState* PTGS = GetWorld()->GetGameState<APTGameState>())
        PTGS->LobbyState = EPTLobbyState::Starting;

    if (APTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<APTLobbyGameMode>())
        GM->TravelToGame();
}

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

void APTLobbyPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
    {
        if (EscapeMenuAction)
        {
            EnhancedInput->BindAction(EscapeMenuAction, ETriggerEvent::Started,
                this, &APTLobbyPlayerController::ToggleEscapeMenu);
        }
    }

    // Tecla P: el host arranca la partida (temporal hasta que exista el botón en el HUD).
    InputComponent->BindKey(EKeys::P, IE_Pressed, this, &APTLobbyPlayerController::OnPressedStartGame);
}

void APTLobbyPlayerController::OnPressedStartGame()
{
    Server_RequestStartGame();
}

void APTLobbyPlayerController::ToggleEscapeMenu(const FInputActionValue& Value)
{
    if (!IsLocalController() || !EscapeMenuWidgetClass) return;

    if (!EscapeMenuWidget)
    {
        EscapeMenuWidget = CreateWidget<UPTLobbyEscapeMenuWidget>(this, EscapeMenuWidgetClass);
        if (EscapeMenuWidget) EscapeMenuWidget->AddToViewport();
    }

    if (EscapeMenuWidget) EscapeMenuWidget->ToggleMenu();
}
