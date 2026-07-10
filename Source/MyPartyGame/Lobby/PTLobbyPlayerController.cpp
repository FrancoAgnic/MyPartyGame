// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyPlayerController.h"
#include "PTLobbyEscapeMenuWidget.h"
#include "PTGameState.h"
#include "PTPlayerState.h"
#include "PTLobbyGameMode.h"
#include "PTMainMenuWidget.h"
#include "PTLobbyHUDWidget.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"

void APTLobbyPlayerController::Server_RequestStartGame_Implementation()
{
    const APTPlayerState* PS = GetPlayerState<APTPlayerState>();
    if (!PS || !PS->bIsHost) return;

    if (APTGameState* PTGS = GetWorld()->GetGameState<APTGameState>())
        PTGS->LobbyState = EPTLobbyState::Starting;

    if (APTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<APTLobbyGameMode>())
        GM->TravelToGame();
}

void APTLobbyPlayerController::Server_SetReady_Implementation(bool bInReady)
{
    APTPlayerState* PS = GetPlayerState<APTPlayerState>();
    if (!PS) return;

    PS->bIsReady = bInReady;

    if (APTLobbyGameMode* GM = GetWorld()->GetAuthGameMode<APTLobbyGameMode>())
        GM->CheckReadyState();
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

        // Menú/lobby diegético: el mouse queda visible para la UI (overlay 2D), pero el
        // teclado (WASD/Space) sigue yendo al juego. El look se bloquea en SetupDioramaView
        // (SetIgnoreLookInput), así que el mouse nunca necesita "capturarse" para nada.
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        Mode.SetHideCursorDuringCapture(false);
        SetInputMode(Mode);
        SetShowMouseCursor(true);

        // SetHideCursorDuringCapture(false) no alcanza: FInputModeGameAndUI igual arma un
        // capture mode que agarra el mouse al primer click (para permitir look FPS), y ese
        // capture es lo que hace desaparecer la flechita en algunos drivers/monitores —
        // pasa en PIE y en Standalone. Como el look de mouse está deshabilitado por diseño,
        // no hace falta ningún capture: lo desactivamos directo en el viewport.
        if (ULocalPlayer* LP = GetLocalPlayer())
        {
            if (UGameViewportClient* VC = LP->ViewportClient)
            {
                VC->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
                VC->SetMouseLockMode(EMouseLockMode::DoNotLock);
            }
        }

        // Que poseer el pawn NO cambie la cámara (si no, volvería a la del personaje).
        bAutoManageActiveCameraTarget = false;

        // Fijar la vista a la cámara diorama. Reintenta si aún no está lista.
        SetupDioramaView();
        if (!bDioramaReady)
            GetWorldTimerManager().SetTimer(DioramaRetry, this,
                &APTLobbyPlayerController::SetupDioramaView, 0.2f, true);

        ShowLobbyOverlay();
    }
}

void APTLobbyPlayerController::ShowLobbyOverlay()
{
    // NM_Standalone: recién se abrió el juego, todavía no hay sesión → Crear/Unirse/Opciones.
    // NM_ListenServer/NM_Client: ya viajamos acá con "?listen" (host) o nos unimos (cliente) →
    // ya estamos en la sala, mostrar la lista de jugadores + Ready.
    const ENetMode NetMode = GetNetMode();
    UE_LOG(LogTemp, Log, TEXT("[Lobby] ShowLobbyOverlay: NetMode=%d (0=Standalone,1=DedicatedServer,2=ListenServer,3=Client)"), (int32)NetMode);

    if (NetMode == NM_Standalone)
    {
        if (MainMenuWidgetClass)
        {
            UE_LOG(LogTemp, Log, TEXT("[Lobby] Mostrando MainMenuWidget (Crear/Unirse/Opciones)."));
            if (UPTMainMenuWidget* Menu = CreateWidget<UPTMainMenuWidget>(this, MainMenuWidgetClass))
                Menu->MenuSetup(DefaultMaxPlayers, SelfMapPath);
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Lobby] MainMenuWidgetClass no está asignado en BP_LobbyPlayerController."));
        }
    }
    else
    {
        if (LobbyHUDWidgetClass)
        {
            UE_LOG(LogTemp, Log, TEXT("[Lobby] Mostrando LobbyHUDWidget (lista + Ready)."));
            if (UPTLobbyHUDWidget* HUD = CreateWidget<UPTLobbyHUDWidget>(this, LobbyHUDWidgetClass))
                HUD->ShowHUD();
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Lobby] LobbyHUDWidgetClass no está asignado en BP_LobbyPlayerController."));
        }
    }
}

void APTLobbyPlayerController::SetupDioramaView()
{
    if (!IsLocalController()) return;

    // Buscar la ACameraActor del nivel por tag (existe localmente en todos los clientes).
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsOfClassWithTag(GetWorld(), ACameraActor::StaticClass(),
                                                 DioramaCameraTag, Found);
    ACameraActor* Cam = Found.Num() > 0 ? Cast<ACameraActor>(Found[0]) : nullptr;
    if (!Cam)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Lobby] SetupDioramaView: no encontré ninguna ACameraActor con tag '%s' en el nivel (reintentando)."),
            *DioramaCameraTag.ToString());
        return; // reintenta en el timer
    }

    UE_LOG(LogTemp, Log, TEXT("[Lobby] SetupDioramaView: cámara diorama encontrada (%s), fijando vista."), *Cam->GetName());

    SetViewTargetWithBlend(Cam, 0.f);
    // La base del movimiento (Yaw) queda relativa a la cámara; el char gira hacia donde
    // se mueve (bOrientRotationToMovement en el character). El mouse no rota (es para la UI).
    SetControlRotation(Cam->GetActorRotation());
    SetIgnoreLookInput(true);

    bDioramaReady = true;
    GetWorldTimerManager().ClearTimer(DioramaRetry);
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

    if (EscapeMenuWidget) EscapeMenuWidget->HandleEscape();
}
