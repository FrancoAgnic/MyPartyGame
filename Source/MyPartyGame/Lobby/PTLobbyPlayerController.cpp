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
#include "PTLockerWidget.h"
#include "PTLockerSubsystem.h"
#include "Engine/GameInstance.h"
#include "PTHeadSculptHUDWidget.h"
#include "../PTGameUserSettings.h"
#include "../Multiplayer/MultiplayerSessionsSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/LocalPlayer.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "../UI/PTColorPickerWidget.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"

// Magic del blob de estado CRUDO de la cabeza (RawState del Locker, para re-editar): campo SDF del
// volumen + ojos locales. 'PTR2' = PT Raw v2 (distinto del blob COCINADO 'PTH2').
static const uint32 PT_HEADRAW_MAGIC = 0x50545232; // 'PTR2'

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

        PushIdentityToServer();
    }
}

void APTLobbyPlayerController::PushIdentityToServer()
{
    if (!IsLocalController()) return;

    APTPlayerState* PS = GetPlayerState<APTPlayerState>();
    if (!PS)
    {
        // El PlayerState todavía no replicó → reintentar (si no, el jugador queda sin nombre).
        FTimerHandle H;
        GetWorldTimerManager().SetTimer(H, this, &APTLobbyPlayerController::PushIdentityToServer, 0.5f, false);
        return;
    }

    // Nombre: backstop del "?Name=" de la URL de travel (si se perdió, nadie vería quién soy).
    if (UMultiplayerSessionsSubsystem* Sessions =
            GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
    {
        const FString MyName = Sessions->GetLocalPlayerDisplayName();
        if (!MyName.IsEmpty()) PS->Server_ReportDisplayName(MyName);
    }

    // Idioma: para que la palabra a adivinar llegue en el mío ya desde el lobby.
    FString Lang = TEXT("es");
    if (const UPTGameUserSettings* S = UPTGameUserSettings::Get()) Lang = S->GetLanguageCode();
    PS->Server_SetLanguage(Lang);

    // MOMENTO 1 de la sincronización de cabezas: me acabo de unir a una sala.
    //  a) le pido al server las cabezas de todos los que ya estaban;
    //  b) el personaje sube la mía guardada (LoadHead lo hace al poseer el pawn), y el server la
    //     reparte al resto. Con las dos direcciones, todos ven a todos.
    PS->Server_RequestAllHeads();

    // Heartbeat: en el LOBBY re-pido cada 2s las cabezas que me falten (si tu cabeza se actualizó a los
    // demás pero vos entraste cargando y te perdiste el broadcast, esto lo repara solo). No manda nada
    // cuando ya está todo sincronizado.
    if (IsLocalController() && !GetWorldTimerManager().IsTimerActive(HeadSyncTimer))
        GetWorldTimerManager().SetTimer(HeadSyncTimer, this,
            &APTLobbyPlayerController::HeadSyncHeartbeat, 2.0f, /*loop=*/true, /*firstDelay=*/2.0f);
}

void APTLobbyPlayerController::HeadSyncHeartbeat()
{
    if (APTPlayerState* PS = GetPlayerState<APTPlayerState>()) PS->RefreshHeadsIfMissing();
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
    // (La tecla P para forzar inicio se sacó: creaba una sesión "fantasma". Solo se juega desde
    //  Crear/Unirse partida + el arranque automático cuando todos están listos.)

    // Tecla G: entrar/salir del modo esculpir tu cabeza custom.
    // (La tecla G quedó reemplazada por el botón "Locker" del menú principal.)

    // Esculpido de la cabeza (solo hace algo en modo cabeza; los handlers gatean con bHeadSculptMode).
    // Mismos inputs que el gameplay: LMB esculpe en el modo actual, 1/2/3 = Add/Erase/Paint, rueda = tamaño.
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadStampPressed);
    InputComponent->BindKey(EKeys::LeftMouseButton, IE_Released, this, &APTLobbyPlayerController::OnHeadStampReleased);
    InputComponent->BindKey(EKeys::MouseScrollUp,   IE_Pressed,  this, &APTLobbyPlayerController::OnHeadScrollUp);
    InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadScrollDown);
    InputComponent->BindKey(EKeys::One,   IE_Pressed, this, &APTLobbyPlayerController::OnHeadModeAdd);
    InputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &APTLobbyPlayerController::OnHeadModeErase);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &APTLobbyPlayerController::OnHeadModePaint);
    InputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &APTLobbyPlayerController::OnHeadModeEyes);
    // ALT (mantener) en Add: pegar el sello a la superficie de la cabeza (detallar de cerca), igual que el gameplay.
    InputComponent->BindKey(EKeys::LeftAlt,  IE_Pressed,  this, &APTLobbyPlayerController::OnHeadSurfaceSnapPressed);
    InputComponent->BindKey(EKeys::LeftAlt,  IE_Released, this, &APTLobbyPlayerController::OnHeadSurfaceSnapReleased);
    InputComponent->BindKey(EKeys::RightAlt, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadSurfaceSnapPressed);
    InputComponent->BindKey(EKeys::RightAlt, IE_Released, this, &APTLobbyPlayerController::OnHeadSurfaceSnapReleased);
    // Enter = confirmar la edición (guarda + equipa + vuelve al Locker). Escape = popup guardar/descartar.
    InputComponent->BindKey(EKeys::Enter,  IE_Pressed, this, &APTLobbyPlayerController::ConfirmHeadEdit);
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &APTLobbyPlayerController::RequestHeadBack);
    // TAB = ciclar la forma del sello (esfera/cubo/cilindro/cono), igual que el gameplay.
    InputComponent->BindKey(EKeys::Tab,   IE_Pressed, this, &APTLobbyPlayerController::OnHeadCycleShape);
    // Rueda del mouse mantenida = rotar el shape (doble click = reset). Igual que el gameplay.
    InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadRotatePressed);
    InputComponent->BindKey(EKeys::MiddleMouseButton, IE_Released, this, &APTLobbyPlayerController::OnHeadRotateReleased);
    // Backspace: toque = undo; mantenido = resetear. Por EVENTOS (IsInputKeyDown no sirve con Backspace
    // en este modo). El acumulado y la confirmación de soltada (con debounce anti auto-repeat) van en PlayerTick.
    InputComponent->BindKey(EKeys::BackSpace, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadClearPressed);
    InputComponent->BindKey(EKeys::BackSpace, IE_Released, this, &APTLobbyPlayerController::OnHeadClearReleased);
    // SHIFT = (solo en Paint) alternar entre pintar la cabeza (arcilla) y el cuerpo (piel).
    InputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &APTLobbyPlayerController::OnHeadToggleBodyPaint);
    // RMB mantenido = color picker (igual que el gameplay).
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadColorPickPressed);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &APTLobbyPlayerController::OnHeadColorPickReleased);
    // E: guardar el color actual en el anillo del color picker (igual que el gameplay).
    InputComponent->BindKey(EKeys::E, IE_Pressed, this, &APTLobbyPlayerController::OnHeadColorSave);
}

void APTLobbyPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    // En el Locker (sin editar): la cámara sigue la POSICIÓN del personaje pero no rota (dirección fija).
    if (bLockerOpen && !bHeadSculptMode) UpdateLockerCam();

    if (!bHeadSculptMode) return;

    // Mantener el resaltado de la hotbar al día con la herramienta equipada (1/2/3/4).
    if (HeadHUD) HeadHUD->Refresh(this);

    // Blend de cámara: mover HeadCam suave hacia el destino (cambio cabeza↔cuerpo y órbita WASD).
    if (HeadCam && bHeadCamInit)
    {
        const FVector  L = FMath::VInterpTo(HeadCam->GetActorLocation(), DesiredCamLoc, DeltaTime, HeadCamBlendSpeed);
        const FRotator R = FMath::RInterpTo(HeadCam->GetActorRotation(), DesiredCamRot, DeltaTime, HeadCamBlendSpeed);
        HeadCam->SetActorLocation(L);
        HeadCam->SetActorRotation(R);
    }

    // Órbita de cámara con WASD (A/D = yaw, W/S = pitch).
    const float dYaw   = (IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::A) ? 1.f : 0.f);
    const float dPitch = (IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
    if (dYaw != 0.f || dPitch != 0.f)
    {
        HeadOrbitYaw   += dYaw   * HeadOrbitSpeed * DeltaTime;
        HeadOrbitPitch  = FMath::Clamp(HeadOrbitPitch + dPitch * HeadOrbitSpeed * DeltaTime, -85.f, 85.f);
        UpdateHeadCam();
    }

    // Mantener BACKSPACE HeadClearHoldDuration s = RESET del contexto actual (cuerpo o cabeza).
    // IMPORTANTE: va ACÁ, ANTES de los returns de color-picker y body-paint. Antes estaba después del
    // `return` del bloque bBodyPaintMode, así que en modo cuerpo NUNCA se ejecutaba (el contador no
    // subía y la soltada no se procesaba). No hay auto-repetición de Backspace (verificado por log):
    // un solo Pressed/Released por pulsación, así que basta acumular DeltaTime mientras está apretado.
    if (bHeadClearHeld && !bHeadClearFired)
    {
        HeadClearHoldTime += DeltaTime;
        if (HeadClearHoldTime >= HeadClearHoldDuration)
        {
            bHeadClearFired = true; // ya reseteó: no repetir mientras se sigue manteniendo
            APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn());
            if (bBodyPaintMode || bHeadSculptBodyOnly)
            {
                if (C) C->ClearBodyPaint(); // solo la pintura del cuerpo
            }
            else
            {
                // Cabeza: el "default" es la bolita inicial BLANCA en el centro, no vacío.
                if (HeadVolume)
                {
                    HeadVolume->Multicast_ClearAll_Implementation();
                    HeadVolume->ApplyStamp(HeadVolume->GetActorLocation(), EPTStampShape::Sphere, 40.f,
                                           EPTEditMode::Add, FLinearColor::White);
                }
                HeadStampRotation = FRotator::ZeroRotator;
                HeadEyes.Reset(); RebuildEyesLiveMesh();
                HeadUndoKinds.Reset();
                if (C) C->ClearHeadPaint();
            }
        }
    }

    // Color picker abierto (RMB): tickearlo con el cursor y tomar el color en vivo. No se esculpe.
    if (bHeadColorActive)
    {
        if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(HeadColorPicker))
        {
            CP->QuickPickTick();
            HeadPaintColor = CP->CurrentColor; // color en vivo en la brocha
        }
        UpdateHeadPreview(nullptr, FVector::UpVector); // ocultar el preview mientras elegís color
        return;
    }

    // Sub-modo "pintar el CUERPO" (SHIFT en Paint): el LMB pinta la piel del personaje por UV
    // (no toca la arcilla). Muestra el mismo anillo de preview que el paint de la cabeza, pero
    // apoyado sobre el cuerpo, y dispara las gotitas de pintura al pintar.
    if (bBodyPaintMode)
    {
        APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn());

        // Cursor actual + raycast para el preview del anillo sobre el cuerpo.
        float MX = 0.f, MY = 0.f;
        const bool bHaveCursor = GetMousePosition(MX, MY);
        const FVector2D Cur(MX, MY);
        FVector2D UV; FVector BPt, BN = FVector::UpVector;
        bool bHit = false;
        if (Char && bHaveCursor)
        {
            FVector O, D;
            if (DeprojectScreenPositionToWorld(MX, MY, O, D))
                bHit = Char->RaycastSkinnedMeshUV(O, D, UV, BPt, BN);
        }

        // Anillo de preview sobre el cuerpo (mismo mesh/material del paint de la cabeza, teñido).
        UpdateHeadPreview(bHit ? &BPt : nullptr, BN);

        if (Char && bHeadStamping && bHaveCursor && !IsPaintBudgetFull())
        {
            // Radio del pincel EN EL MUNDO (cm). El pintado sin costuras trabaja en 3D, no en el UV.
            const float R = FMath::Max(1.f, HeadBrushSize * 0.5f * BodyPaintBrushScale);

            // Interpolar EN PANTALLA desde el último cursor: cada muestra hace su propio raycast y
            // pinta una esfera de mundo (los dos lados de una costura se pintan juntos → sin cortes).
            const FVector2D From = bHasLastBodyCursor ? LastBodyCursor : Cur;
            const float DistPx   = FVector2D::Distance(From, Cur);
            const int32 Steps    = bHasLastBodyCursor ? FMath::Clamp(FMath::CeilToInt(DistPx / 6.f), 1, 128) : 1;
            for (int32 s = 1; s <= Steps; ++s)
            {
                const FVector2D P = FMath::Lerp(From, Cur, (float)s / (float)Steps);
                FVector O, D, HP, HN; FVector2D StepUV;
                if (DeprojectScreenPositionToWorld(P.X, P.Y, O, D)
                    && Char->RaycastSkinnedMeshUV(O, D, StepUV, HP, HN))
                    Char->PaintBodyWorldSphere(HP, R, HeadPaintColor);
            }
            Char->FlushBodyPaint(); // una sola subida al GPU por frame
            if (bHit && HeadVolume) HeadVolume->PlayPaintFXAt(BPt, HeadPaintColor, HeadBrushSize); // gotitas
            LastBodyCursor     = Cur;
            bHasLastBodyCursor = true;
            // Recalcular el peso cada ~0.3s mientras pintás (para la barra y el corte por presupuesto).
            PaintBudgetAccum += DeltaTime;
            if (PaintBudgetAccum >= 0.3f) { PaintBudgetAccum = 0.f; Char->RecomputeBodyPaintBytes(); }
        }
        else
        {
            if (bHasLastBodyCursor && Char) Char->RecomputeBodyPaintBytes(); // soltaste → peso final
            bHasLastBodyCursor = false;
            PaintBudgetAccum = 0.f;
        }
        return;
    }

    // Rotar el shape EN EL LUGAR mientras se mantiene la rueda (solo con forma = Agregar). El punto
    // del sello se congela y el cursor se fija, así el shape rota sin desplazarse.
    if (bHeadRotatingShape && HeadToolUsesShapes())
    {
        float DX = 0.f, DY = 0.f;
        GetInputMouseDelta(DX, DY);
        if (DX != 0.f || DY != 0.f)
        {
            HeadStampRotation.Yaw   += DX * HeadShapeRotateSpeed;
            HeadStampRotation.Pitch += DY * HeadShapeRotateSpeed;
            HeadStampRotation.Normalize();
        }
        SetMouseLocation((int32)HeadRotateCursorX, (int32)HeadRotateCursorY); // fijar el cursor
    }

    // (El mantener-Backspace se procesa ARRIBA, antes de los returns de color-picker/body, en
    //  UpdateHeadClearHoldTick — si no, en modo cuerpo nunca se ejecutaba.)

    // Punto de la brocha (cursor) — sirve para el preview y para esculpir. Normal para el preview.
    FVector Pt, Nrm = FVector::UpVector;
    bool bHavePt = GetHeadStampPoint(Pt, Nrm);
    // Mientras rotás, el sello queda CONGELADO en su lugar (rota sin moverse). Se captura al primer frame.
    if (bHeadRotatingShape)
    {
        if (!bHeadRotateHasFrozen && bHavePt)
        {
            HeadRotateFrozenPt = Pt; HeadRotateFrozenNrm = Nrm; bHeadRotateHasFrozen = true;
        }
        if (bHeadRotateHasFrozen) { Pt = HeadRotateFrozenPt; Nrm = HeadRotateFrozenNrm; bHavePt = true; }
    }

    // ¿La brocha quedó fuera del área de esculpido? → icono 🚫 y no se sella ahí.
    bHeadStampOutside = bHavePt && HeadVolume && !HeadVolume->IsInsideCanvas(Pt);

    // Preview de la herramienta siguiendo el cursor.
    UpdateHeadPreview(bHavePt ? &Pt : nullptr, Nrm);

    // Glow de arcilla nueva: el material de la cabeza (M_HeadPaint) usa NowTime igual que el clay.
    APTLobbyCharacter* HeadChar = Cast<APTLobbyCharacter>(GetPawn());
    if (HeadPaintMID && GetWorld())
        HeadPaintMID->SetScalarParameterValue(TEXT("NowTime"), GetWorld()->GetTimeSeconds());

    // ALT + Add: congelar el plano de esculpido en el 1er sello del trazo (a la profundidad de la
    // superficie donde apoyaste), para que el resto del trazo vaya a profundidad constante (no trepa).
    if (HeadVolume && bHeadStamping && bHavePt && bHeadStrokeIsDetail && !bHeadStrokePlaneLocked)
    {
        HeadStrokePlaneOrigin  = Pt;
        HeadStrokePlaneNormal  = HeadCam ? HeadCam->GetActorForwardVector() : FVector::ForwardVector;
        bHeadStrokePlaneLocked = true;
    }

    // Stamp continuo mientras se mantiene el LMB.
    if (HeadVolume && bHeadStamping && bHavePt)
    {
        // Paint (no ojos): pintar la TEXTURA 2D de la cabeza (world-space, sin costuras, igual al cuerpo),
        // NO el atlas.
        if (HeadChar && !bHeadEyesTool && HeadEditMode == EPTEditMode::Paint)
        {
            if (!IsPaintBudgetFull()) // límite de pintura: no pintar más allá del peso replicable
            {
                HeadChar->PaintHeadWorldSphere(HeadVolume->GetMeshComponent(), Pt, HeadBrushSize * 0.5f, HeadPaintColor);
                // También las capas de detalle (lentes/bigote): pintan la MISMA textura 2D por posición.
                for (UProceduralMeshComponent* DM : HeadVolume->GetDetailMeshes())
                    if (DM) HeadChar->PaintHeadWorldSphere(DM, Pt, HeadBrushSize * 0.5f, HeadPaintColor);
                HeadChar->FlushHeadPaint();
                HeadVolume->PlayPaintFXAt(Pt, HeadPaintColor, HeadBrushSize); // gotitas de pintura
                PaintBudgetAccum += DeltaTime;
                if (PaintBudgetAccum >= 0.3f) { PaintBudgetAccum = 0.f; HeadChar->RecomputeHeadPaintBytes(); }
            }
        }
        else
        {
            // Add/Erase: el punto ya viene CLAMPEADO al interior del área (desde el pivot), así que
            // siempre se sella. Si el trazo es de DETALLE (Alt), va a la capa aparte (bDetail=true);
            // si no, a la base. (Multicast_..._Implementation elige el campo y dispara las partículas.)
            HeadVolume->Multicast_ApplyStamp_Implementation(Pt, EffectiveHeadShape(), HeadBrushSize,
                HeadEditMode, HeadPaintColor, HeadStampRotation, bHeadStrokeIsDetail);

            // AGREGAR limpia la pintura 2D vieja de esa zona (la textura de la cabeza es direccional, así
            // que sin esto la arcilla nueva mostraría el color pintado antes ahí en vez del color del
            // picker). Se borra por los MISMOS triángulos que pinta el pincel (base + capas de detalle),
            // así el borrado coincide exacto con dónde pintaría (sin costuras/aproximaciones).
            if (HeadChar && !bHeadEyesTool && HeadEditMode == EPTEditMode::Add)
            {
                // Borrar la pintura vieja por DIRECCIÓN (cono), no por triángulos: la arcilla recién
                // agregada aún no está mallada este frame, así que un borrado por geometría no la
                // alcanzaría y quedarían rastros. El cono limpia esa dirección siempre. Radio un poco
                // mayor que la arcilla nueva para tapar también el fringe.
                HeadChar->ClearHeadPaintCone(HeadVolume->GetMeshComponent(), Pt, HeadBrushSize * 0.65f);
                HeadChar->FlushHeadPaint();
            }
        }
    }
}

