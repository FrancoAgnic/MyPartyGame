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
#include "PTLobbyCharacter.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "ProceduralMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Components/SkeletalMeshComponent.h"

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
            {
                Menu->MenuSetup(DefaultMaxPlayers, SelfMapPath);
                ActiveOverlay = Menu; // para colapsarlo al esculpir la cabeza
            }
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
            {
                HUD->ShowHUD();
                ActiveOverlay = HUD; // para colapsarlo al esculpir la cabeza
            }
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
    // Mismos inputs que el gameplay: LMB esculpe en el modo actual, 1/2/3 = Add/Erase/Paint, rueda = tamaño.
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadStampPressed);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &APTLobbyPlayerController::OnHeadStampReleased);
    InputComponent->BindKey(EKeys::MouseScrollUp,   IE_Pressed,  this, &APTLobbyPlayerController::OnHeadScrollUp);
    InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadScrollDown);
    InputComponent->BindKey(EKeys::One,   IE_Pressed, this, &APTLobbyPlayerController::OnHeadModeAdd);
    InputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &APTLobbyPlayerController::OnHeadModeErase);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &APTLobbyPlayerController::OnHeadModePaint);
}

void APTLobbyPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!bHeadSculptMode) return;

    // Órbita de cámara con WASD (A/D = yaw, W/S = pitch).
    const float dYaw   = (IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::A) ? 1.f : 0.f);
    const float dPitch = (IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
    if (dYaw != 0.f || dPitch != 0.f)
    {
        HeadOrbitYaw   += dYaw   * HeadOrbitSpeed * DeltaTime;
        HeadOrbitPitch  = FMath::Clamp(HeadOrbitPitch + dPitch * HeadOrbitSpeed * DeltaTime, -85.f, 85.f);
        UpdateHeadCam();
    }

    // Flecha de preview: hacia dónde MIRA el personaje (su forward), desde el centro de la cabeza.
    // Así sabés la orientación de la cara mientras modelás desde cualquier ángulo.
    if (HeadVolume && GetPawn())
    {
        const FVector C = HeadVolume->GetActorLocation();
        const FVector Fwd = GetPawn()->GetActorForwardVector();
        DrawDebugDirectionalArrow(GetWorld(), C, C + Fwd * (HeadRadius * 2.5f),
            HeadRadius * 0.6f, FColor(80, 200, 255), false, -1.f, 0, 2.f);
    }

    // Punto de la brocha (cursor) — sirve para el preview y para esculpir.
    FVector Pt;
    const bool bHavePt = GetHeadStampPoint(Pt);

    // Preview de la herramienta siguiendo el cursor (Add/Erase; Paint no muestra malla 3D).
    UpdateHeadPreview(bHavePt ? &Pt : nullptr);

    // Stamp continuo mientras se mantiene el LMB.
    if (HeadVolume && bHeadStamping && bHavePt)
        HeadVolume->ApplyStamp(Pt, EPTStampShape::Sphere, HeadBrushSize, HeadEditMode, HeadPaintColor);
}

void APTLobbyPlayerController::UpdateHeadPreview(const FVector* At)
{
    if (!HeadPreviewActor || !HeadPreviewMesh) return;

    // Paint no lleva malla de preview 3D (como el gameplay: cursor 2D). Sin punto → oculto.
    const bool bShow = At && bHeadSculptMode && HeadEditMode != EPTEditMode::Paint;
    HeadPreviewActor->SetActorHiddenInGame(!bShow);
    if (!bShow) return;

    // Reconstruir la malla de la brocha sólo si cambió tamaño o modo.
    if (HeadPreviewSize != HeadBrushSize || HeadPreviewMode != HeadEditMode)
    {
        TArray<FVector> V, Nn; TArray<int32> Tt;
        APTSculptVolume::BuildStampPreview(EPTStampShape::Sphere, HeadBrushSize,
            HeadVolume ? HeadVolume->VoxelSize : 5.f, V, Tt, Nn);
        HeadPreviewMesh->CreateMeshSection(0, V, Tt, Nn, {}, {}, {}, false);

        UMaterialInterface* Mat = (HeadEditMode == EPTEditMode::Erase) ? HeadPreviewMatErase : HeadPreviewMatAdd;
        if (Mat) HeadPreviewMesh->SetMaterial(0, Mat);

        HeadPreviewSize = HeadBrushSize;
        HeadPreviewMode = HeadEditMode;
    }
    HeadPreviewActor->SetActorLocation(*At);
}

