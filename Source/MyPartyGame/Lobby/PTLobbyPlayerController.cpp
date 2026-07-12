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
#include "Camera/CameraComponent.h"
#include "../Sculpt/PTSculptVolume.h"
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

    UE_LOG(LogTemp, Log, TEXT("[Lobby] APTLobbyPlayerController::BeginPlay. IsLocalController=%d NetMode=%d"),
        IsLocalController() ? 1 : 0, (int32)GetNetMode());

    // Que poseer el pawn NO cambie la cámara. Va FUERA del bloque IsLocalController a propósito:
    // en un cliente, esta misma clase corre en el SERVIDOR como proxy remoto (IsLocalController()
    // ahí es false), y si el flag queda en true el servidor auto-administra la cámara de ese
    // cliente y le manda ClientSetViewTarget(pawn) por red, pisando la cámara diorama que el
    // cliente fija localmente. Desactivarlo en ambos lados es lo que hace que los clientes también
    // vean la cámara fija del lobby y no su primera persona.
    bAutoManageActiveCameraTarget = false;

    // Solo el controlador local necesita el contexto de input y la vista/overlay locales.
    if (IsLocalController())
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            if (LobbyMappingContext)
                Subsystem->AddMappingContext(LobbyMappingContext, 0);
        }

        ApplyDioramaInputMode();

        // Fijar la vista a la cámara diorama. Reintenta si aún no está lista.
        SetupDioramaView();
        if (!bDioramaReady)
            GetWorldTimerManager().SetTimer(DioramaRetry, this,
                &APTLobbyPlayerController::SetupDioramaView, 0.2f, true);

        ShowLobbyOverlay();
    }
}

void APTLobbyPlayerController::ApplyDioramaInputMode()
{
    if (!IsLocalController()) return;

    // Menú/lobby diegético: el mouse queda visible para la UI (overlay 2D), pero el teclado
    // (WASD/Space) sigue yendo al juego. El look se bloquea en SetupDioramaView (SetIgnoreLookInput),
    // así que el mouse nunca necesita "capturarse" para nada.
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
    SetShowMouseCursor(true);

    // SetHideCursorDuringCapture(false) no alcanza: FInputModeGameAndUI igual arma un capture
    // mode que agarra el mouse al primer click (para permitir look FPS), y ese capture es lo que
    // hace desaparecer la flechita en algunos drivers/monitores. Como el look está deshabilitado
    // por diseño, no hace falta ningún capture: lo desactivamos directo en el viewport.
    if (ULocalPlayer* LP = GetLocalPlayer())
    {
        if (UGameViewportClient* VC = LP->ViewportClient)
        {
            VC->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
            VC->SetMouseLockMode(EMouseLockMode::DoNotLock);
        }
    }
}

void APTLobbyPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);

    // La posesión del pawn fija la vista al personaje (en clientes llega por replicación DESPUÉS
    // del BeginPlay, así que la cámara diorama que se fijó ahí queda pisada). Re-asegurarla.
    if (IsLocalController())
    {
        bDioramaReady = false;
        SetupDioramaView();
        if (!bDioramaReady)
            GetWorldTimerManager().SetTimer(DioramaRetry, this,
                &APTLobbyPlayerController::SetupDioramaView, 0.2f, true);
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

    // Tecla G: entrar/salir del modo esculpir tu cabeza custom.
    InputComponent->BindKey(EKeys::G, IE_Pressed, this, &APTLobbyPlayerController::ToggleHeadSculptMode);

    // Esculpido de la cabeza (solo hace algo en modo cabeza; los handlers gatean con bHeadSculptMode).
    InputComponent->BindKey(EKeys::LeftMouseButton,  IE_Pressed,  this, &APTLobbyPlayerController::OnHeadStampPressed);
    InputComponent->BindKey(EKeys::LeftMouseButton,  IE_Released, this, &APTLobbyPlayerController::OnHeadStampReleased);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadErasePressed);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &APTLobbyPlayerController::OnHeadEraseReleased);
    InputComponent->BindKey(EKeys::MouseScrollUp,    IE_Pressed,  this, &APTLobbyPlayerController::OnHeadScrollUp);
    InputComponent->BindKey(EKeys::MouseScrollDown,  IE_Pressed,  this, &APTLobbyPlayerController::OnHeadScrollDown);
}

void APTLobbyPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!bHeadSculptMode || !HeadVolume || (!bHeadStamping && !bHeadErasing)) return;

    FVector Pt;
    if (!GetHeadStampPoint(Pt)) return;
    const EPTEditMode Mode = bHeadErasing ? EPTEditMode::Erase : EPTEditMode::Add;
    HeadVolume->ApplyStamp(Pt, EPTStampShape::Sphere, HeadBrushSize, Mode, HeadPaintColor);
}