void APTLobbyPlayerController::UpdateHeadPreview(const FVector* At, const FVector& Normal)
{
    if (!HeadPreviewActor || !HeadPreviewMesh) return;

    const bool bEyes  = bHeadEyesTool;
    const bool bPaint = (!bEyes && HeadEditMode == EPTEditMode::Paint);
    const bool bTint  = (!bEyes && (HeadEditMode == EPTEditMode::Add || HeadEditMode == EPTEditMode::Paint));
    // Ojos: siempre muestra una esfera. Paint: sólo si asignaste su mesh. Add/Erase: siempre.
    const bool bShow  = At && bHeadSculptMode && (bEyes || !bPaint || HeadPreviewMeshPaint != nullptr);
    HeadPreviewActor->SetActorHiddenInGame(!bShow);
    if (!bShow) return;

    // Reconstruir/elegir el mesh de la brocha sólo si cambió tamaño, modo, forma o el toggle de ojos.
    if (HeadPreviewSize != HeadBrushSize || HeadPreviewMode != HeadEditMode
        || bHeadPreviewEyesCached != bEyes || HeadPreviewShapeCached != EffectiveHeadShape())
    {
        HeadPreviewMID = nullptr;

        // Elegir mesh propio + material según el modo.
        UStaticMesh* ToolMesh = nullptr;
        UMaterialInterface* Mat = nullptr;
        if (bEyes)
        {
            // Preview de ojo = mesh propio del ojo (o esfera) con su material de preview.
            ToolMesh = HeadEyePreviewMesh;
            Mat      = HeadEyePreviewMat;
            if (!Mat) { if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn())) Mat = Char->EyeMaterial; }
            if (!Mat) Mat = HeadPreviewMatAdd;
        }
        else switch (HeadEditMode)
        {
        case EPTEditMode::Erase: ToolMesh = HeadPreviewMeshErase; Mat = HeadPreviewMatErase; break;
        case EPTEditMode::Paint: ToolMesh = HeadPreviewMeshPaint; Mat = HeadPreviewMatPaint; break;
        default: // Add: un mesh por FORMA (asignable). Si esa forma no tiene mesh → preview procedural.
            switch (EffectiveHeadShape())
            {
            case EPTStampShape::Sphere:   ToolMesh = HeadShapeMeshSphere;   break;
            case EPTStampShape::Cube:     ToolMesh = HeadShapeMeshCube;     break;
            case EPTStampShape::Cylinder: ToolMesh = HeadShapeMeshCylinder; break;
            case EPTStampShape::TriPrism: ToolMesh = HeadShapeMeshCone;     break;
            default: break;
            }
            if (!ToolMesh) ToolMesh = HeadPreviewMeshAdd; // fallback general (si lo asignaste)
            Mat = HeadPreviewMatAdd;
            break;
        }

        if (ToolMesh && HeadPreviewStatic)
        {
            // Mesh propio del usuario: escalarlo al tamaño de brocha; ocultar el procedural.
            HeadPreviewStatic->SetStaticMesh(ToolMesh);
            float Scale;
            if (bEyes)
            {
                // El ojo se coloca con radio = HeadBrushSize/2, escalando el mesh por radio/BaseSize.
                float EyeBase = 50.f;
                if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn())) EyeBase = Char->HeadEyeBaseSize;
                Scale = (HeadBrushSize * 0.5f) / FMath::Max(EyeBase, 1.f);
            }
            else
            {
                Scale = HeadBrushSize / FMath::Max(HeadPreviewMeshBaseSize, 1.f);
            }
            HeadPreviewStatic->SetWorldScale3D(FVector(Scale));
            if (Mat) { if (bTint) HeadPreviewMID = HeadPreviewStatic->CreateDynamicMaterialInstance(0, Mat); else HeadPreviewStatic->SetMaterial(0, Mat); }
            HeadPreviewStatic->SetVisibility(true);
            HeadPreviewMesh->SetVisibility(false);
        }
        else
        {
            // Sin mesh propio → forma procedural del sello (la forma elegida; esfera para Ojos).
            if (HeadPreviewStatic) HeadPreviewStatic->SetVisibility(false);
            TArray<FVector> V, Nn; TArray<int32> Tt;
            APTSculptVolume::BuildStampPreview(EffectiveHeadShape(), HeadBrushSize,
                HeadVolume ? HeadVolume->VoxelSize : 5.f, V, Tt, Nn);
            HeadPreviewMesh->CreateMeshSection(0, V, Tt, Nn, {}, {}, {}, false);
            if (Mat) { if (bTint) HeadPreviewMID = HeadPreviewMesh->CreateDynamicMaterialInstance(0, Mat); else HeadPreviewMesh->SetMaterial(0, Mat); }
            HeadPreviewMesh->SetVisibility(true);
        }

        HeadPreviewSize        = HeadBrushSize;
        HeadPreviewMode        = HeadEditMode;
        bHeadPreviewEyesCached = bEyes;
        HeadPreviewShapeCached = EffectiveHeadShape();
    }

    HeadPreviewActor->SetActorLocation(*At);
    // Paint/Ojos: se apoyan sobre la normal de la superficie. Add: usa la rotación del shape (rueda).
    HeadPreviewActor->SetActorRotation((bPaint || bEyes) ? Normal.Rotation()
        : (HeadEditMode == EPTEditMode::Add ? HeadStampRotation : FRotator::ZeroRotator));
    // Color + glow del preview. El material del preview (M_HeadPreview, custom node) usa:
    //   Color = color seleccionado, Glow = 0 en reposo → sube a 1 mientras mantenés apretado al esculpir.
    const float Dt = GetWorld() ? GetWorld()->GetDeltaSeconds() : 0.f;
    const bool  bSculptingNow = bHeadStamping && !bEyes && (HeadEditMode == EPTEditMode::Add);
    HeadPreviewGlow = FMath::FInterpTo(HeadPreviewGlow, bSculptingNow ? 1.f : 0.f, Dt, 12.f);
    if (HeadPreviewMID)
    {
        HeadPreviewMID->SetVectorParameterValue(TEXT("Color"), HeadPaintColor);
        HeadPreviewMID->SetScalarParameterValue(TEXT("Glow"),  HeadPreviewGlow);
    }
}