void APTLobbyPlayerController::UpdateHeadCam()
{
    if (!HeadCam || !HeadVolume) return;
    const FVector C   = HeadVolume->GetActorLocation();
    const FVector Dir = FRotator(HeadOrbitPitch, HeadOrbitYaw, 0.f).Vector(); // dirección de mirada
    const FVector CamLoc = C - Dir * HeadCamDistance;
    HeadCam->SetActorLocation(CamLoc);
    HeadCam->SetActorRotation(Dir.Rotation());
}

void APTLobbyPlayerController::ApplyHeadSculptInputMode()
{
    // GameAndUI + cursor, pero con CaptureDuringMouseDown → el LMB llega al esculpido (a diferencia
    // del modo diorama que usa NoCapture y manda los clicks solo a la UI).
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
    SetShowMouseCursor(true);
    if (ULocalPlayer* LP = GetLocalPlayer())
        if (UGameViewportClient* VC = LP->ViewportClient)
        {
            VC->SetMouseCaptureMode(EMouseCaptureMode::CaptureDuringMouseDown);
            VC->SetMouseLockMode(EMouseLockMode::DoNotLock);
        }
}

void APTLobbyPlayerController::OnHeadStampPressed()  { if (bHeadSculptMode) bHeadStamping = true; }
void APTLobbyPlayerController::OnHeadStampReleased() { bHeadStamping = false; }
void APTLobbyPlayerController::OnHeadScrollUp()   { if (bHeadSculptMode) HeadBrushSize = FMath::Clamp(HeadBrushSize + 4.f, 6.f, 80.f); }
void APTLobbyPlayerController::OnHeadScrollDown() { if (bHeadSculptMode) HeadBrushSize = FMath::Clamp(HeadBrushSize - 4.f, 6.f, 80.f); }
void APTLobbyPlayerController::OnHeadModeAdd()   { if (bHeadSculptMode) HeadEditMode = EPTEditMode::Add;   }
void APTLobbyPlayerController::OnHeadModeErase() { if (bHeadSculptMode) HeadEditMode = EPTEditMode::Erase; }
void APTLobbyPlayerController::OnHeadModePaint() { if (bHeadSculptMode) HeadEditMode = EPTEditMode::Paint; }