void APTLobbyPlayerController::OnHeadStampPressed()  { if (bHeadSculptMode) bHeadStamping = true; }
void APTLobbyPlayerController::OnHeadStampReleased() { bHeadStamping = false; }
void APTLobbyPlayerController::OnHeadErasePressed()  { if (bHeadSculptMode) bHeadErasing = true; }
void APTLobbyPlayerController::OnHeadEraseReleased() { bHeadErasing = false; }
void APTLobbyPlayerController::OnHeadScrollUp()   { if (bHeadSculptMode) HeadBrushSize = FMath::Clamp(HeadBrushSize + 4.f, 6.f, 80.f); }
void APTLobbyPlayerController::OnHeadScrollDown() { if (bHeadSculptMode) HeadBrushSize = FMath::Clamp(HeadBrushSize - 4.f, 6.f, 80.f); }

bool APTLobbyPlayerController::GetHeadStampPoint(FVector& OutWorld) const
{
    if (!HeadVolume || !GetWorld()) return false;

    FVector Origin, Dir;
    if (!const_cast<APTLobbyPlayerController*>(this)->DeprojectMousePositionToWorld(Origin, Dir)) return false;

    // 1) Raycast contra la arcilla (si el volumen tiene colisión de query).
    FHitResult Hit;
    FCollisionQueryParams Q; Q.bTraceComplex = true;
    if (GetWorld()->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * 5000.f, ECC_Visibility, Q)
        && Hit.GetActor() == HeadVolume)
    {
        OutWorld = Hit.ImpactPoint;
        return true;
    }

    // 2) Fallback: intersección rayo–esfera (esfera de sculpt en el centro del volumen).
    const FVector C = HeadVolume->GetActorLocation();
    const FVector m = Origin - C;
    const float b = FVector::DotProduct(m, Dir);
    const float c = FVector::DotProduct(m, m) - HeadRadius * HeadRadius;
    if (c > 0.f && b > 0.f) return false;
    const float disc = b * b - c;
    if (disc < 0.f) return false;
    OutWorld = Origin + Dir * FMath::Max(-b - FMath::Sqrt(disc), 0.f);
    return true;
}

// ── Modo esculpir cabeza (tecla G) ───────────────────────────────────────────

void APTLobbyPlayerController::ToggleHeadSculptMode()
{
    if (!IsLocalController()) return;
    if (bHeadSculptMode) ExitHeadSculpt();
    else                 EnterHeadSculpt();
}

void APTLobbyPlayerController::EnterHeadSculpt()
{
    APawn* P = GetPawn();
    if (bHeadSculptMode || !HeadVolumeClass || !P || !GetWorld()) return;
    bHeadSculptMode = true;

    // Punto del "banco de esculpido": adelante y un poco arriba del personaje.
    const FVector Fwd = P->GetActorForwardVector();
    const FVector Center = P->GetActorLocation() + Fwd * HeadFrontDistance + FVector(0, 0, HeadUpOffset);

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    HeadVolume = GetWorld()->SpawnActor<APTSculptVolume>(HeadVolumeClass, Center, FRotator::ZeroRotator, SP);

    // Bolita inicial de arcilla para que haya algo que esculpir (color piel neutro).
    if (HeadVolume)
        HeadVolume->ApplyStamp(Center, EPTStampShape::Sphere, 40.f, EPTEditMode::Add, FLinearColor(0.95f, 0.78f, 0.66f));

    // Cámara mirando el banco desde el frente (entre la cámara diorama y el volumen).
    HeadCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass());
    if (HeadCam)
    {
        const FVector CamLoc = Center - Fwd * HeadCamDistance + FVector(0, 0, 20.f);
        HeadCam->SetActorLocation(CamLoc);
        HeadCam->SetActorRotation((Center - CamLoc).Rotation());
        SetViewTargetWithBlend(HeadCam, 0.35f);
    }

    // Congelar el personaje y dejar el mouse para esculpir (el input real es P3).
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    ApplyDioramaInputMode(); // GameAndUI + cursor

    UE_LOG(LogTemp, Log, TEXT("[Lobby] Modo esculpir-cabeza: ON."));
}

void APTLobbyPlayerController::ExitHeadSculpt()
{
    if (!bHeadSculptMode) return;
    bHeadSculptMode = false;
    bHeadStamping = bHeadErasing = false;

    // (P4 horneará la malla al HeadSocket antes de destruir el volumen.)
    if (HeadVolume) { HeadVolume->Destroy(); HeadVolume = nullptr; }
    if (HeadCam)    { HeadCam->Destroy();    HeadCam = nullptr; }

    SetIgnoreMoveInput(false);
    SetupDioramaView();      // vuelve la cámara fija del lobby
    ApplyDioramaInputMode(); // restaura cursor/input del lobby

    UE_LOG(LogTemp, Log, TEXT("[Lobby] Modo esculpir-cabeza: OFF."));
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

    // Al cerrar el menú de Escape, UPTLobbyEscapeMenuWidget::ToggleMenu deja el input en
    // GameOnly + cursor oculto (correcto para el juego FPS, NO para el lobby diegético, que
    // necesita el mouse para tocar Listo). Si el menú quedó cerrado, restaurar el modo diorama.
    if (EscapeMenuWidget && !EscapeMenuWidget->IsMenuOpen())
        ApplyDioramaInputMode();
}