void APTLobbyPlayerController::UpdateHeadCam()
{
    if (!HeadCam) return;

    // Foco: en modo "pintar cuerpo" apunta al CENTRO DEL PERSONAJE (más lejos, para encuadrar todo
    // el cuerpo); si no, al volumen de la cabeza.
    FVector Center;
    float   Dist;
    if (bBodyPaintMode)
    {
        const APawn* P = GetPawn();
        Center = P ? P->GetActorLocation() : (HeadVolume ? HeadVolume->GetActorLocation() : FVector::ZeroVector);
        Dist   = BodyCamDistance;
    }
    else
    {
        if (!HeadVolume) return;
        Center = HeadVolume->GetActorLocation();
        Dist   = HeadCamDistance;
    }

    const FVector Dir = FRotator(HeadOrbitPitch, HeadOrbitYaw, 0.f).Vector(); // dirección de mirada
    DesiredCamLoc = Center - Dir * Dist;
    DesiredCamRot = Dir.Rotation();

    // La primera vez (al entrar) se coloca de una, sin blend (si no arrancaría viajando desde el origen).
    if (!bHeadCamInit)
    {
        HeadCam->SetActorLocation(DesiredCamLoc);
        HeadCam->SetActorRotation(DesiredCamRot);
        bHeadCamInit = true;
    }
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

float APTLobbyPlayerController::GetPaintBudget01() const
{
    const APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn());
    if (!Char || MaxPaintChunks <= 0) return 0.f;
    const int32 Chunks = FMath::DivideAndRoundUp(Char->GetPaintPngBytes(), 8 * 1024);
    return FMath::Clamp((float)Chunks / (float)MaxPaintChunks, 0.f, 1.f);
}
bool APTLobbyPlayerController::IsPaintBudgetFull() const
{
    const APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn());
    if (!Char) return false;
    const int32 Chunks = FMath::DivideAndRoundUp(Char->GetPaintPngBytes(), 8 * 1024);
    return Chunks >= MaxPaintChunks;
}

void APTLobbyPlayerController::OnHeadStampPressed()
{
    if (!bHeadSculptMode) return;
    if (bDiscardPopupOpen) return; // popup abierto: los clicks son para sus botones, no para esculpir
    if (bHeadEyesTool) { PlaceEyeAtCursor(); return; } // ojos: un ojo por click (no continuo)

    APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn());
    const bool bPainting = bBodyPaintMode || HeadEditMode == EPTEditMode::Paint;

    // Límite de pintura: si ya llegaste al máximo replicable, no dejar pintar más (hay que deshacer
    // o resetear para liberar). Add/Erase (geometría) NO se bloquean.
    if (bPainting && IsPaintBudgetFull())
    {
        if (GEngine) GEngine->AddOnScreenDebugMessage(7010, 2.f, FColor(255, 90, 90),
            TEXT("Límite de pintura alcanzado — deshacé (Backspace) o reseteá para pintar más."));
        return; // no arranca el trazo
    }

    bHeadStamping = true;

    if (bBodyPaintMode)
    {
        // Trazo de pintura del CUERPO: snapshot para el undo (el body es su propia pila, aparte).
        if (Char) Char->PushBodyPaintUndo();
    }
    else if (HeadEditMode == EPTEditMode::Paint)
    {
        // Trazo de pintura de la CABEZA: snapshot para el undo.
        if (Char) Char->PushHeadPaintUndo();
        HeadUndoKinds.Add(1);
    }
    else if (HeadVolume && (HeadEditMode == EPTEditMode::Add || HeadEditMode == EPTEditMode::Erase))
    {
        // ¿Trazo de DETALLE? (Add + Alt): nueva CAPA aparte que NO se fusiona con la cabeza
        // (lentes/bigote/etc.). Erase nunca es detalle. Se latchea para todo el trazo.
        bHeadStrokeIsDetail = (HeadEditMode == EPTEditMode::Add && bHeadSurfaceSnap);
        if (bHeadStrokeIsDetail)
        {
            HeadVolume->Multicast_BeginDetailLayer_Implementation(); // capa nueva (el undo la saca entera)
        }
        else
        {
            HeadVolume->Multicast_BeginStroke_Implementation(); // trazo normal sobre la base
            bHeadStrokeActive = true;
        }
        // 0 = geometría: el undo de la cabeza delega en el volumen, que decide si deshace base o capa.
        HeadUndoKinds.Add(0);
    }
    while (HeadUndoKinds.Num() > 32) HeadUndoKinds.RemoveAt(0);
}
void APTLobbyPlayerController::OnHeadStampReleased()
{
    bHeadStamping = false;
    bHeadStrokePlaneLocked = false; // soltó: liberar el plano congelado del Alt-detalle
    bHeadStrokeIsDetail    = false;
    if (bHeadStrokeActive && HeadVolume)
    {
        HeadVolume->Multicast_EndStroke_Implementation(); // cierra el trazo (queda en la pila de undo)
        bHeadStrokeActive = false;
    }
    // Peso final del trazo de pintura de la cabeza (para la barra / el corte por presupuesto).
    PaintBudgetAccum = 0.f;
    if (!bBodyPaintMode && !bHeadEyesTool && HeadEditMode == EPTEditMode::Paint)
        if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn())) Char->RecomputeHeadPaintBytes();
}

void APTLobbyPlayerController::OnHeadRotatePressed()
{
    if (!bHeadSculptMode || !HeadToolUsesShapes()) return; // solo rota cuando hay forma (Agregar)
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    if (Now - LastHeadWheelPressTime <= 0.35f) // doble click de rueda → reset de rotación
    {
        HeadStampRotation = FRotator::ZeroRotator;
        HeadPreviewSize   = -1.f; // forzar reconstruir/reorientar el preview
    }
    LastHeadWheelPressTime = Now;
    bHeadRotatingShape = true;
    bHeadRotateHasFrozen = false;                       // el próximo tick congela el punto del sello
    GetMousePosition(HeadRotateCursorX, HeadRotateCursorY); // para fijar el cursor mientras rotás
}
void APTLobbyPlayerController::OnHeadRotateReleased()
{
    bHeadRotatingShape = false;
    bHeadRotateHasFrozen = false;
}

void APTLobbyPlayerController::OnHeadClearPressed()
{
    if (!bHeadSculptMode) return;
    bHeadClearHeld    = true;
    HeadClearHoldTime = 0.f;
    bHeadClearFired   = false;
}
void APTLobbyPlayerController::OnHeadClearReleased()
{
    if (!bHeadSculptMode) return;
    // Toque corto (soltó antes del umbral y NO llegó a resetear) = UNDO individual por contexto.
    // Backspace no tiene auto-repetición acá (verificado por log), así que Released es confiable.
    if (bHeadClearHeld && !bHeadClearFired && HeadClearHoldTime < HeadUndoTapMaxTime)
    {
        APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn());
        if (bBodyPaintMode || bHeadSculptBodyOnly)
        {
            if (Char) Char->UndoBodyPaint();
        }
        else if (HeadUndoKinds.Num() > 0)
        {
            const uint8 Kind = HeadUndoKinds.Last();
            bool bDone = false;
            if      (Kind == 0 && HeadVolume) { HeadVolume->Multicast_Undo_Implementation(); bDone = true; }
            else if (Kind == 1 && Char)       { bDone = Char->UndoHeadPaint(); }
            else if (Kind == 3)               { if (HeadEyes.Num() > 0) { HeadEyes.Pop(); RebuildEyesLiveMesh(); } bDone = true; }
            if (bDone) HeadUndoKinds.Pop();
        }
    }
    bHeadClearHeld    = false;
    HeadClearHoldTime = 0.f;
    bHeadClearFired   = false;
}
void APTLobbyPlayerController::OnHeadColorSave()
{
    // Solo con el picker abierto: guarda el color actual en el anillo (igual que el gameplay).
    if (!bHeadSculptMode || !bHeadColorActive) return;
    if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(HeadColorPicker)) CP->SaveCurrentColor();
}
void APTLobbyPlayerController::OnHeadScrollUp()
{
    if (!bHeadSculptMode) return;
    // Con el color picker abierto, la rueda ajusta el brillo del color (como el gameplay).
    if (bHeadColorActive)
    {
        if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(HeadColorPicker)) CP->QuickAdjustValue(+0.05f);
        return;
    }
    HeadBrushSize = FMath::Clamp(HeadBrushSize + 4.f, 6.f, 80.f);
}
void APTLobbyPlayerController::OnHeadScrollDown()
{
    if (!bHeadSculptMode) return;
    if (bHeadColorActive)
    {
        if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(HeadColorPicker)) CP->QuickAdjustValue(-0.05f);
        return;
    }
    HeadBrushSize = FMath::Clamp(HeadBrushSize - 4.f, 6.f, 80.f);
}
void APTLobbyPlayerController::OnHeadToggleBodyPaint()
{
    // En modo edición de un SLOT DE CUERPO no se puede salir del foco del cuerpo (no hay cabeza que editar).
    if (bHeadSculptBodyOnly) return;
    // Solo tiene sentido con la herramienta Paint (no Ojos): alterna cabeza ↔ cuerpo.
    if (!bHeadSculptMode || bHeadEyesTool || HeadEditMode != EPTEditMode::Paint) return;
    bBodyPaintMode = !bBodyPaintMode;
    bHeadStamping  = false; // no arrastrar pintura al cambiar de foco
    UpdateHeadCam();        // reencuadra al cuerpo o a la cabeza
}