bool APTLobbyPlayerController::GetHeadStampPoint(FVector& OutWorld) const
{
    if (!HeadVolume || !GetWorld()) return false;

    if (!HeadCam) return false;

    FVector Origin, Dir;
    if (!const_cast<APTLobbyPlayerController*>(this)->DeprojectMousePositionToWorld(Origin, Dir)) return false;

    // IGUAL QUE EL GAMEPLAY (estilo SculptrVR): el sello NO cae sobre la superficie de la arcilla
    // —eso la haría crecer hacia la cámara y no dejaría hacer trazos laterales—, sino sobre un
    // PLANO a la profundidad de la cabeza, perpendicular a la cámara. Arrastrar el cursor desliza
    // el sello por ese plano → trazos laterales a profundidad fija, desde cualquier ángulo (WASD).
    const FVector C = HeadVolume->GetActorLocation();      // centro de la cabeza (punto del plano)
    const FVector N = HeadCam->GetActorForwardVector();    // normal del plano = mirada de la cámara
    const float denom = FVector::DotProduct(Dir, N);
    if (FMath::Abs(denom) < 1e-4f) return false;           // rayo casi paralelo al plano
    const float t = FVector::DotProduct(C - Origin, N) / denom;
    if (t <= 0.f) return false;                            // el plano está detrás del cursor
    OutWorld = Origin + Dir * t;
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

    APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(P);

    // Personaje recto y quieto (sin baile ni jiggle del physics asset) mientras esculpís.
    if (Char) Char->SetSculptPose(true);
    // Borrar la cabeza que ya tenía asignada: molesta para modelar una nueva desde cero.
    if (Char) Char->ClearHeadMesh();
    // Colapsar la UI del lobby para que el mouse llegue al esculpido (no lo agarre la UI).
    if (ActiveOverlay) ActiveOverlay->SetVisibility(ESlateVisibility::Collapsed);

    // El "banco de esculpido" cae JUSTO sobre el HeadSocket del personaje: esculpís donde va la
    // cabeza y la cámara orbita alrededor de ese centro. Fallback: arriba del actor.
    FVector Center = P->GetActorLocation() + FVector(0, 0, HeadUpOffset);
    if (Char && Char->GetMesh() && Char->GetMesh()->DoesSocketExist(TEXT("HeadSocket")))
        Center = Char->GetMesh()->GetSocketLocation(TEXT("HeadSocket"));

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    HeadVolume = GetWorld()->SpawnActor<APTSculptVolume>(HeadVolumeClass, Center, FRotator::ZeroRotator, SP);

    // Bolita inicial de arcilla para que haya algo que esculpir (color piel neutro).
    if (HeadVolume)
        HeadVolume->ApplyStamp(Center, EPTStampShape::Sphere, 40.f, EPTEditMode::Add, FLinearColor(0.95f, 0.78f, 0.66f));

    // Actor de preview de la brocha (fantasma que sigue al cursor, como el gameplay).
    HeadPreviewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Center, FRotator::ZeroRotator, SP);
    if (HeadPreviewActor)
    {
        HeadPreviewMesh = NewObject<UProceduralMeshComponent>(HeadPreviewActor, TEXT("HeadPreviewMesh"));
        HeadPreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HeadPreviewMesh->SetCastShadow(false);
        HeadPreviewMesh->RegisterComponent();
        HeadPreviewActor->SetRootComponent(HeadPreviewMesh);
        HeadPreviewActor->SetActorHiddenInGame(true);
    }
    HeadPreviewSize = -1.f;                    // fuerza reconstruir la malla en el primer tick
    HeadPreviewMode = EPTEditMode::Smooth;

    // Cámara ORBITAL: arranca mirando la cara del personaje (desde adelante). WASD la orbita.
    HeadCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass());
    HeadOrbitYaw   = P->GetActorRotation().Yaw + 180.f; // mirar desde el frente hacia el personaje
    HeadOrbitPitch = -8.f;
    UpdateHeadCam();
    if (HeadCam) SetViewTargetWithBlend(HeadCam, 0.35f);

    // Congelar el personaje; input con CAPTURA para que el LMB llegue al esculpido (no a la UI).
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    ApplyHeadSculptInputMode();

    UE_LOG(LogTemp, Log, TEXT("[Lobby] Modo esculpir-cabeza: ON."));
}

void APTLobbyPlayerController::ExitHeadSculpt()
{
    if (!bHeadSculptMode) return;
    bHeadSculptMode = false;
    bHeadStamping = false;

    // Hornear: copiar la escultura (malla local del volumen) a la cabeza del personaje, pegada
    // al HeadSocket. La arcilla se esculpió centrada en el origen del volumen → queda centrada
    // en el socket. Ajustá el tamaño con el RelativeScale3D del HeadMesh en BP_LobbyCharacter.
    if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn()))
    {
        if (HeadVolume) Char->SetHeadMeshFrom(HeadVolume->GetMeshComponent());
        Char->SetSculptPose(false); // restaura el baile + jiggle
    }

    if (HeadVolume)       { HeadVolume->Destroy();       HeadVolume = nullptr; }
    if (HeadCam)          { HeadCam->Destroy();          HeadCam = nullptr; }
    if (HeadPreviewActor) { HeadPreviewActor->Destroy(); HeadPreviewActor = nullptr; HeadPreviewMesh = nullptr; }

    // Restaurar la UI del lobby.
    if (ActiveOverlay) ActiveOverlay->SetVisibility(ESlateVisibility::Visible);

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