// En modo edición de un slot de CUERPO (bHeadSculptBodyOnly) SOLO se puede pintar: no hay volumen de
// cabeza, así que Agregar/Borrar/Ojos dejarían el modo trabado. Se ignoran las otras herramientas.
void APTLobbyPlayerController::OnHeadModeAdd()   { if (bHeadSculptMode && !bHeadSculptBodyOnly) { if (bBodyPaintMode) { bBodyPaintMode = false; UpdateHeadCam(); } HeadEditMode = EPTEditMode::Add;   bHeadEyesTool = false; } }
void APTLobbyPlayerController::OnHeadModeErase()
{
    if (!bHeadSculptMode || bHeadSculptBodyOnly) return;
    // Entrar en Borrar arranca siempre en Esfera (igual que el gameplay): es la forma esperada
    // para corregir, y una forma rara heredada de Agregar haría el primer borrado confuso.
    if (bBodyPaintMode) { bBodyPaintMode = false; UpdateHeadCam(); } // Borrar es siempre en la cabeza
    if (HeadEditMode != EPTEditMode::Erase) HeadStampShape = EPTStampShape::Sphere;
    HeadEditMode = EPTEditMode::Erase; bHeadEyesTool = false;
}
void APTLobbyPlayerController::OnHeadModePaint() { if (bHeadSculptMode) { HeadEditMode = EPTEditMode::Paint; bHeadEyesTool = false; } } // Paint arranca en la cabeza; SHIFT lleva al cuerpo
void APTLobbyPlayerController::OnHeadModeEyes()  { if (bHeadSculptMode && !bHeadSculptBodyOnly) { if (bBodyPaintMode) { bBodyPaintMode = false; UpdateHeadCam(); } bHeadEyesTool = true; } }
void APTLobbyPlayerController::OnHeadCycleShape()
{
    if (!bHeadSculptMode || bHeadEyesTool || bHeadSculptBodyOnly) return; // ojos = esfera; en body-only no hay formas
    switch (HeadStampShape)
    {
    case EPTStampShape::Sphere:   HeadStampShape = EPTStampShape::Cube;     break;
    case EPTStampShape::Cube:     HeadStampShape = EPTStampShape::Cylinder; break;
    case EPTStampShape::Cylinder: HeadStampShape = EPTStampShape::TriPrism; break;
    default:                      HeadStampShape = EPTStampShape::Sphere;   break;
    }
    HeadStampRotation = FRotator::ZeroRotator; // forma nueva arranca sin rotar (como el gameplay)
    HeadPreviewSize = -1.f; // forzar reconstruir el mesh del preview con la nueva forma
}

void APTLobbyPlayerController::PlaceEyeAtCursor()
{
    if (!HeadVolume) return;
    FVector Pt, Nrm;
    if (!GetHeadStampPoint(Pt, Nrm)) return; // sobre la malla (raymarch a la superficie)
    // El centro del ojo cae sobre la superficie → la mitad de la esfera queda hundida en la arcilla.
    const FVector Local = HeadVolume->GetActorTransform().InverseTransformPosition(Pt);
    HeadEyes.Add(FVector4(Local.X, Local.Y, Local.Z, HeadBrushSize * 0.5f));
    RebuildEyesLiveMesh();
    HeadUndoKinds.Add(3); // los ojos también entran al undo (Backspace saca el último)
    while (HeadUndoKinds.Num() > 32) HeadUndoKinds.RemoveAt(0);
}

void APTLobbyPlayerController::RebuildEyesLiveMesh()
{
    if (!HeadEyesLiveMesh) return;
    HeadEyesLiveMesh->ClearAllMeshSections();
    if (HeadEyes.Num() == 0) return;

    UStaticMesh* EyeMesh = nullptr; float EyeBase = 50.f;
    if (APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn())) { EyeMesh = C->HeadEyeMesh; EyeBase = C->HeadEyeBaseSize; }
    const FPTHeadSection S = APTLobbyCharacter::BuildEyesSection(HeadEyes, EyeMesh, EyeBase);
    if (S.Verts.Num() == 0) return;
    const TArray<FProcMeshTangent> NoTangents;
    HeadEyesLiveMesh->CreateMeshSection(0, S.Verts, S.Tris, S.Normals, S.UVs, S.Colors, NoTangents, false);
    // Material de ojos del personaje (si hay); si no, el de preview Add.
    UMaterialInterface* Mat = nullptr;
    if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn())) Mat = Char->EyeMaterial;
    if (!Mat) Mat = HeadPreviewMatAdd;
    if (Mat) HeadEyesLiveMesh->SetMaterial(0, Mat);
}

void APTLobbyPlayerController::OnHeadColorPickPressed()
{
    if (!bHeadSculptMode || !HeadColorPickerClass || HeadColorPicker) return;
    HeadColorPicker = CreateWidget<UUserWidget>(this, HeadColorPickerClass);
    if (!HeadColorPicker) return;
    HeadColorPicker->AddToViewport(10);
    // GameAndUI sin captura: la rueda del picker recibe el cursor (como el gameplay).
    SetInputMode(FInputModeGameAndUI());
    SetShowMouseCursor(true);
    bHeadColorActive = true;
    bHeadStamping    = false; // no esculpir mientras elegís color
}

void APTLobbyPlayerController::OnHeadColorPickReleased()
{
    if (!bHeadColorActive) return;
    bHeadColorActive = false;

    // Confirmar el color (swatch guardado o rueda) y tomarlo para la brocha.
    if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(HeadColorPicker))
    {
        CP->ConfirmQuickPick();          // deja CP->CurrentColor en el color final
        HeadPaintColor = CP->CurrentColor;
    }
    if (HeadColorPicker) { HeadColorPicker->RemoveFromParent(); HeadColorPicker = nullptr; }

    // Igual que el gameplay: al elegir color, si estabas en Erase pasás a Paint (si estabas en
    // Add te quedás en Add y esculpís directo con el color).
    if (HeadEditMode == EPTEditMode::Erase) HeadEditMode = EPTEditMode::Paint;

    // Restaurar el input de esculpido (captura para el LMB).
    ApplyHeadSculptInputMode();
}

bool APTLobbyPlayerController::GetHeadStampPoint(FVector& OutWorld, FVector& OutNormal) const
{
    if (!HeadVolume || !HeadCam || !GetWorld()) return false;

    FVector Origin, Dir;
    if (!const_cast<APTLobbyPlayerController*>(this)->DeprojectMousePositionToWorld(Origin, Dir)) return false;

    // ADD + ALT DURANTE el trazo: dibujar sobre el PLANO CONGELADO (fijado al 1er sello), para hacer
    // trazos laterales sin que la arcilla trepe hacia la cámara (igual que el gameplay).
    if (HeadEditMode == EPTEditMode::Add && bHeadStrokeIsDetail && bHeadStrokePlaneLocked)
    {
        const float denom = FVector::DotProduct(Dir, HeadStrokePlaneNormal);
        FVector Pf = HeadStrokePlaneOrigin;
        if (FMath::Abs(denom) > 1e-4f)
        {
            const float t = FVector::DotProduct(HeadStrokePlaneOrigin - Origin, HeadStrokePlaneNormal) / denom;
            if (t > 0.f) Pf = Origin + Dir * t;
        }
        OutNormal = -Dir;
        OutWorld  = HeadVolume->ClampInsideCanvas(Pf, 0.f);
        return true;
    }

    // PAINT y OJOS: pegar el cursor a la SUPERFICIE (raymarch). ADD lo hace SOLO con ALT (detallar de
    // cerca sobre la cabeza). Devuelve la normal (para apoyar el preview en el mesh).
    if (HeadEditMode == EPTEditMode::Paint || bHeadEyesTool
        || (HeadEditMode == EPTEditMode::Add && bHeadSurfaceSnap))
    {
        constexpr float StepSize = 6.f;
        constexpr int32 MaxSteps = 500;
        float prevD = HeadVolume->SampleWorldDensity(Origin);
        for (int32 i = 1; i <= MaxSteps; ++i)
        {
            const FVector P = Origin + Dir * (StepSize * i);
            const float   d = HeadVolume->SampleWorldDensity(P);
            if (prevD <= 0.f && d > 0.f) // cruce aire→sólido
            {
                FVector lo = P - Dir * StepSize, hi = P;
                for (int32 j = 0; j < 5; ++j)
                {
                    const FVector mid = (lo + hi) * 0.5f;
                    (HeadVolume->SampleWorldDensity(mid) > 0.f ? hi : lo) = mid;
                }
                const FVector Surf = (lo + hi) * 0.5f;
                const float E = HeadVolume->VoxelSize * 0.5f;
                FVector Nn(
                    HeadVolume->SampleWorldDensity(Surf + FVector(E,0,0)) - HeadVolume->SampleWorldDensity(Surf - FVector(E,0,0)),
                    HeadVolume->SampleWorldDensity(Surf + FVector(0,E,0)) - HeadVolume->SampleWorldDensity(Surf - FVector(0,E,0)),
                    HeadVolume->SampleWorldDensity(Surf + FVector(0,0,E)) - HeadVolume->SampleWorldDensity(Surf - FVector(0,0,E)));
                Nn = (-Nn).GetSafeNormal();
                OutNormal = Nn.IsNearlyZero() ? -Dir : Nn;
                OutWorld  = Surf;
                return true;
            }
            prevD = d;
        }
        // Sin superficie: Paint/Ojos no muestran nada; Add+Alt cae al plano de abajo (primer blob).
        if (HeadEditMode != EPTEditMode::Add) return false;
    }

    // ADD/ERASE normal (estilo SculptrVR): el sello cae sobre un PLANO a la profundidad de la cabeza,
    // perpendicular a la cámara (no trepa). Se CLAMPEA al interior del área desde el pivot (mitad de
    // la esfera adentro), así la brocha choca con las paredes/piso/techo y no se sale.
    const FVector C = HeadVolume->GetActorLocation();      // centro de la cabeza (punto del plano)
    const FVector N = HeadCam->GetActorForwardVector();    // normal del plano = mirada de la cámara
    const float denom = FVector::DotProduct(Dir, N);
    if (FMath::Abs(denom) < 1e-4f) return false;           // rayo casi paralelo al plano
    const float t = FVector::DotProduct(C - Origin, N) / denom;
    if (t <= 0.f) return false;                            // el plano está detrás del cursor
    OutWorld  = HeadVolume->ClampInsideCanvas(Origin + Dir * t, 0.f);
    OutNormal = -N;                                        // hacia la cámara
    return true;
}

// ── Modo esculpir cabeza (tecla G) ───────────────────────────────────────────

void APTLobbyPlayerController::ToggleHeadSculptMode()
{
    if (!IsLocalController()) return;
    if (bHeadSculptMode) ExitHeadSculpt();
    else                 EnterHeadSculpt();
}

// ── Locker ─────────────────────────────────────────────────────────────────────
void APTLobbyPlayerController::SetupLockerCam()
{
    APawn* P = GetPawn();
    if (!P || !GetWorld()) return;
    if (!LockerCam)
    {
        FActorSpawnParameters SP; SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        LockerCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SP);
    }
    UpdateLockerCam(); // ubica siguiendo al personaje, con dirección fija (no atachada → no rota con él)
}

void APTLobbyPlayerController::UpdateLockerCam()
{
    APawn* P = GetPawn();
    if (!LockerCam || !P) return;
    // Solo desplaza la cámara con el personaje; el offset y la rotación son FIJOS EN EL MUNDO, así el
    // personaje puede rotar/moverse sin que la cámara gire (siempre mira al mismo fondo).
    LockerCam->SetActorLocationAndRotation(P->GetActorLocation() + LockerCamOffset, LockerCamRotation);
}

void APTLobbyPlayerController::OpenLocker()
{
    if (!IsLocalController() || !LockerWidgetClass || bHeadSculptMode) return;

    // Colapsar el menú principal (Play/Settings/etc.) → el Locker ocupa toda la pantalla.
    if (ActiveOverlay) ActiveOverlay->SetVisibility(ESlateVisibility::Collapsed);

    // Cámara atachada al personaje (te movés con WASD por el nivel y el personaje queda en el encuadre).
    SetupLockerCam();
    if (LockerCam) SetViewTargetWithBlend(LockerCam, LockerCamBlend);
    SetIgnoreMoveInput(false); // poder caminar por el nivel

    // Miniatura del slot "Default" (slot 0): si estás mostrando el look base y aún no tiene foto, capturarla.
    if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn()))
        if (UPTLockerSubsystem* L = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr)
        {
            if (L->GetEquippedHead() == 0 && L->IsHeadSlotDefault(0) && L->GetHeadThumb(0).Num() == 0)
            { TArray<uint8> T; if (Char->CaptureLookThumbnailPNG(T, /*bHeadFocus=*/true,  256)) L->SetHeadThumb(0, T); }
            if (L->GetEquippedBody() == 0 && L->IsBodySlotDefault(0) && L->GetBodyThumb(0).Num() == 0)
            { TArray<uint8> T; if (Char->CaptureLookThumbnailPNG(T, /*bHeadFocus=*/false, 256)) L->SetBodyThumb(0, T); }
        }

    // Widget a pantalla completa + foco de teclado (navegar/atajos).
    if (!LockerWidget) LockerWidget = CreateWidget<UPTLockerWidget>(this, LockerWidgetClass);
    if (!LockerWidget) return;
    if (!LockerWidget->IsInViewport()) LockerWidget->AddToViewport(50);
    LockerWidget->SetVisibility(ESlateVisibility::Visible);
    LockerWidget->RefreshSlots();
    LockerWidget->SetKeyboardFocus();

    bLockerOpen = true;
}

void APTLobbyPlayerController::CloseLocker()
{
    if (LockerWidget) { LockerWidget->RemoveFromParent(); LockerWidget = nullptr; }
    if (LockerCam) { LockerCam->Destroy(); LockerCam = nullptr; }
    bLockerOpen = false;
    if (ActiveOverlay) ActiveOverlay->SetVisibility(ESlateVisibility::Visible); // vuelve el menú
    SetupDioramaView(); // cámara fija del lobby (con blend a 0)
}

void APTLobbyPlayerController::EquipHeadSlot(int32 Idx)
{
    if (UPTLockerSubsystem* L = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr)
        L->EquipHead(Idx);
    if (APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn())) C->LoadHead(); // aplica + replica lo equipado
}
void APTLobbyPlayerController::EquipBodySlot(int32 Idx)
{
    if (UPTLockerSubsystem* L = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr)
        L->EquipBody(Idx);
    if (APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn())) C->LoadHead();
}

void APTLobbyPlayerController::PreviewLookSlot(int32 Index, bool bHead)
{
    UPTLockerSubsystem* L = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr;
    APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn());
    if (!L || !C) return;
    // La parte que NO se está previsualizando queda en lo equipado.
    const int32 H = bHead ? Index : L->GetEquippedHead();
    const int32 B = bHead ? L->GetEquippedBody() : Index;
    C->ApplyLookPreview(H, B);
}

void APTLobbyPlayerController::RevertLookPreview()
{
    UPTLockerSubsystem* L = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr;
    APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn());
    if (!L || !C) return;
    C->ApplyLookPreview(L->GetEquippedHead(), L->GetEquippedBody()); // volver a lo realmente equipado
}

void APTLobbyPlayerController::EnterHeadSculptForSlot(int32 Idx)
{
    EditingHeadSlot = Idx; EditingBodySlot = -1; bHeadSculptBodyOnly = false;
    bReturnToLockerAfterEdit = bLockerOpen;
    if (LockerWidget) LockerWidget->SetVisibility(ESlateVisibility::Collapsed); // ocultar Locker al editar
    EnterHeadSculpt();
}
void APTLobbyPlayerController::EnterBodyPaintForSlot(int32 Idx)
{
    EditingBodySlot = Idx; EditingHeadSlot = -1; bHeadSculptBodyOnly = true;
    bReturnToLockerAfterEdit = bLockerOpen;
    if (LockerWidget) LockerWidget->SetVisibility(ESlateVisibility::Collapsed);
    EnterHeadSculpt(); // adentro respeta bHeadSculptBodyOnly (foco cuerpo, sin volumen de cabeza)
}

void APTLobbyPlayerController::EnterHeadSculpt()
{
    APawn* P = GetPawn();
    if (bHeadSculptMode || !HeadVolumeClass || !P || !GetWorld()) return;
    bHeadSculptMode = true;
    bDiscardPopupOpen = false;

    APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(P);

    // Personaje recto y quieto (sin baile ni jiggle del physics asset) mientras esculpís.
    // SetSculptPose evalúa la pose YA (RefreshBoneTransforms) → el socket queda recto ANTES de leerlo.
    if (Char) Char->SetSculptPose(true, HeadSculptPoseAnim);
    // Borrar la cabeza que ya tenía asignada: molesta para modelar una nueva desde cero.
    // Editando un slot de CUERPO: NO borrar la cabeza equipada (la seguís viendo mientras pintás).
    if (Char && !bHeadSculptBodyOnly) Char->ClearHeadMesh();
    // Flecha "hacia dónde mira" visible mientras esculpís.
    if (Char) Char->SetFacingArrowVisible(true);
    // Colapsar la UI del lobby para que el mouse llegue al esculpido (no lo agarre la UI).
    if (ActiveOverlay) ActiveOverlay->SetVisibility(ESlateVisibility::Collapsed);

    // El "banco de esculpido" cae JUSTO sobre el HeadSocket. Además el volumen se alinea a la
    // ROTACIÓN del socket: así el espacio en el que esculpís = el espacio del socket, y la cabeza
    // horneada NO queda rotada respecto a como la modelaste, camine el personaje hacia donde camine.
    FVector  Center   = P->GetActorLocation() + FVector(0, 0, HeadUpOffset);
    FRotator SpawnRot = FRotator::ZeroRotator;
    if (Char && Char->GetMesh() && Char->GetMesh()->DoesSocketExist(TEXT("HeadSocket")))
    {
        const FTransform ST = Char->GetMesh()->GetSocketTransform(TEXT("HeadSocket"), RTS_World);
        Center   = ST.GetLocation();
        SpawnRot = ST.Rotator();
    }

  // Todo el volumen de cabeza + su pintura + ojos SOLO se crean si editás una CABEZA. Para un slot de
  // CUERPO no hace falta (pintás la piel del personaje directamente).
  if (!bHeadSculptBodyOnly)
  {
    // Spawn DIFERIDO: la cabeza es chica (radio ~40), así que con el ColorVoxel por defecto (3) el
    // atlas de color queda a ~27 vóxeles de ancho → pixelado. Lo bajamos ANTES de que el volumen
    // inicialice el campo de color (BeginPlay) para tener color de alta resolución en la cabeza.
    const FTransform HeadXform(SpawnRot, Center);
    HeadVolume = GetWorld()->SpawnActorDeferred<APTSculptVolume>(
        HeadVolumeClass, HeadXform, nullptr, nullptr,
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (HeadVolume)
    {
        HeadVolume->ColorVoxel = 0.5f; // color de alta resolución (el mínimo efectivo del sistema)
        UGameplayStatics::FinishSpawningActor(HeadVolume, HeadXform);
    }

    // ¿RE-editar un slot que ya tiene estado CRUDO guardado? (Fase 2) Si sí, cargamos su geometría
    // y sus ojos en vez de empezar de cero. La pintura 2D se restaura del BakedBlob más abajo.
    UPTLockerSubsystem* Lk = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr;
    bool bReEdit = false;
    TArray<FVector4> LoadedEyes;
    if (HeadVolume && Lk && EditingHeadSlot >= 0)
    {
        const TArray<uint8>& RawState = Lk->GetHeadRawState(EditingHeadSlot);
        uint32 Magic = 0;
        if (RawState.Num() > 0)
        {
            FMemoryReader Ar(RawState, /*bIsPersistent=*/true);
            Ar << Magic;
            if (Magic == PT_HEADRAW_MAGIC)
            {
                TArray<uint8> FieldBytes;
                Ar << FieldBytes;
                Ar << LoadedEyes;
                if (HeadVolume->LoadFieldState(FieldBytes)) bReEdit = true;
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("[HeadReEdit] slot=%d rawBytes=%d magicOK=%d reEdit=%d"),
               EditingHeadSlot, RawState.Num(), (Magic == PT_HEADRAW_MAGIC) ? 1 : 0, bReEdit ? 1 : 0);
    }

    // Slot nuevo (sin crudo): bolita inicial de arcilla para tener algo que esculpir (color piel).
    if (HeadVolume && !bReEdit)
        HeadVolume->ApplyStamp(Center, EPTStampShape::Sphere, 40.f, EPTEditMode::Add, FLinearColor::White); // bolita inicial BLANCA

    // Pintado 2D de la cabeza (igual al cuerpo): centro FIJO de proyección esférica = centro del volumen
    // en espacio LOCAL. La malla viva usa el material que muestrea esa textura, así ves la pintura al toque.
    if (HeadVolume)
        if (APTLobbyCharacter* PaintChar = Cast<APTLobbyCharacter>(GetPawn()))
        {
            const FVector CenterLocal = HeadVolume->GetActorTransform().InverseTransformPosition(Center);

            // Re-editar: restaurar la textura de pintura desde el BakedBlob del slot (headPNG + centro).
            bool bPaintRestored = false;
            if (bReEdit && Lk)
            {
                const TArray<uint8>& Baked = Lk->GetHeadBaked(EditingHeadSlot);
                if (Baked.Num() > 0)
                {
                    TArray<FPTHeadSection> Secs; FVector BCenter; TArray<uint8> HeadPNG, BodyPNG;
                    if (PaintChar->ParseHeadBlob(Baked, Secs, BCenter, HeadPNG, BodyPNG) && HeadPNG.Num() > 0)
                    {
                        PaintChar->ApplyHeadPaintFromPNG(HeadPNG, BCenter);
                        bPaintRestored = true;
                    }
                }
            }
            if (!bPaintRestored)
                PaintChar->InitHeadPaint(CenterLocal); // slot nuevo → textura en blanco

            HeadPaintMID = PaintChar->CreateHeadPaintMID();
            PaintChar->RecomputeHeadPaintBytes(); // peso inicial (para la barra de presupuesto)
            PaintChar->RecomputeBodyPaintBytes();
            // El volumen usa este material al re-mallar (sin pelear con un set-por-tick → sin parpadeo).
            HeadVolume->ClayMaterialOverride = HeadPaintMID;
            if (HeadPaintMID)
                if (UProceduralMeshComponent* CM = HeadVolume->GetMeshComponent())
                    for (int32 s = 0; s < CM->GetNumSections(); ++s) CM->SetMaterial(s, HeadPaintMID);
        }

    // Herramienta de ojos + malla viva de ojos (pegada al volumen para verlos colocados). Si estamos
    // re-editando, arrancamos con los ojos guardados; si es un slot nuevo, vacío.
    bHeadEyesTool = false;
    HeadEyes = LoadedEyes; // vacío si no había crudo
    if (HeadVolume)
    {
        HeadEyesLiveMesh = NewObject<UProceduralMeshComponent>(HeadVolume, TEXT("HeadEyesLiveMesh"));
        HeadEyesLiveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HeadEyesLiveMesh->SetCastShadow(false);
        HeadEyesLiveMesh->SetupAttachment(HeadVolume->GetRootComponent());
        HeadEyesLiveMesh->RegisterComponent();
        RebuildEyesLiveMesh(); // dibujar los ojos restaurados (si los hay)
    }
  } // fin if(!bHeadSculptBodyOnly)
  else
  {
    // Modo edición de un slot de CUERPO: foco en el cuerpo, cargar la textura del slot (si existe).
    bBodyPaintMode = true;
    HeadEditMode   = EPTEditMode::Paint;
    bHeadEyesTool  = false;
    if (Char)
    {
        Char->InitCharacterPaint();
        if (UPTLockerSubsystem* L = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr)
        {
            const TArray<uint8>& PNG = L->GetBodyPNG(EditingBodySlot);
            if (PNG.Num() > 0) Char->ApplyBodyPaintFromPNG(PNG); // editar sobre lo que ya tenías
            else               Char->ClearBodyPaint();           // slot nuevo → lienzo limpio
        }
        Char->RecomputeBodyPaintBytes();
    }
  }

    // Actor de preview de la brocha (fantasma que sigue al cursor, como el gameplay).
    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    HeadPreviewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), Center, FRotator::ZeroRotator, SP);
    if (HeadPreviewActor)
    {
        HeadPreviewMesh = NewObject<UProceduralMeshComponent>(HeadPreviewActor, TEXT("HeadPreviewMesh"));
        HeadPreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HeadPreviewMesh->SetCastShadow(false);
        HeadPreviewMesh->RegisterComponent();
        HeadPreviewActor->SetRootComponent(HeadPreviewMesh);

        // Componente para el mesh propio opcional del preview (Add/Erase).
        HeadPreviewStatic = NewObject<UStaticMeshComponent>(HeadPreviewActor, TEXT("HeadPreviewStatic"));
        HeadPreviewStatic->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HeadPreviewStatic->SetCastShadow(false);
        HeadPreviewStatic->SetupAttachment(HeadPreviewMesh);
        HeadPreviewStatic->RegisterComponent();

        HeadPreviewActor->SetActorHiddenInGame(true);
    }
    HeadPreviewSize = -1.f;                    // fuerza reconstruir la malla en el primer tick
    HeadPreviewMode = EPTEditMode::Smooth;

    // Cámara ORBITAL: arranca mirando la cara del personaje (desde adelante). WASD la orbita.
    HeadCam = GetWorld()->SpawnActor<ACameraActor>(ACameraActor::StaticClass());
    HeadOrbitYaw   = P->GetActorRotation().Yaw + 180.f; // mirar desde el frente hacia el personaje
    HeadOrbitPitch = -8.f;
    bHeadCamInit   = false; // al entrar, colocar la cámara de una (sin blend desde el origen)
    UpdateHeadCam();
    if (HeadCam) SetViewTargetWithBlend(HeadCam, 0.35f);

    // Congelar el personaje; input con CAPTURA para que el LMB llegue al esculpido (no a la UI).
    SetIgnoreMoveInput(true);
    SetIgnoreLookInput(true);
    ApplyHeadSculptInputMode();

    // Hotbar del modo G (las 4 herramientas + leyenda de controles). Se muestra encima de todo,
    // la UI del lobby ya está colapsada. HitTestInvisible: no roba el mouse al esculpido.
    if (HeadHUDClass && !HeadHUD)
    {
        HeadHUD = CreateWidget<UPTHeadSculptHUDWidget>(this, HeadHUDClass);
        if (HeadHUD)
        {
            HeadHUD->AddToViewport();
            HeadHUD->SetVisibility(ESlateVisibility::HitTestInvisible);
            HeadHUD->Refresh(this);
        }
    }

    UE_LOG(LogTemp, Log, TEXT("[Lobby] Modo esculpir-cabeza: ON."));
}

// Flujo por TECLAS (en edición el cursor no sirve):
//   ENTER  → si el popup está abierto = Sí (guardar); si no = confirmar directo (guardar + equipar).
//   ESCAPE → si el popup está cerrado = abrirlo; si ya está abierto = No (descartar y salir).
void APTLobbyPlayerController::ConfirmHeadEdit()
{
    if (!bHeadSculptMode) return;
    // Con el popup abierto, las TECLAS ya no lo resuelven: solo se resuelve con CLICK en los botones
    // (evita que un Enter/Escape sin querer guarde o descarte). Enter directo (sin popup) sí confirma.
    if (bDiscardPopupOpen) return;
    ExitHeadSculpt(true); // Enter directo = confirmar (guardar + equipar)
}
void APTLobbyPlayerController::RequestHeadBack()
{
    if (!bHeadSculptMode) return;
    // Con el popup YA abierto, Escape lo CIERRA y vuelve a editar (ignora el popup). No descarta ni
    // aplica: eso solo se hace clickeando los botones (Aplicar y equipar / No guardar). Así un doble
    // Escape sin querer ya no borra la creación.
    if (bDiscardPopupOpen)
    {
        bDiscardPopupOpen = false;
        if (HeadHUD)
        {
            HeadHUD->ShowDiscardPopup(false);
            HeadHUD->SetVisibility(ESlateVisibility::HitTestInvisible); // volver a click-through para esculpir
        }
        ApplyHeadSculptInputMode(); // restaurar el input de esculpido (cursor + captura al click)
        return;
    }
    bDiscardPopupOpen = true;
    bHeadStamping     = false; // cortar cualquier trazo en curso al abrir el popup
    if (HeadHUD)
    {
        HeadHUD->ShowDiscardPopup(true);
        // El HUD se agregó como HitTestInvisible (para no robarle el mouse al esculpido): así NINGÚN
        // hijo, ni los botones, recibía clicks/hover. Al abrir el popup lo pasamos a
        // SelfHitTestInvisible → el root sigue sin bloquear, pero los HIJOS (los botones del popup)
        // SÍ reciben mouse. Al cerrar se sale de la edición (ExitHeadSculpt destruye el HUD).
        HeadHUD->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }

    // Foco en el popup: cursor visible y clicks que llegan a los BOTONES (NoCapture: el viewport no
    // captura el click durante el mouse-down). El esculpido queda bloqueado por bDiscardPopupOpen
    // (ver OnHeadStampPressed), así clickear "afuera" no hace nada y no se pierde el foco del popup.
    // GameAndUI (no UIOnly) para que Enter/Esc sigan funcionando como alternativa a los botones.
    FInputModeGameAndUI Mode;
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
    SetShowMouseCursor(true);
    if (ULocalPlayer* LP = GetLocalPlayer())
        if (UGameViewportClient* VC = LP->ViewportClient)
        {
            VC->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
            VC->SetMouseLockMode(EMouseLockMode::DoNotLock);
        }
}
void APTLobbyPlayerController::ResolveHeadBack(bool bSave)
{
    if (!bHeadSculptMode) return;
    bDiscardPopupOpen = false;
    if (HeadHUD) HeadHUD->ShowDiscardPopup(false);
    ExitHeadSculpt(bSave);
}

void APTLobbyPlayerController::ExitHeadSculpt(bool bSaveChanges)
{
    if (!bHeadSculptMode) return;
    bHeadSculptMode = false;
    bHeadStamping = false;
    bBodyPaintMode = false;
    bHeadRotatingShape = false; bHeadClearHeld = false; HeadClearHoldTime = 0.f;
    bHeadStrokeActive = false; HeadStampRotation = FRotator::ZeroRotator;
    HeadPaintMID = nullptr;
    HeadUndoKinds.Reset();
    if (APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn())) C->ClearPaintUndo();

    // Guardar en el Locker (local) el slot que estabas editando, equiparlo, y replicar el look equipado.
    if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn()))
    {
        UPTLockerSubsystem* L = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr;

        Char->SetSculptPose(false);         // restaura el baile + jiggle
        Char->SetFacingArrowVisible(false); // ocultar la flecha (que no salga en la miniatura)

        if (!bSaveChanges)
        {
            // DESCARTAR: no guardar ni equipar el slot editado; restaurar el look que tenías equipado.
            Char->LoadHead();
        }
        else if (bHeadSculptBodyOnly)
        {
            // Slot de CUERPO: el cuerpo ya está pintado sobre el personaje → capturar miniatura y guardar.
            TArray<uint8> Thumb; Char->CaptureLookThumbnailPNG(Thumb, /*bHeadFocus=*/false, 256);
            TArray<uint8> PNG;
            if (L && EditingBodySlot >= 0 && Char->GetBodyPaintPNG(PNG))
            {
                L->SaveBodySlot(EditingBodySlot, PNG, Thumb);
                L->EquipBody(EditingBodySlot);
            }
            Char->LoadHead(); // aplica + replica (cabeza equipada + este cuerpo)
        }
        else if (HeadVolume)
        {
            // Slot de CABEZA: hornear (aplica la cabeza al personaje) → recién ahí capturar la miniatura.
            TArray<uint8> Blob;
            Char->BakeAndReplicateHead(HeadVolume->GetMeshComponent(), HeadVolume, HeadEyes, Blob);
            TArray<uint8> Thumb; Char->CaptureLookThumbnailPNG(Thumb, /*bHeadFocus=*/true, 256);

            // Estado CRUDO (Fase 2): campo SDF del volumen + ojos (locales). Permite volver a este
            // slot y SEGUIR esculpiendo desde donde lo dejaste (la pintura 2D se restaura del BakedBlob).
            TArray<uint8> Raw;
            {
                TArray<uint8> FieldBytes; HeadVolume->SaveFieldState(FieldBytes);
                FMemoryWriter Ar(Raw, /*bIsPersistent=*/true);
                uint32 Magic = PT_HEADRAW_MAGIC; Ar << Magic;
                Ar << FieldBytes;
                Ar << HeadEyes;
            }

            const int32 Slot = (EditingHeadSlot >= 0) ? EditingHeadSlot : 0; // fallback slot 0
            if (L && Blob.Num() > 0)
            {
                L->SaveHeadSlot(Slot, Blob, Raw, Thumb); // ahora SÍ guardamos el crudo (Fase 2)
                L->EquipHead(Slot);
            }
            // Subir el blob horneado (ya incluye el cuerpo pintado en esta sesión).
            if (Blob.Num() > 0)
                if (APTPlayerState* PS = Char->GetPlayerState<APTPlayerState>()) PS->UploadHead(Blob);
        }
    }
    EditingHeadSlot = -1; EditingBodySlot = -1; bHeadSculptBodyOnly = false;

    // Cerrar el color picker si quedó abierto.
    if (HeadColorPicker) { HeadColorPicker->RemoveFromParent(); HeadColorPicker = nullptr; }
    bHeadColorActive = false;
    bHeadEyesTool    = false;
    HeadEyes.Reset();
    HeadEyesLiveMesh = nullptr; // era componente del volumen; se destruye con él

    if (HeadVolume)       { HeadVolume->Destroy();       HeadVolume = nullptr; }
    if (HeadCam)          { HeadCam->Destroy();          HeadCam = nullptr; }
    if (HeadPreviewActor) { HeadPreviewActor->Destroy(); HeadPreviewActor = nullptr; HeadPreviewMesh = nullptr; }

    // Sacar la hotbar del modo G.
    if (HeadHUD) { HeadHUD->RemoveFromParent(); HeadHUD = nullptr; }

    SetIgnoreMoveInput(false);
    ApplyDioramaInputMode(); // restaura cursor/input del lobby

    if (bReturnToLockerAfterEdit)
    {
        // Volver a la vista del Locker (cámara atachada) y re-mostrar el widget con el slot ya lleno.
        // La pose normal (animada) ya se restauró arriba con SetSculptPose(false).
        bReturnToLockerAfterEdit = false;
        SetupLockerCam(); // re-atar por las dudas (el pawn pudo recrearse)
        if (LockerCam) SetViewTargetWithBlend(LockerCam, LockerCamBlend);
        if (LockerWidget)
        {
            LockerWidget->SetVisibility(ESlateVisibility::Visible);
            LockerWidget->RefreshSlots();
            LockerWidget->SetKeyboardFocus();
        }
    }
    else
    {
        // Restaurar la UI del lobby y la cámara fija (flujo legacy, sin Locker).
        if (ActiveOverlay) ActiveOverlay->SetVisibility(ESlateVisibility::Visible);
        SetupDioramaView();
    }

    UE_LOG(LogTemp, Log, TEXT("[Lobby] Modo esculpir-cabeza: OFF."));
}

void APTLobbyPlayerController::OnPressedStartGame()
{
    Server_RequestStartGame();
}

void APTLobbyPlayerController::ToggleEscapeMenu(const FInputActionValue& Value)
{
    if (!IsLocalController() || !EscapeMenuWidgetClass) return;
    if (bHeadSculptMode) return; // en edición, Escape abre el popup de guardar/descartar, no el menú
    if (bLockerOpen)     return; // en el Locker, Escape es "Back" (lo maneja el widget)

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
