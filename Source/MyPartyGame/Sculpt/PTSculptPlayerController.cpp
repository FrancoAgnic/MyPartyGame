#include "PTSculptPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h" // SetHardwareCursor (cursor dot del radial)
#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "../UI/PTColorPickerWidget.h"
#include "PTShapeRadialWidget.h"
#include "../Lobby/PTLobbyEscapeMenuWidget.h"
#include "../Lobby/PTLobbyCharacter.h"
#include "PTSculptPlane.h"
#include "../PTInputBindings.h"
#include "../Lobby/PTPlayerState.h"
#include "../PTGameUserSettings.h"
#include "../PTTextTable.h"
#include "../Multiplayer/MultiplayerSessionsSubsystem.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Culture.h"
#include "Engine/Engine.h"
#include "PTSculptGameMode.h"
#include "PTSculptGameState.h"
#include "PTGameplayHUDWidget.h"
#include "PTSculptSoundComponent.h"
#include "../PTSpectatorComponent.h"
#include "../PTGameInstance.h"
#include "Misc/Compression.h"

APTSculptPlayerController::APTSculptPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = false;
    SculptSounds = CreateDefaultSubobject<UPTSculptSoundComponent>(TEXT("SculptSounds"));
}

void APTSculptPlayerController::AcknowledgePossession(APawn* P)
{
    Super::AcknowledgePossession(P);
    // Lvl-01 es un vacío sin piso caminable → arrancar volando (no caminar/saltar). Acá en el
    // cliente local activa el bFlying para que el input de vuelo funcione; el servidor ya dejó la
    // copia autoritativa en vuelo (StartPawnFlying). Solo pasa en Lvl-01: el lobby usa
    // APTLobbyPlayerController y conserva el caminar.
    if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(P))
        Char->ApplyGameplayMovementMode(); // vuelo obligatorio + sin orientar al movimiento

    // Avisar al servidor mi idioma (para ver la palabra a adivinar EN MI IDIOMA).
    PushLanguageToServer();
}

void APTSculptPlayerController::PushLanguageToServer()
{
    if (!IsLocalController()) return;

    // El idioma sale de NUESTRA preferencia guardada, no de la cultura del motor:
    // SetCurrentCulture("en") falla en silencio mientras no haya datos de localización.
    FString Lang = TEXT("es");
    if (const UPTGameUserSettings* S = UPTGameUserSettings::Get()) Lang = S->GetLanguageCode();

    if (APTPlayerState* PS = GetPlayerState<APTPlayerState>())
    {
        PS->Server_SetLanguage(Lang);

        // De paso, reportar el nombre: si el "?Name=" de la URL de travel se perdió, este es el
        // único camino por el que el servidor (y con él los demás clientes) se entera de quién soy.
        if (UMultiplayerSessionsSubsystem* Sessions =
                GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
        {
            const FString MyName = Sessions->GetLocalPlayerDisplayName();
            if (!MyName.IsEmpty()) PS->Server_ReportDisplayName(MyName);
        }

        // MOMENTO 3 de la sincronización de cabezas: recién entramos a Lvl-01 después del travel.
        // Los pawns se crearon de cero, así que le pedimos al server TODAS las cabezas otra vez.
        PS->Server_RequestAllHeads();

        // Robustez: heartbeat que re-pide SOLO lo que falte, y con anti-flood (no re-pide mientras hay
        // una transferencia en curso ni más seguido que el cooldown). Cubre "a veces se veían, a veces
        // no" en el Lvl-01 sin desbordar el buffer confiable (que causaba el loop de conexión).
        if (IsLocalController() && GetWorld() && !GetWorldTimerManager().IsTimerActive(HeadSyncTimer))
            GetWorldTimerManager().SetTimer(HeadSyncTimer, this,
                &APTSculptPlayerController::HeadSyncHeartbeat, 4.f, /*loop=*/true, /*firstDelay=*/8.f);

        UE_LOG(LogTemp, Log, TEXT("[Lang] Idioma enviado al servidor: %s"), *Lang);
        return;
    }

    // El PlayerState todavía no llegó por replicación → reintentar.
    if (UWorld* W = GetWorld())
    {
        FTimerHandle H;
        W->GetTimerManager().SetTimer(H, this, &APTSculptPlayerController::PushLanguageToServer, 0.5f, false);
    }
}

void APTSculptPlayerController::HeadSyncHeartbeat()
{
    if (APTPlayerState* PS = GetPlayerState<APTPlayerState>()) PS->RefreshHeadsIfMissing();
}

void APTSculptPlayerController::BeginPlay()
{
    Super::BeginPlay();

    SetInputMode(FInputModeGameOnly());

    // El MovementMappingContext se agrega en PlayerTick (no acá): en PIE el LocalPlayer
    // puede no estar listo en BeginPlay. Ver el reintento en PlayerTick.

    Volume = Cast<APTSculptVolume>(
        UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
    if (!Volume)
        UE_LOG(LogTemp, Warning, TEXT("[PTSculptPC] No APTSculptVolume in level!"));



    // Actor para la preview de la forma del stamp
    FActorSpawnParameters SP;
    SP.bNoFail = true;
    PreviewActor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(),
                                                   FVector::ZeroVector, FRotator::ZeroRotator, SP);
    if (PreviewActor)
    {
        PreviewMesh = NewObject<UProceduralMeshComponent>(PreviewActor, TEXT("PreviewMesh"));
        PreviewMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PreviewMesh->SetCastShadow(false);
        PreviewMesh->SetReceivesDecals(false); // que la sombra-decal no lo tape
        PreviewMesh->RegisterComponent();
        PreviewActor->SetRootComponent(PreviewMesh);
        if (PreviewMeshMaterial)
            PreviewMesh->SetMaterial(0, PreviewMeshMaterial);
        // Translúcido vs translúcido: UE ordena por objeto (distancia del pivote), no por píxel, así que
        // el boundary (M_SculptBoundary) a veces tapaba el preview aunque el preview esté DENTRO. Le damos
        // al preview una prioridad alta para que siempre se dibuje por encima del boundary (que va a -100).
        PreviewMesh->SetTranslucentSortPriority(10);

        // Mesh estático opcional (cuando el usuario asigna sus propios meshes).
        PreviewStaticMesh = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("PreviewStaticMesh"));
        PreviewStaticMesh->SetupAttachment(PreviewMesh);
        PreviewStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PreviewStaticMesh->SetCastShadow(false);
        PreviewStaticMesh->SetReceivesDecals(false);
        PreviewStaticMesh->SetTranslucentSortPriority(10); // igual que PreviewMesh: por encima del boundary
        PreviewStaticMesh->RegisterComponent();
        PreviewStaticMesh->SetVisibility(false);

        // Gizmo de ejes (modo eje). Mesh asignable, se muestra solo cuando aplica.
        AxisGizmo = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("AxisGizmo"));
        AxisGizmo->SetupAttachment(PreviewMesh);
        AxisGizmo->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        AxisGizmo->SetCastShadow(false);
        if (AxisGizmoMesh) AxisGizmo->SetStaticMesh(AxisGizmoMesh);
        AxisGizmo->SetReceivesDecals(false);
        AxisGizmo->RegisterComponent();
        AxisGizmo->SetVisibility(false);

        // Preview de superficie de Paint/Smooth (mesh + alineación a la superficie).
        PaintRing = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("PaintRing"));
        PaintRing->SetupAttachment(PreviewMesh);
        PaintRing->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PaintRing->SetCastShadow(false);
        PaintRing->SetReceivesDecals(false);
        PaintRing->RegisterComponent();
        PaintRing->SetVisibility(false);

        // Overlay X-ray: capa extra encima del preview de la ARCILLA (material base intacto). El gizmo de
        // ejes (grilla indicadora) queda FUERA: no debe verse a través de la geometría.
        if (PreviewOverlayMaterial)
        {
            PreviewMesh->SetOverlayMaterial(PreviewOverlayMaterial);
            PreviewStaticMesh->SetOverlayMaterial(PreviewOverlayMaterial);
            PaintRing->SetOverlayMaterial(PreviewOverlayMaterial);
        }

        // Sombra falsa: decal que se proyecta en el piso justo debajo del cursor.
        ShadowDecal = NewObject<UDecalComponent>(PreviewActor, TEXT("ShadowDecal"));
        ShadowDecal->SetupAttachment(PreviewMesh);
        if (ShadowDecalMaterial) ShadowDecal->SetDecalMaterial(ShadowDecalMaterial);
        ShadowDecal->RegisterComponent();
        ShadowDecal->SetVisibility(false);

        // Palito indicador de altura (piso → cursor). Si no se asignó mesh, usar el
        // cilindro básico del motor.
        HeightStick = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("HeightStick"));
        HeightStick->SetupAttachment(PreviewMesh);
        HeightStick->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HeightStick->SetCastShadow(false);
        HeightStick->SetReceivesDecals(false);
        {
            UStaticMesh* StickMesh = HeightStickMesh;
            if (!StickMesh)
                StickMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
            if (StickMesh) HeightStick->SetStaticMesh(StickMesh);
            if (HeightStickMaterial) HeightStick->SetMaterial(0, HeightStickMaterial);
        }
        HeightStick->RegisterComponent();
        HeightStick->SetVisibility(false);

        // Límite del área de esculpido: box con grilla (aparece cerca del cursor).
        BoundaryMesh = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("BoundaryMesh"));
        BoundaryMesh->SetupAttachment(PreviewMesh);
        BoundaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        BoundaryMesh->SetCastShadow(false);
        BoundaryMesh->SetReceivesDecals(false);
        {
            UStaticMesh* BoxMesh = BoundaryBoxMesh;
            if (!BoxMesh)
                BoxMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
            if (BoxMesh) BoundaryMesh->SetStaticMesh(BoxMesh);
        }
        BoundaryMesh->RegisterComponent();
        // Siempre detrás de los demás translúcidos (preview, ring, etc.): así el límite no tapa el
        // preview del Add que está adentro del volumen.
        BoundaryMesh->SetTranslucentSortPriority(-100);
        // El MID se crea DESPUÉS de registrar, si no el render se queda con el material
        // base (CursorPos en 0 → grilla estática) e ignora las updates del C++.
        if (BoundaryMaterial)
            BoundaryMID = BoundaryMesh->CreateDynamicMaterialInstance(0, BoundaryMaterial);
        BoundaryMesh->SetVisibility(false);
    }

    // HUD de la partida: solo el jugador local lo crea. Maneja fase/reloj/chat/elección
    // e input mode por su cuenta (ver UPTGameplayHUDWidget).
    if (IsLocalController() && GameplayHUDClass)
    {
        GameplayHUD = CreateWidget<UPTGameplayHUDWidget>(this, GameplayHUDClass);
        if (GameplayHUD) GameplayHUD->ShowHUD();
    }

    // Cámara espectador (dev-only): se maneja por comandos de consola.
    if (IsLocalController())
        Spectator = NewObject<UPTSpectatorComponent>(this, TEXT("SpectatorCam"));
    if (Spectator) Spectator->RegisterComponent();
}

// ── Comandos de consola dev (cámara espectador) ─────────────────────────────
void APTSculptPlayerController::PTSpectate()
{
    if (!Spectator) return;
    Spectator->Toggle();
    // El modo captura (nombres → "Player N") acompaña al vuelo libre.
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
        GI->SetCaptureMode(Spectator->IsActive());
    // Avisar al server para sacarme/reincorporarme a la partida (turnos, conteo, personaje visible).
    Server_SetSpectator(Spectator->IsActive());
}

void APTSculptPlayerController::Server_SetSpectator_Implementation(bool bInSpectator)
{
    if (APTPlayerState* PS = GetPlayerState<APTPlayerState>())
        PS->bIsDevSpectator = bInSpectator;
}

void APTSculptPlayerController::PTHideUI()
{
    bHudHidden = !bHudHidden;
    if (GameplayHUD)
        GameplayHUD->SetVisibility(bHudHidden ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
}

void APTSculptPlayerController::PTHideNames()
{
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
        GI->SetHideNames(!GI->AreNamesHidden());
}

void APTSculptPlayerController::PTRevealWord()
{
    if (GameplayHUD) GameplayHUD->ToggleRevealWord();
}

void APTSculptPlayerController::PTHideHotbar()
{
    if (GameplayHUD) GameplayHUD->ToggleHotbar();
}

void APTSculptPlayerController::PTSpecSpeed(float N)
{
    if (Spectator) Spectator->SetSpeedScale(N);
}

void APTSculptPlayerController::PTSpecSmooth(float N)
{
    if (Spectator) Spectator->SetSmoothSpeed(N);
}

void APTSculptPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent) return;

    // Stamp
    InputComponent->BindAction("Sculpt", IE_Pressed,  this, &APTSculptPlayerController::OnStampPressed);
    InputComponent->BindAction("Sculpt", IE_Released, this, &APTSculptPlayerController::OnStampReleased);

    // El resto de las teclas salen de PTInput (fuente de verdad única, compartida con la UI del
    // HUD). Nada de EKeys hardcodeados acá: si el jugador rebindea, con RefreshFromSettings +
    // volver a llamar a esto alcanza, y la UI ya lo refleja sola.
    const auto K = [](const TCHAR* Id) { return PTInput::GetKey(FName(Id)); };

    // Tamaño con rueda (la rueda no se rebindea, pero sale de la tabla igual).
    InputComponent->BindKey(EKeys::MouseScrollUp,   IE_Pressed, this, &APTSculptPlayerController::OnScrollUp);
    InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &APTSculptPlayerController::OnScrollDown);

    // Modos: Add / Erase / Paint / Ojos
    InputComponent->BindKey(K(TEXT("ModeAdd")),   IE_Pressed, this, &APTSculptPlayerController::SetModeAdd);
    InputComponent->BindKey(K(TEXT("ModeErase")), IE_Pressed, this, &APTSculptPlayerController::SetModeErase);
    InputComponent->BindKey(K(TEXT("ModePaint")), IE_Pressed, this, &APTSculptPlayerController::SetModePaint);
    InputComponent->BindKey(K(TEXT("ModeEyes")),  IE_Pressed, this, &APTSculptPlayerController::SetModeEyes);

    // Formas: MANTENER abre el menú radial; soltar selecciona el slot bajo el cursor (arrastrando).
    // Si no hay WBP radial asignado, el Pressed cae al comportamiento viejo (un toque cicla).
    InputComponent->BindKey(K(TEXT("CycleShape")), IE_Pressed,  this, &APTSculptPlayerController::OnShapeRadialPressed);
    InputComponent->BindKey(K(TEXT("CycleShape")), IE_Released, this, &APTSculptPlayerController::OnShapeRadialReleased);

    // Modo eje (dibujo recto sobre plano congelado): vertical / horizontal.
    // Ejes = HOLD: Pressed activa el plano, Released vuelve a Add.
    InputComponent->BindKey(K(TEXT("AxisVertical")),   IE_Pressed,  this, &APTSculptPlayerController::OnAxisVerticalPressed);
    InputComponent->BindKey(K(TEXT("AxisVertical")),   IE_Released, this, &APTSculptPlayerController::OnAxisVerticalReleased);
    InputComponent->BindKey(K(TEXT("AxisHorizontal")), IE_Pressed,  this, &APTSculptPlayerController::OnAxisHorizontalPressed);
    InputComponent->BindKey(K(TEXT("AxisHorizontal")), IE_Released, this, &APTSculptPlayerController::OnAxisHorizontalReleased);

    // Rotar el shape: mantener + arrastrar (doble click = reset).
    InputComponent->BindKey(K(TEXT("RotateShape")), IE_Pressed,  this, &APTSculptPlayerController::OnShapeRotatePressed);
    InputComponent->BindKey(K(TEXT("RotateShape")), IE_Released, this, &APTSculptPlayerController::OnShapeRotateReleased);

    // ALT (mantener) en Add: pegar el sello a la superficie de la arcilla ya dibujada (detallar de cerca).
    InputComponent->BindKey(EKeys::LeftAlt,  IE_Pressed,  this, &APTSculptPlayerController::OnSurfaceSnapPressed);
    InputComponent->BindKey(EKeys::LeftAlt,  IE_Released, this, &APTSculptPlayerController::OnSurfaceSnapReleased);
    InputComponent->BindKey(EKeys::RightAlt, IE_Pressed,  this, &APTSculptPlayerController::OnSurfaceSnapPressed);
    InputComponent->BindKey(EKeys::RightAlt, IE_Released, this, &APTSculptPlayerController::OnSurfaceSnapReleased);

    // Borrar TODA la escultura: mantener 3s (solo el escultor).
    InputComponent->BindKey(K(TEXT("ClearAll")), IE_Pressed,  this, &APTSculptPlayerController::OnClearAllPressed);
    InputComponent->BindKey(K(TEXT("ClearAll")), IE_Released, this, &APTSculptPlayerController::OnClearAllReleased);

    // Color rápido: mantener abre la rueda; soltar confirma. Guardar color con su tecla.
    InputComponent->BindKey(K(TEXT("ColorPick")), IE_Pressed,  this, &APTSculptPlayerController::OnColorPickPressed);
    InputComponent->BindKey(K(TEXT("ColorPick")), IE_Released, this, &APTSculptPlayerController::OnColorPickReleased);
    InputComponent->BindKey(K(TEXT("SaveColor")), IE_Pressed,  this, &APTSculptPlayerController::OnColorSavePressed);

    InputComponent->BindKey(K(TEXT("Pause")), IE_Pressed, this, &APTSculptPlayerController::OnPausePressed);
    InputComponent->BindKey(K(TEXT("Chat")),  IE_Pressed, this, &APTSculptPlayerController::OnOpenChat);
}

void APTSculptPlayerController::OnOpenChat()
{
    if (GameplayHUD) GameplayHUD->FocusChat();
}


void APTSculptPlayerController::OnPausePressed()
{
    if (!IsLocalController() || !PauseMenuClass) return;

    if (!EscapeMenu)
    {
        EscapeMenu = CreateWidget<UPTLobbyEscapeMenuWidget>(this, PauseMenuClass);
        if (EscapeMenu) EscapeMenu->AddToViewport(10);
    }
    // Navegación de dos niveles + manejo de input/cursor lo hace el propio widget.
    if (EscapeMenu) EscapeMenu->HandleEscape();
}

bool APTSculptPlayerController::IsEscapeMenuOpen() const
{
    return EscapeMenu && EscapeMenu->IsMenuOpen();
}

// ── Tick ─────────────────────────────────────────────────────────────────────

void APTSculptPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    // Teclas toggle del modo espectador (dev): H=HUD, N=nombres, J=hotbar. Solo mientras espectás,
    // para no pisar teclas del juego. Reusan las mismas funciones que los comandos de consola.
    if (Spectator && Spectator->IsActive())
    {
        if (WasInputKeyJustPressed(EKeys::H)) PTHideUI();
        if (WasInputKeyJustPressed(EKeys::N)) PTHideNames();
        if (WasInputKeyJustPressed(EKeys::J)) PTHideHotbar();
    }
    // F9 = screenshot en alta resolución (2x). Disponible siempre (para el trailer/capturas).
    if (WasInputKeyJustPressed(EKeys::F9)) ConsoleCommand(TEXT("HighResShot 2"), true);

    // Durante el seamless travel de vuelta al lobby, este PlayerController sigue tickeando mientras
    // el SculptVolume ya se está destruyendo. Aunque Volume es UPROPERTY, hay una ventana en la que
    // el actor quedó "pending kill" pero el puntero todavía no es null → limpiarlo acá evita que
    // GetStampPoint/ClampInsideCanvas corran sobre un volumen moribundo y crasheen (crash del
    // cliente al tocar "Back to Lobby", visto en logs 2026-08-19).
    if (Volume && !IsValid(Volume)) Volume = nullptr;

    // ── Menú de pausa (ESC) abierto: pausar el gameplay del mouse ──
    // Frena la cámara (el Look del character usa AddControllerYaw/PitchInput, que respetan
    // IgnoreLookInput) y el movimiento/vuelo (AddMovementInput respeta IgnoreMoveInput), dejando
    // la flecha solo para la UI. Se hace por TRANSICIÓN (bMenuInputLocked) para no desbalancear
    // el contador de IgnoreLook/MoveInput — así cualquier vía de cierre (ESC, Resume, Leave) se
    // refleja acá. El cursor/GameAndUI lo sostiene el HUD (ve IsEscapeMenuOpen en su bWantUI).
    const bool bMenuOpen = IsEscapeMenuOpen();
    if (bMenuOpen != bMenuInputLocked)
    {
        bMenuInputLocked = bMenuOpen;
        SetIgnoreLookInput(bMenuOpen);
        SetIgnoreMoveInput(bMenuOpen);
    }

    // ── Fases con UI que necesita el foco: elección de palabra (solo el escultor) y scoreboard
    //    de fin de partida (todos). Antes se podía mover la cámara/volar sobre esos paneles y el
    //    movimiento le robaba el foco a la UI. Igual que el menú de pausa: frená look+move por
    //    transición mientras dura la fase (contador balanceado con su propio flag).
    bool bPhaseWantsUILock = false;
    if (const APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr)
    {
        bPhaseWantsUILock =
            (G->TurnPhase == EPTTurnPhase::GameOver) ||
            (G->TurnPhase == EPTTurnPhase::ChoosingWord && G->IsLocalPlayerSculptor());

        // Al EMPEZAR tu turno de esculpir (transición a "sos el escultor"), resetear rotación y escala.
        const bool bMyTurn = (G->TurnPhase == EPTTurnPhase::Drawing) && G->IsLocalPlayerSculptor();
        if (bMyTurn && !bWasSculptorTurn)
        {
            StampRotation = FRotator::ZeroRotator;
            StampScale    = FVector::OneVector;
            bPreviewDirty = true;
        }
        bWasSculptorTurn = bMyTurn;
    }
    if (bPhaseWantsUILock != bGamePhaseInputLocked)
    {
        bGamePhaseInputLocked = bPhaseWantsUILock;
        SetIgnoreLookInput(bPhaseWantsUILock);
        SetIgnoreMoveInput(bPhaseWantsUILock);
    }

    // Radial de formas abierto: congelar cámara y movimiento (mismo lock por transición balanceado)
    // así arrastrar el mouse elige el slot sin girar la cámara — el foco queda en el radial.
    if (bShapeRadialActive != bShapeRadialInputLocked)
    {
        bShapeRadialInputLocked = bShapeRadialActive;
        SetIgnoreLookInput(bShapeRadialActive);
        SetIgnoreMoveInput(bShapeRadialActive);
    }

    // (La lista de controles con H ahora vive en la UI del HUD: ver UPTGameplayHUDWidget::SetControlsVisible.)

    // Rueda de color abierta (mantener RMB): seguir el cursor para elegir matiz/saturación.
    if (bQuickColorActive)
        if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(ColorPicker))
            CP->QuickPickTick();

    // Menú radial de formas abierto (mantener la tecla de forma): seguir el cursor para resaltar el slot.
    // Además, aplicar la forma resaltada EN VIVO para que el preview de la herramienta cambie mientras
    // hacés hover sobre cada slot (sin esperar a soltar). En la zona muerta se vuelve a la forma previa.
    if (bShapeRadialActive && ShapeRadial)
    {
        // Rueda del mouse → cambiar de PÁGINA de formas (arriba = anterior, abajo = siguiente).
        if (WasInputKeyJustPressed(EKeys::MouseScrollUp))   ShapeRadial->PrevPage();
        if (WasInputKeyJustPressed(EKeys::MouseScrollDown)) ShapeRadial->NextPage();

        ShapeRadial->UpdateSelection();
        EPTStampShape HoverShape;
        if (ShapeRadial->GetSelectedShape(HoverShape))
        {
            if (StampShape != HoverShape) SetShape(HoverShape);
        }
        else if (StampShape != ShapeBeforeRadial)
        {
            SetShape(ShapeBeforeRadial); // cursor en el centro → preview vuelve a la forma actual
        }
    }

    // Agregar el mapping context de movimiento acá (no en BeginPlay): en PIE el
    // LocalPlayer/subsistema puede no estar listo en BeginPlay, y entonces nunca se
    // agregaba → sin movimiento. Reintenta cada frame hasta que se puede.
    if (!bMovementCtxReady && MovementMappingContext)
    {
        if (ULocalPlayer* LP = GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Sub =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LP))
            {
                Sub->AddMappingContext(MovementMappingContext, 0);
                bMovementCtxReady = true;
            }
        }
    }

    // Re-buscar el Volume si no se encontró en BeginPlay (podía no estar disponible
    // todavía por timing de PIE / replicación). Solo itera mientras siga null.
    if (!Volume)
        Volume = Cast<APTSculptVolume>(
            UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));

    // Resaltar al escultor con el overlay amarillo (lo ven TODOS). Corre para todos
    // los clientes, antes del gate de escultor; solo se recalcula cuando cambia quién esculpe.
    {
        APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
        APlayerState* Sculptor = G ? G->CurrentSculptor : nullptr;
        if (Sculptor != LastSculptorHighlight.Get())
        {
            LastSculptorHighlight = Sculptor;
            UpdateSculptorHighlights();
        }
    }

    if (!Volume) return;

    // Solo el escultor del turno ve los previews de las herramientas. Para el resto —o con el
    // menú de pausa abierto— ocultar el actor de preview y no calcular/aplicar nada de la brocha.
    if (!CanLocalPlayerSculpt() || bMenuOpen)
    {
        if (PreviewActor) PreviewActor->SetActorHiddenInGame(true);
        bStrokeActive = false;
        bClearHeld    = false; // no dejar el "borrar todo" cargándose fuera de tu turno
        ClearHoldTime = 0.f;
        bStampOutsideCanvas = false; // que no quede el ícono de prohibido colgado
        // Se terminó tu turno con el modo eje activo → apagarlo (y destruir su plano con colisión,
        // para no dejarlo frenándote cuando ya no esculpís).
        if (bAxisLock && !CanLocalPlayerSculpt()) SetAxisMode(false, bAxisHorizontal);
        return;
    }
    if (PreviewActor) PreviewActor->SetActorHiddenInGame(false);

    // Borrar TODO: mantener BACKSPACE 3s (con cuenta regresiva en pantalla para que se entienda
    // que hay que sostenerlo y para poder arrepentirse soltando).
    // El feedback (círculo + contador) lo muestra el cuadrito del HUD leyendo GetClearHoldProgress.
    if (bClearHeld)
    {
        ClearHoldTime += DeltaTime;
        if (ClearHoldTime >= ClearHoldDuration)
        {
            bClearHeld    = false; // consumido: soltar después ya no dispara el "deshacer"
            ClearHoldTime = 0.f;
            Server_ClearSculpture();
            if (SculptSounds) SculptSounds->PlayUndoClearAll(Volume ? Volume->GetActorLocation()
                                                                     : (GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector));
        }
    }

    // Rotar el shape mientras se mantiene la rueda del mouse (la cámara está bloqueada).
    if (bRotatingShape)
    {
        float DX = 0.f, DY = 0.f;
        GetInputMouseDelta(DX, DY);
        if (DX != 0.f || DY != 0.f)
        {
            StampRotation.Yaw   += DX * ShapeRotateSpeed;
            StampRotation.Pitch += DY * ShapeRotateSpeed;
            StampRotation.Normalize();
        }
    }

    FVector Normal;
    FVector StampPos = GetStampPoint(Normal);

    // ¿Estás apuntando fuera de la zona de modelado? El HUD lo avisa con el ícono de prohibido.
    bStampOutsideCanvas = Volume && !Volume->IsInsideCanvas(StampPos);

    // Preview: actualizar cuando cambia forma, tamaño o modo (tool).
    if (PreviewMesh && (bPreviewDirty
        || CachedPreviewShape != StampShape
        || CachedPreviewSize  != StampSize
        || CachedPreviewScale != StampScale
        || CachedPreviewMode  != EditMode
        || CachedPreviewColor != CurrentPaintColor))
    {
        UpdatePreviewVisual();
        CachedPreviewShape = StampShape;
        CachedPreviewSize  = StampSize;
        CachedPreviewScale = StampScale;
        CachedPreviewMode  = EditMode;
        CachedPreviewColor = CurrentPaintColor;
        bPreviewDirty      = false;
    }

    // Brillo del preview mientras esculpís en Add (cada frame, barato: solo setea el MID).
    UpdatePreviewBrightness(DeltaTime);

    // Mover preview al punto de stamp. La rotación del actor = rotación del sello, así ves la
    // forma girada antes de colocarla (los demás componentes —gizmo, ring, sombra, límite— fijan
    // su propio transform WORLD cada tick, así que no les afecta).
    if (PreviewActor)
    {
        PreviewActor->SetActorLocation(StampPos);
        PreviewActor->SetActorRotation(StampRotation);
    }

    // Gizmo de ejes: visible solo en modo eje con la herramienta Add.
    if (AxisGizmo)
    {
        const bool bShowGizmo = bAxisLock && EditMode == EPTEditMode::Add && !bEyesTool;
        AxisGizmo->SetVisibility(bShowGizmo);
        if (bShowGizmo)
        {
            // Plano CONGELADO (fijado al activar el modo): el gizmo usa siempre AxisPlaneN.
            AxisGizmo->SetWorldLocation(StampPos);
            AxisGizmo->SetWorldRotation(AxisPlaneN.Rotation());
            const float Base = FMath::Max(PreviewMeshBaseSize, 1.f);
            AxisGizmo->SetWorldScale3D(FVector(StampSize / Base));
        }
    }

    // Decal indicador: solo Add/Erase (Paint y Smooth usan mesh de superficie).
    if (BrushDecalMaterial && (EditMode == EPTEditMode::Add || EditMode == EPTEditMode::Erase))
    {
        FRotator DecalRot = (-Normal).Rotation();
        UGameplayStatics::SpawnDecalAtLocation(
            GetWorld(), BrushDecalMaterial,
            FVector(StampSize * 0.5f),
            StampPos, DecalRot, 0.12f);
    }

    // Sombra falsa + palito de altura: trazar recto hacia abajo hasta lo primero que
    // haya debajo (la malla de arcilla o el piso del nivel) para medir la altura desde ahí.
    {
        FHitResult Down;
        FCollisionQueryParams QP;
        QP.bTraceComplex = true; // superficie real de la arcilla (ProceduralMesh)
        if (PreviewActor)                QP.AddIgnoredActor(PreviewActor);
        if (const APawn* Pw = GetPawn())  QP.AddIgnoredActor(Pw);
        // Arrancar el rayo 50 UU debajo del cursor: así no pega en la arcilla que estás
        // esculpiendo justo ahí (se veía raro) y mide desde la superficie de más abajo.
        const FVector DownStart = StampPos - FVector(0.f, 0.f, 50.f);
        const FVector DownEnd   = DownStart - FVector(0.f, 0.f, 100000.f);
        const bool bHit = GetWorld()->LineTraceSingleByChannel(Down, DownStart, DownEnd, ECC_Visibility, QP);

        // Decal de sombra: tamaño FIJO (ShadowSize), no escala con la brocha.
        if (ShadowDecal && ShadowDecalMaterial)
        {
            if (bHit)
            {
                ShadowDecal->SetVisibility(true);
                // Proyectar hacia abajo (eje X del decal = -Z mundo → Pitch -90).
                ShadowDecal->SetWorldLocationAndRotation(
                    Down.ImpactPoint + FVector(0.f, 0.f, 20.f), FRotator(-90.f, 0.f, 0.f));
                const float R = FMath::Max(1.f, ShadowSize);
                ShadowDecal->DecalSize = FVector(32.f, R, R); // (proyección fina, ancho, alto)
            }
            else ShadowDecal->SetVisibility(false);
        }

        // Palito de altura: del piso al cursor. Se estira/achica con la distancia.
        if (HeightStick)
        {
            const float H = bHit ? (StampPos.Z - Down.ImpactPoint.Z) : 0.f;
            if (bHit && H > 2.f)
            {
                HeightStick->SetVisibility(true);
                HeightStick->SetWorldLocation((Down.ImpactPoint + StampPos) * 0.5f);
                HeightStick->SetWorldRotation(FRotator::ZeroRotator);
                const float ZScale = H / FMath::Max(1.f, HeightStickMeshLength);
                HeightStick->SetWorldScale3D(FVector(HeightStickThickness, HeightStickThickness, ZScale));
            }
            else HeightStick->SetVisibility(false);
        }
    }

    // Límite del área: ubicar el box en el BoundsBox del volumen y pasarle el cursor al
    // material (la grilla aparece cerca del cursor). El cubo básico del motor mide 100³
    // (semi-extensión 50), así que la escala = extensión del box / 50.
    if (BoundaryMesh && BoundaryMID)
    {
        if (UBoxComponent* Box = Volume->FindComponentByClass<UBoxComponent>())
        {
            BoundaryMesh->SetVisibility(true);
            BoundaryMesh->SetWorldLocationAndRotation(Box->GetComponentLocation(), Box->GetComponentRotation());
            BoundaryMesh->SetWorldScale3D(Box->GetScaledBoxExtent() / 50.f);
            BoundaryMID->SetVectorParameterValue(TEXT("CursorPos"), StampPos);
        }
    }

    // Preview de superficie (Paint por shape + color; Smooth su propio mesh):
    // alineado a la normal, escalado con la brocha.
    if (PaintRing)
    {
        UStaticMesh* RingMesh = nullptr;
        bool  bTint     = false;
        bool  bEyeScale = false; // los ojos escalan por su propio radio/base
        if (bEyesTool)
        {
            // Preview del ojo apoyado sobre la malla (igual que en modo G): mesh del ojo o esfera.
            RingMesh = (Volume && Volume->EyeMesh) ? Volume->EyeMesh : PaintMeshSphere;
            bEyeScale = true;
        }
        else if (EditMode == EPTEditMode::Paint)
        {
            switch (StampShape)
            {
            case EPTStampShape::Sphere:   RingMesh = PaintMeshSphere;   break;
            case EPTStampShape::Cube:     RingMesh = PaintMeshCube;     break;
            case EPTStampShape::Cylinder: RingMesh = PaintMeshCylinder; break;
            case EPTStampShape::TriPrism: RingMesh = PaintMeshCone;     break;
            }
            bTint = true;
        }
        else if (EditMode == EPTEditMode::Smooth)
        {
            RingMesh = SmoothRingMesh;
        }

        // El preview se ve SIEMPRE (aunque estés fuera del lienzo): que no se pueda construir ahí
        // lo avisa el ícono de "prohibido" del centro de la pantalla, no la desaparición del preview.
        const bool bShow = (RingMesh != nullptr);
        PaintRing->SetVisibility(bShow);
        if (bShow)
        {
            // Rearmar si cambió el mesh O si cambió el estado de tinte (Paint ↔ Ojos/Smooth).
            if (CachedRingMesh != RingMesh || CachedRingTint != bTint)
            {
                PaintRing->SetStaticMesh(RingMesh);
                PaintRingMID = nullptr;
                if (bTint)
                {
                    // Paint: MID para teñir con el color del picker.
                    if (UMaterialInterface* M = PaintRing->GetMaterial(0))
                        PaintRingMID = PaintRing->CreateDynamicMaterialInstance(0, M);
                }
                else
                {
                    // Ojos/Smooth: sacar cualquier override (el MID tintado de Paint) para que el
                    // mesh use SU material original → el ojo siempre se ve igual, sin color.
                    PaintRing->SetMaterial(0, nullptr);
                }
                CachedRingMesh = RingMesh;
                CachedRingTint = bTint;
            }
            PaintRing->SetWorldLocation(StampPos);
            PaintRing->SetWorldRotation(FRotationMatrix::MakeFromZ(Normal).Rotator());
            float Scale;
            if (bEyeScale)
            {
                const float EyeBase = (Volume && Volume->EyeBaseSize > 1.f) ? Volume->EyeBaseSize : 50.f;
                Scale = (StampSize * 0.5f) / EyeBase; // mismo radio con que se coloca el ojo
            }
            else
            {
                Scale = StampSize / FMath::Max(PreviewMeshBaseSize, 1.f);
            }
            PaintRing->SetWorldScale3D(FVector(Scale));
            if (bTint && PaintRingMID) PaintRingMID->SetVectorParameterValue(TEXT("Color"), CurrentPaintColor);
        }
    }

    // Aplicar stamp si se está presionando. Se INTERPOLAN sellos entre la posición
    // del frame anterior y la actual → trazos fluidos y continuos (sin huecos al
    // mover rápido, sobre todo con brocha chica en Paint).
    if (bIsStamping && CanLocalPlayerSculpt())
    {
        const EPTStampShape Sh = EffectiveShape();
        if (bStrokeActive)
        {
            const float Dist = FVector::Dist(LastStampPos, StampPos);
            const float Step = FMath::Max(StampSize * 0.2f, 2.f); // solape entre sellos
            const int32 N = FMath::Clamp(FMath::CeilToInt(Dist / Step), 1, 32);
            for (int32 i = 1; i <= N; ++i)
            {
                const FVector P = FMath::Lerp(LastStampPos, StampPos, (float)i / N);
                Server_ApplyStamp(P, Sh, StampSize, EditMode, CurrentPaintColor, StampRotation, bStrokeIsDetail, StampScale);
            }
        }
        else
        {
            Server_ApplyStamp(StampPos, Sh, StampSize, EditMode, CurrentPaintColor, StampRotation, bStrokeIsDetail, StampScale);
            bStrokeActive = true;
            // ALT+Add: congelar el plano en el 1er sello (perpendicular a la vista) para que el resto
            // del trazo vaya a profundidad constante y no trepe hacia la cámara.
            if (bStrokeIsDetail)
            {
                FVector S, D;
                if (GetCameraRay(S, D))
                {
                    SculptPlaneOrigin  = StampPos;
                    SculptPlaneNormal  = D;      // plano perpendicular a la vista al iniciar el trazo
                    bStrokePlaneLocked = true;
                }
            }
        }
        LastStampPos = StampPos;
    }
    else
    {
        bStrokeActive      = false;
        bStrokePlaneLocked = false; // soltó el click: liberar el plano congelado
    }

    // X-ray del preview: ocultarlo mientras se AGREGA arcilla (Add + click apretado). El contorno de
    // solape molesta justo cuando estás construyendo; solo tiene sentido al posicionar (sin sellar).
    const bool bAddingClay = bIsStamping && CanLocalPlayerSculpt() && (EditMode == EPTEditMode::Add);
    SetPreviewXrayEnabled(!bAddingClay);

    // Loop 3D de la herramienta (Add/Erase/Paint) mientras se está sellando; se corta solo al soltar.
    if (SculptSounds)
        SculptSounds->SetActiveTool(EditMode, bEyesTool, bIsStamping && CanLocalPlayerSculpt(), StampPos);
}

void APTSculptPlayerController::SetPreviewXrayEnabled(bool bOn)
{
    if (bOn == bXrayOverlayOn) return; // sin cambios → no re-setear
    bXrayOverlayOn = bOn;
    UMaterialInterface* Ov = bOn ? PreviewOverlayMaterial : nullptr;
    if (PreviewMesh)       PreviewMesh->SetOverlayMaterial(Ov);
    if (PreviewStaticMesh) PreviewStaticMesh->SetOverlayMaterial(Ov);
    if (PaintRing)         PaintRing->SetOverlayMaterial(Ov);
}

// ── Lógica de cursor ─────────────────────────────────────────────────────────

bool APTSculptPlayerController::GetCameraRay(FVector& Start, FVector& Dir) const
{
    ACharacter* MyChar = Cast<ACharacter>(GetPawn());
    if (UCameraComponent* Cam = MyChar ? MyChar->FindComponentByClass<UCameraComponent>() : nullptr)
    {
        Start = Cam->GetComponentLocation();
        Dir   = Cam->GetForwardVector();
        return true;
    }
    int32 W, H;
    GetViewportSize(W, H);
    return DeprojectScreenPositionToWorld(W * 0.5f, H * 0.5f, Start, Dir);
}

bool APTSculptPlayerController::EyedropColorUnderCursor(FLinearColor& OutColor) const
{
    if (!Volume) return false;

    // Rayo por el CURSOR (mientras el picker está abierto el cursor es libre; no vale el centro).
    FVector Start, Dir;
    float MX = 0.f, MY = 0.f;
    if (!(const_cast<APTSculptPlayerController*>(this)->GetMousePosition(MX, MY)
          && const_cast<APTSculptPlayerController*>(this)->DeprojectScreenPositionToWorld(MX, MY, Start, Dir)))
    {
        if (!GetCameraRay(Start, Dir)) return false;
    }

    // Raymarch a la superficie de la arcilla (mismo enfoque que GetStampPoint) y leer el color del atlas.
    static constexpr float StepSize = 8.f;
    static constexpr int32 MaxSteps = 700;
    float prevD = Volume->SampleWorldDensity(Start);
    for (int32 i = 1; i <= MaxSteps; ++i)
    {
        const FVector P = Start + Dir * (StepSize * i);
        const float   d = Volume->SampleWorldDensity(P);
        if (prevD <= 0.f && d > 0.f)
        {
            FVector lo = P - Dir * StepSize, hi = P;
            for (int32 j = 0; j < 6; ++j)
            {
                const FVector mid = (lo + hi) * 0.5f;
                (Volume->SampleWorldDensity(mid) > 0.f ? hi : lo) = mid;
            }
            const FVector Surf = (lo + hi) * 0.5f;
            const float CV = FMath::Max(4.f, Volume->VoxelSize);

            // 1) PAINT (atlas): la textura es sRGB → el material la DECODIFICA al renderizar, así que
            //    lo que se ve = FromSRGBColor(bytes). La pintura es una cáscara fina, así que probamos
            //    varios offsets a lo largo del rayo (afuera/adentro) para no perder el trazo.
            bool bPainted = false; FLinearColor RawPaint;
            for (float Off : { 0.f, -CV, CV, -2.f * CV, 2.f * CV, -3.f * CV, 3.f * CV })
            {
                RawPaint = Volume->SampleWorldPaintColor(Surf + Dir * Off, bPainted);
                if (bPainted) break;
            }
            if (bPainted) { OutColor = FLinearColor::FromSRGBColor(RawPaint.ToFColor(false)); OutColor.A = 1.f; return true; }

            // 2) Sin Paint → COLOR BASE del campo (Add+color). La arcilla lo renderiza con los bytes
            //    reinterpretados como lineales (SIN decodificar) → devolvemos el color CRUDO tal cual
            //    (decodificarlo lo oscurecería). Se muestrea un poco ADENTRO para caer en arcilla sólida.
            FLinearColor Base = Volume->SampleWorldColor(Surf + Dir * CV);
            Base.A = 1.f; OutColor = Base;
            return true;
        }
        prevD = d;
    }
    return false;
}

FVector APTSculptPlayerController::GetStampPoint(FVector& OutNormal) const
{
    FVector Start, Dir;
    if (!GetCameraRay(Start, Dir))
    {
        OutNormal = FVector::UpVector;
        return Start + Dir * AirDepth;
    }

    // ── Modo eje: trazo recto sobre el plano CONGELADO (fijado al activar Z/X) ───
    // Solo con la herramienta de esculpir (Add) y fuera de la tool de ojos. El rayo del cursor
    // se interseca con el plano fijo → trazos rectos; soltar/re-presionar mantiene el mismo plano.
    if (bAxisLock && EditMode == EPTEditMode::Add && !bEyesTool && !bSurfaceSnap)
    {
        const float denom = FVector::DotProduct(Dir, AxisPlaneN);
        FVector Pf = AxisOrigin;
        if (FMath::Abs(denom) > 1e-4f)
        {
            const float t = FVector::DotProduct(AxisOrigin - Start, AxisPlaneN) / denom;
            if (t > 0.f) Pf = Start + Dir * t;
        }
        OutNormal = AxisPlaneN;
        // Clamp desde el PIVOT (centro) del sello: inset 0 → el centro llega justo a la cara del área
        // (mitad de la esfera adentro, mitad afuera).
        return Volume ? Volume->ClampInsideCanvas(Pf, 0.f) : Pf;
    }

    // ── Paint, Smooth y OJOS: pegar el cursor a la superficie (raymarch) para trabajar
    // preciso sobre la malla donde apuntás. ────────────────────────────────────
    // ALT+Add DURANTE un trazo: NO re-raymarchear (la arcilla nueva quedaría más cerca y el trazo
    // treparía hacia la cámara). Se dibuja sobre el plano congelado en el 1er sello (perpendicular a
    // la vista) → trazos laterales/verticales a profundidad constante.
    if (EditMode == EPTEditMode::Add && bStrokeIsDetail && bStrokeActive && bStrokePlaneLocked)
    {
        const float denom = FVector::DotProduct(Dir, SculptPlaneNormal);
        FVector Pf = SculptPlaneOrigin;
        if (FMath::Abs(denom) > 1e-4f)
        {
            const float t = FVector::DotProduct(SculptPlaneOrigin - Start, SculptPlaneNormal) / denom;
            if (t > 0.f) Pf = Start + Dir * t;
        }
        OutNormal = -Dir;
        return Volume ? Volume->ClampInsideCanvas(Pf, 0.f) : Pf;
    }

    // Paint/Smooth/Ojos SIEMPRE se pegan a la superficie; Add lo hace solo con ALT (bSurfaceSnap):
    // así podés apoyar el sello sobre la arcilla ya dibujada y detallar de cerca en vez de agregar
    // a distancia fija del brazo.
    if (((EditMode == EPTEditMode::Paint || EditMode == EPTEditMode::Smooth || bEyesTool)
         || (EditMode == EPTEditMode::Add && bSurfaceSnap)) && Volume)
    {
        static constexpr float StepSize = 8.f;  // ~1 voxel: preciso
        static constexpr int32 MaxSteps = 700;
        float prevD = Volume->SampleWorldDensity(Start);
        for (int32 i = 1; i <= MaxSteps; ++i)
        {
            const FVector P = Start + Dir * (StepSize * i);
            const float   d = Volume->SampleWorldDensity(P);
            if (prevD <= 0.f && d > 0.f)
            {
                FVector lo = P - Dir * StepSize, hi = P;
                for (int32 j = 0; j < 5; ++j)
                {
                    const FVector mid = (lo + hi) * 0.5f;
                    (Volume->SampleWorldDensity(mid) > 0.f ? hi : lo) = mid;
                }
                const FVector Surf = (lo + hi) * 0.5f;
                const float E = Volume->VoxelSize * 0.5f;
                FVector N(
                    Volume->SampleWorldDensity(Surf + FVector(E,0,0)) - Volume->SampleWorldDensity(Surf - FVector(E,0,0)),
                    Volume->SampleWorldDensity(Surf + FVector(0,E,0)) - Volume->SampleWorldDensity(Surf - FVector(0,E,0)),
                    Volume->SampleWorldDensity(Surf + FVector(0,0,E)) - Volume->SampleWorldDensity(Surf - FVector(0,0,E)));
                N = (-N).GetSafeNormal();
                OutNormal = N.IsNearlyZero() ? -Dir : N;
                return Surf;
            }
            prevD = d;
        }
        // Sin superficie a la vista → brazo extendido (no pinta nada igual).
    }

    // Cursor tipo SculptrVR: siempre a distancia fija (brazo extendido).
    OutNormal = -Dir;
    const FVector Air = Start + Dir * AirDepth;
    // Clamp desde el PIVOT (centro) del sello: inset 0 → el centro se frena en la cara interior del
    // área, con la mitad de la esfera adentro y la otra mitad afuera (podés dibujar sobre las paredes).
    return Volume ? Volume->ClampInsideCanvas(Air, 0.f) : Air;
}

// Umbral (en UU) para fijar el eje del trazo: ~medio tamaño de brocha.
float APTSculptPlayerController::VoxelHint() const
{
    return StampSize * 0.15f;
}

// ── Preview mesh ─────────────────────────────────────────────────────────────

void APTSculptPlayerController::RebuildPreviewMesh()
{
    if (!PreviewMesh || !Volume) return;

    TArray<FVector> Verts, Normals;
    TArray<int32>   Tris;
    APTSculptVolume::BuildStampPreview(EffectiveShape(), StampSize, Volume->VoxelSize, Verts, Tris, Normals, StampScale);

    // Add/Paint: teñir la preview con el color del picker (por vertex color).
    // Otras tools: blanco (el material del tool decide su propio look).
    TArray<FColor> Colors;
    const bool bTint = (EditMode == EPTEditMode::Add || EditMode == EPTEditMode::Paint);
    const FColor VC = bTint ? CurrentPaintColor.ToFColor(true) : FColor::White;
    Colors.Init(VC, Verts.Num());
    PreviewMesh->CreateMeshSection(0, Verts, Tris, Normals, {}, Colors, {}, false);

    ApplyPreviewMaterial();
}

void APTSculptPlayerController::ApplyPreviewMaterial()
{
    UMaterialInterface* Mat = PreviewMeshMaterial;
    switch (EditMode)
    {
    case EPTEditMode::Add:    if (PreviewMatAdd)    Mat = PreviewMatAdd;    break;
    case EPTEditMode::Erase:  if (PreviewMatErase)  Mat = PreviewMatErase;  break;
    case EPTEditMode::Smooth: if (PreviewMatSmooth) Mat = PreviewMatSmooth; break;
    case EPTEditMode::Paint:  if (PreviewMatPaint)  Mat = PreviewMatPaint;  break;
    }
    if (!Mat) return;

    // En Add/Paint teñir con el color del picker vía Material Instance Dinámico
    // (parámetro "Color"). Funciona tanto en el mesh procedural como en uno custom.
    const bool bTint = (EditMode == EPTEditMode::Add || EditMode == EPTEditMode::Paint);

    PreviewMID = nullptr;
    PreviewStaticMID = nullptr;
    auto ApplyTo = [&](UMeshComponent* Comp) -> UMaterialInstanceDynamic*
    {
        if (!Comp) return nullptr;
        if (bTint)
        {
            UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0, Mat);
            if (MID)
            {
                MID->SetVectorParameterValue(TEXT("Color"), CurrentPaintColor);
                // El preview NO debe tener el glow por-tiempo de la arcilla nueva (no tiene el dato
                // de "tiempo agregado" por vóxel). GlowEnable=0 lo apaga; en el material del volumen
                // real queda en su default 1. (El brillo AL estampar lo maneja aparte el param "Glow".)
                MID->SetScalarParameterValue(TEXT("GlowEnable"), 0.f);
            }
            return MID;
        }
        Comp->SetMaterial(0, Mat);
        return nullptr;
    };
    PreviewMID       = ApplyTo(PreviewMesh);
    PreviewStaticMID = ApplyTo(PreviewStaticMesh);
}

void APTSculptPlayerController::UpdatePreviewBrightness(float Dt)
{
    if (!PreviewMID && !PreviewStaticMID) return;

    // Objetivo: 1 mientras mantenés el sello en Agregar, 0 si no. Sube rápido al apretar y se
    // DESVANECE suave al soltar (mismo tiempo que el brillo de la arcilla nueva), en vez de cortar
    // seco. Así el preview "acompaña" el brillo del trazo.
    const bool  bBright = bIsStamping && EditMode == EPTEditMode::Add && !bEyesTool;
    const float Target  = bBright ? 1.f : 0.f;
    const float FadeSec = Volume ? Volume->NewClayGlowSeconds : 1.2f;
    const float UpSpeed = 14.f;                         // casi instantáneo al empezar a esculpir
    const float DnSpeed = 1.f / FMath::Max(FadeSec, 0.05f);
    PreviewGlowAmt = FMath::FInterpTo(PreviewGlowAmt, Target, Dt, (Target > PreviewGlowAmt) ? UpSpeed : DnSpeed);

    // El color base NO cambia (el preview mantiene su color real); el brillo lo maneja el
    // parámetro "Glow" (0..1). Con Glow=0 el material del preview NO emite → se ve como el material
    // original; con Glow=1 emite el glow. El material del preview Add debe multiplicar su Emissive
    // por "Glow" (así queda apagado cuando no esculpís).
    if (PreviewMID)
    {
        PreviewMID->SetVectorParameterValue(TEXT("Color"), CurrentPaintColor);
        PreviewMID->SetScalarParameterValue(TEXT("Glow"), PreviewGlowAmt);
    }
    if (PreviewStaticMID)
    {
        PreviewStaticMID->SetVectorParameterValue(TEXT("Color"), CurrentPaintColor);
        PreviewStaticMID->SetScalarParameterValue(TEXT("Glow"), PreviewGlowAmt);
    }
}

// Elige el mesh de preview: override por tool > override por stamp > procedural.
void APTSculptPlayerController::UpdatePreviewVisual()
{
    if (!PreviewMesh) return;

    // Smooth (decal), Paint (cursor 2D) y OJOS (preview sobre la malla vía PaintRing): sin malla 3D acá.
    if (EditMode == EPTEditMode::Smooth || EditMode == EPTEditMode::Paint || bEyesTool)
    {
        PreviewMesh->SetVisibility(false);
        if (PreviewStaticMesh) PreviewStaticMesh->SetVisibility(false);
        return;
    }

    // Mesh estático DEDICADO a la forma (opcional). Solo las 4 clásicas pueden tener uno asignado.
    UStaticMesh* ShapeMesh = nullptr;
    switch (EffectiveShape())
    {
    case EPTStampShape::Sphere:     ShapeMesh = PreviewMeshSphere;     break;
    case EPTStampShape::Cube:       ShapeMesh = PreviewMeshCube;       break;
    case EPTStampShape::Cylinder:   ShapeMesh = PreviewMeshCylinder;   break;
    case EPTStampShape::TriPrism:   ShapeMesh = PreviewMeshTriPrism;   break;
    case EPTStampShape::Pyramid:    ShapeMesh = PreviewMeshPyramid;    break;
    case EPTStampShape::Torus:      ShapeMesh = PreviewMeshTorus;      break;
    case EPTStampShape::Capsule:    ShapeMesh = PreviewMeshCapsule;    break;
    case EPTStampShape::HexPrism:   ShapeMesh = PreviewMeshHexPrism;   break;
    case EPTStampShape::Octahedron: ShapeMesh = PreviewMeshOctahedron; break;
    default: break;
    }
    // El preview debe MATCHEAR la forma. Si hay mesh dedicado a esa forma se usa; si no (formas nuevas),
    // va el PROCEDURAL (generado del SDF, siempre coincide). NO se usa un mesh genérico por-tool porque
    // haría que el preview no cambie entre formas (era el bug: las nuevas mostraban el mesh del tool).
    UStaticMesh* Chosen = ShapeMesh;

    if (Chosen && PreviewStaticMesh)
    {
        // Usar el mesh estático del usuario, escalado al tamaño de brocha.
        PreviewStaticMesh->SetStaticMesh(Chosen);
        // AUTO-FIT: escalar el mesh para que su lado más ancho = StampSize (lo que ocupa el sello). Así
        // el preview coincide con lo que se esculpe SIN importar el tamaño nativo del mesh (antes dependía
        // de PreviewMeshBaseSize y no cuadraba). Se usa la escala RELATIVA (frame rotado del actor) para
        // que la escala no-uniforme siga los ejes locales del sello.
        const FVector Ext = Chosen->GetBoundingBox().GetExtent(); // half-extents
        const float MaxExt = FMath::Max3(Ext.X, Ext.Y, Ext.Z);
        const float Fit = (MaxExt > 1.f) ? (StampSize * 0.5f) / MaxExt
                                         : (StampSize / FMath::Max(PreviewMeshBaseSize, 1.f));
        PreviewStaticMesh->SetRelativeScale3D(FVector(Fit) * StampScale);
        PreviewStaticMesh->SetVisibility(true);
        PreviewMesh->SetVisibility(false);
    }
    else
    {
        // Mesh procedural (forma SDF del sello).
        if (PreviewStaticMesh) PreviewStaticMesh->SetVisibility(false);
        PreviewMesh->SetVisibility(true);
        RebuildPreviewMesh();
    }
    ApplyPreviewMaterial();

    // Re-aplicar el overlay X-ray al componente que quedó visible, según el estado actual (así las formas
    // que van por procedural también intentan mostrar el xray tras el rebuild, no solo las de mesh estático).
    UMaterialInterface* Ov = bXrayOverlayOn ? PreviewOverlayMaterial : nullptr;
    if (PreviewMesh)       PreviewMesh->SetOverlayMaterial(Ov);
    if (PreviewStaticMesh) PreviewStaticMesh->SetOverlayMaterial(Ov);
}

// ── Acciones de input ─────────────────────────────────────────────────────────

void APTSculptPlayerController::OnStampPressed()
{
    // Herramienta de ojos: un ojo por click (no esculpe ni deja bIsStamping).
    if (bEyesTool) { PlaceEyeAtCursor(); return; }

    bIsStamping = true;
    // ¿Este trazo es de DETALLE? (Add + Alt al apretar). Se latchea para todo el trazo.
    bStrokeIsDetail = (EditMode == EPTEditMode::Add && bSurfaceSnap);
    if (CanLocalPlayerSculpt())
    {
        if (bStrokeIsDetail)
            Server_BeginDetailLayer(); // nueva capa aparte (no fusiona con la base ni otras capas)
        else
            Server_BeginStroke();      // trazo normal sobre la base (undo por trazo)
    }
    // NOTA: el plano del modo eje NO se recalcula al apretar (se fijó al activar el modo con
    // Z/X). Así soltar y re-presionar el esculpido sigue en el MISMO plano.
}

void APTSculptPlayerController::OnStampReleased()
{
    // Los trazos de la BASE se cierran con EndStroke (para el undo por trazo); las capas de detalle
    // ya quedaron committeadas al crearse, así que no necesitan cierre.
    if (bIsStamping && !bStrokeIsDetail && CanLocalPlayerSculpt()) Server_EndStroke();
    bIsStamping     = false;
    bStrokeIsDetail = false;
    AxisChosen      = -1;
}

void APTSculptPlayerController::OnClearAllPressed()
{
    // Mientras escribís en el chat, BACKSPACE es para borrar texto, no la escultura.
    if (GameplayHUD && GameplayHUD->IsChatOpen()) return;
    if (!CanLocalPlayerSculpt()) return;
    bClearHeld    = true;
    ClearHoldTime = 0.f;
}

void APTSculptPlayerController::OnClearAllReleased()
{
    // Toque CORTO (soltaste antes de completar los 3s) = deshacer la última acción.
    // Mantener hasta el final = borrar todo (eso lo dispara el tick, que ya limpió bClearHeld).
    if (bClearHeld && ClearHoldTime < UndoTapMaxTime && CanLocalPlayerSculpt())
    {
        Server_Undo();
        if (SculptSounds) SculptSounds->PlayUndoSimple(Volume ? Volume->GetActorLocation()
                                                               : (GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector));
        if (GEngine) GEngine->AddOnScreenDebugMessage(987723, 1.2f, FColor(150, 220, 255),
            PTText::GetStr(TEXT("SCULPT_UNDO")));
    }
    bClearHeld    = false;
    ClearHoldTime = 0.f;
}

void APTSculptPlayerController::Server_BeginStroke_Implementation()
{
    if (!Volume)
        Volume = Cast<APTSculptVolume>(
            UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
    if (Volume) Volume->Multicast_BeginStroke();
}

void APTSculptPlayerController::Server_EndStroke_Implementation()
{
    if (Volume) Volume->Multicast_EndStroke();
}

void APTSculptPlayerController::Server_Undo_Implementation()
{
    // Mismo gating que el stamp: solo el escultor del turno deshace.
    if (const APTSculptGameState* G = GetWorld()->GetGameState<APTSculptGameState>())
    {
        if (G->TurnPhase != EPTTurnPhase::Drawing ||
            G->CurrentSculptor != GetPlayerState<APTPlayerState>())
            return;
    }
    if (!Volume)
        Volume = Cast<APTSculptVolume>(
            UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
    if (Volume) Volume->Multicast_Undo(); // todos deshacen su propio respaldo del último trazo
}

void APTSculptPlayerController::Server_ClearSculpture_Implementation()
{
    // Mismo gating que el stamp: solo el escultor del turno puede borrar todo.
    if (const APTSculptGameState* G = GetWorld()->GetGameState<APTSculptGameState>())
    {
        if (G->TurnPhase != EPTTurnPhase::Drawing ||
            G->CurrentSculptor != GetPlayerState<APTPlayerState>())
            return;
    }
    if (!Volume)
        Volume = Cast<APTSculptVolume>(
            UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
    if (Volume) Volume->Multicast_ClearAll(); // lienzo en blanco en TODOS
}

void APTSculptPlayerController::OnShapeRotatePressed()
{
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

    // Doble click de rueda → resetear rotación Y escala del sello.
    if (Now - LastWheelPressTime <= WheelDoubleClickWindow)
    {
        StampRotation = FRotator::ZeroRotator;
        StampScale    = FVector::OneVector;
        bPreviewDirty = true;
        UE_LOG(LogTemp, Log, TEXT("[Sculpt] Rotación + escala del shape: reset"));
    }
    LastWheelPressTime = Now;

    // Mientras se mantiene la rueda, el mouse rota el sello y NO mueve la cámara.
    if (!bRotatingShape)
    {
        bRotatingShape = true;
        SetIgnoreLookInput(true);
    }
}

void APTSculptPlayerController::OnShapeRotateReleased()
{
    if (!bRotatingShape) return;
    bRotatingShape = false;
    SetIgnoreLookInput(false);
}

// HOLD: mantener la tecla activa el eje; soltarla vuelve a Add. Al soltar solo apago si el eje activo
// es el de esta tecla (así si tenés Z apretada y tocás X, soltar X no rompe el vertical, y viceversa).
// Z/X/Alt son holds mutuamente excluyentes: al apretar uno se apaga el otro (la nueva tecla quema a la
// anterior). Si dejo Alt prendido y aprieto Z, se usa SOLO el eje; y viceversa. No "vuelven" solos al
// soltar el nuevo (hay que re-apretar el anterior), que es justo lo que evita los estados combinados.
void APTSculptPlayerController::OnAxisVerticalPressed()   { if (bIsStamping) return; bSurfaceSnap = false; SetAxisMode(true, /*bHorizontal=*/false); }
void APTSculptPlayerController::OnAxisVerticalReleased()  { if (bAxisLock && !bAxisHorizontal) SetAxisMode(false, false); }
void APTSculptPlayerController::OnAxisHorizontalPressed() { if (bIsStamping) return; bSurfaceSnap = false; SetAxisMode(true, /*bHorizontal=*/true); }
void APTSculptPlayerController::OnAxisHorizontalReleased(){ if (bAxisLock && bAxisHorizontal) SetAxisMode(false, true); }

void APTSculptPlayerController::OnSurfaceSnapPressed()
{
    // No cambiar de herramienta hold MID-TRAZO (con el click apretado): apagar el eje a mitad del trazo
    // hacía que el sello volviera al "brazo extendido" y la arcilla escalara hacia la cámara. Solo se
    // puede cambiar con el click suelto.
    if (bIsStamping) return;
    // Alt quema el modo eje: si había un eje activo, se apaga antes de prender el snap.
    if (bAxisLock) SetAxisMode(false, bAxisHorizontal);
    bSurfaceSnap = true;
}

void APTSculptPlayerController::SetAxisMode(bool bEnable, bool bHorizontal)
{
    bAxisLock = bEnable;

    if (bEnable)
    {
        // El modo eje solo tiene sentido esculpiendo: auto-equipar Agregar (sin resetear el eje,
        // que es justo lo que estamos por prender). Antes podías activarlo en Erase/Paint y quedaba
        // tosco: el plano estaba pero no hacía nada.
        SetModeInternal(EPTEditMode::Add, /*bResetAxis=*/false);

        bAxisHorizontal = bHorizontal;
        // Fijar el plano AHORA (al ángulo/posición actual de cámara) y no recalcularlo más.
        FVector Start, Dir;
        if (GetCameraRay(Start, Dir))
        {
            AxisOrigin = Start + Dir * AirDepth;
            if (bHorizontal)
            {
                AxisPlaneN = FVector(0.f, 0.f, 1.f); // plano horizontal a la altura de AxisOrigin
            }
            else
            {
                FVector N(Dir.X, Dir.Y, 0.f);        // normal horizontal → plano vertical
                AxisPlaneN = N.GetSafeNormal();
                if (AxisPlaneN.IsNearlyZero()) AxisPlaneN = FVector(1.f, 0.f, 0.f);
            }
        }
    }

    // Movimiento y cámara quedan LIBRES: lo que te impide cruzar el plano es su colisión física
    // (APTSculptPlane), que el servidor spawnea y que solo bloquea al escultor.
    Server_SetSculptPlane(bAxisLock, AxisOrigin, AxisPlaneN);

    bPreviewDirty = true;
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Modo eje: %s (%s)"),
           bAxisLock ? TEXT("ON") : TEXT("OFF"), bHorizontal ? TEXT("horizontal") : TEXT("vertical"));
}

void APTSculptPlayerController::Server_SetSculptPlane_Implementation(bool bEnable, FVector Origin, FVector Normal)
{
    // Solo el escultor del turno puede poner su plano (mismo gating que el stamp).
    if (bEnable)
    {
        if (const APTSculptGameState* G = GetWorld()->GetGameState<APTSculptGameState>())
        {
            if (G->TurnPhase != EPTTurnPhase::Drawing ||
                G->CurrentSculptor != GetPlayerState<APTPlayerState>())
                bEnable = false;
        }
    }

    // Siempre limpiar el plano anterior (toggle off, o cambio de plano vertical↔horizontal).
    if (ActivePlane) { ActivePlane->Destroy(); ActivePlane = nullptr; }
    if (!bEnable || !SculptPlaneClass) return;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SP.Owner = this;
    ActivePlane = GetWorld()->SpawnActor<APTSculptPlane>(
        SculptPlaneClass, Origin, Normal.Rotation(), SP);
    // Solo bloquea a MI pawn (el escultor); los demás lo ignoran al moverse.
    if (ActivePlane) ActivePlane->SetTargetPawn(GetPawn());
}

void APTSculptPlayerController::OnScrollUp()
{
    // Con la rueda de color abierta (RMB) la rueda del mouse sube el brillo del color.
    if (bQuickColorActive)
    {
        if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(ColorPicker))
            CP->QuickAdjustValue(+0.05f);
        return;
    }
    // Modo eje activo → la rueda ESCALA (no uniforme): eje Z (modo vertical) o X+Y (modo horizontal).
    if (bAxisLock)
    {
        if (bAxisHorizontal) { StampScale.X = FMath::Clamp(StampScale.X + ScaleStep, MinScale, MaxScale); StampScale.Y = StampScale.X; }
        else                 { StampScale.Z = FMath::Clamp(StampScale.Z + ScaleStep, MinScale, MaxScale); }
        bPreviewDirty = true;
        UE_LOG(LogTemp, Log, TEXT("[Sculpt] Scale: %s"), *StampScale.ToString());
        return;
    }
    StampSize += SizeStep;
    ClampStampSize();
    bPreviewDirty = true;
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Size: %.0f"), StampSize);
}

void APTSculptPlayerController::OnScrollDown()
{
    // Con la rueda de color abierta (RMB) la rueda del mouse baja el brillo (hacia negro).
    if (bQuickColorActive)
    {
        if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(ColorPicker))
            CP->QuickAdjustValue(-0.05f);
        return;
    }
    if (bAxisLock)
    {
        if (bAxisHorizontal) { StampScale.X = FMath::Clamp(StampScale.X - ScaleStep, MinScale, MaxScale); StampScale.Y = StampScale.X; }
        else                 { StampScale.Z = FMath::Clamp(StampScale.Z - ScaleStep, MinScale, MaxScale); }
        bPreviewDirty = true;
        UE_LOG(LogTemp, Log, TEXT("[Sculpt] Scale: %s"), *StampScale.ToString());
        return;
    }
    StampSize -= SizeStep;
    ClampStampSize();
    bPreviewDirty = true;
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Size: %.0f"), StampSize);
}

void APTSculptPlayerController::SetShape(EPTStampShape S)
{
    StampShape    = S;
    bPreviewDirty = true;
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Shape: %d"), (int32)S);
}

void APTSculptPlayerController::CycleShapes()
{
    // Cicla las 4 formas (Smooth ya no existe como modo; las formas son independientes).
    switch (StampShape)
    {
    case EPTStampShape::Sphere:   StampShape = EPTStampShape::Cube;     break;
    case EPTStampShape::Cube:     StampShape = EPTStampShape::Cylinder; break;
    case EPTStampShape::Cylinder: StampShape = EPTStampShape::TriPrism; break;
    case EPTStampShape::TriPrism: StampShape = EPTStampShape::Sphere;   break;
    }
    bPreviewDirty = true;
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Shape: %d"), (int32)StampShape);
}

void APTSculptPlayerController::OnShapeRadialPressed()
{
    if (bShapeRadialActive) return;

    // El radial de formas SOLO tiene sentido con herramientas que usan formas (Agregar/Borrar/Pintar).
    // Con Ojos (u otra tool sin formas) la tecla no hace nada.
    if (!ToolUsesShapes()) return;

    // Sin WBP radial asignado: mantener el comportamiento viejo (un toque cicla la forma).
    if (!ShapeRadialClass)
    {
        CycleShapes();
        return;
    }

    ShapeRadial = CreateWidget<UPTShapeRadialWidget>(this, ShapeRadialClass);
    if (!ShapeRadial) { CycleShapes(); return; } // si falla, no dejar al jugador sin poder cambiar

    // Recordar la forma actual: el hover la cambia en vivo y si soltás en el centro se restaura.
    ShapeBeforeRadial = StampShape;

    ShapeRadial->AddToViewport(20);
    ShapeRadial->BeginRadial(LastShapePage); // reabre en la ÚLTIMA página que dejaste, no siempre la 1
    SetInputMode(FInputModeGameAndUI());
    bShowMouseCursor = true;

    // Centrar el cursor: el radial se dibuja centrado y el arrastre define la dirección de elección.
    int32 VX = 0, VY = 0;
    GetViewportSize(VX, VY);
    if (VX > 0 && VY > 0) SetMouseLocation(VX / 2, VY / 2);

    bShapeRadialActive = true;
}

void APTSculptPlayerController::OnShapeRadialReleased()
{
    if (!bShapeRadialActive) return;
    bShapeRadialActive = false;

    if (ShapeRadial)
    {
        // Recordar la página en la que quedó, para reabrir ahí la próxima vez.
        LastShapePage = ShapeRadial->GetCurrentPage();

        // Soltar sobre un slot → esa forma; soltar en el centro (zona muerta) → volver a la de antes
        // (el hover la había cambiado en vivo).
        EPTStampShape Selected;
        if (ShapeRadial->GetSelectedShape(Selected)) SetShape(Selected);
        else                                         SetShape(ShapeBeforeRadial);

        ShapeRadial->RemoveFromParent(); // NativeDestruct restaura el cursor
        ShapeRadial = nullptr;
    }

    // Volver al modo de juego normal del esculpido (sin cursor), igual que la rueda de color.
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;
}

void APTSculptPlayerController::SetMode(EPTEditMode M)
{
    // No cambiar de herramienta MID-TRAZO (con el click apretado): hacerlo a mitad de un trazo bugea
    // (el sello sigue del modo viejo, cambia profundidad, etc.). Solo se cambia con el click suelto.
    if (bIsStamping) return;
    SetModeInternal(M, /*bResetAxis=*/true);
}

void APTSculptPlayerController::SetModeInternal(EPTEditMode M, bool bResetAxis)
{
    // Cambiar de herramienta SIEMPRE resetea el modo eje al default (apagado) y destruye su plano
    // con colisión: si no, quedaba activo estorbando al pintar/borrar.
    if (bResetAxis && bAxisLock) SetAxisMode(false, bAxisHorizontal);

    bEyesTool = false; // 1/2/3 salen de la herramienta de ojos

    // Entrar en Borrar arranca SIEMPRE en Esfera: es la forma que uno espera para corregir, y si
    // quedara la última usada en Agregar (un cono, por ejemplo) el primer borrado sale raro.
    if (M == EPTEditMode::Erase && EditMode != EPTEditMode::Erase)
    {
        StampShape     = EPTStampShape::Sphere;
        StampRotation  = FRotator::ZeroRotator; // y sin la rotación que traía el shape anterior
    }

    EditMode = M;
    ClampStampSize(); // respetar el mínimo del nuevo modo (Paint permite más chico)
    bPreviewDirty = true;
    ApplyPreviewMaterial();
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Mode: %d"), (int32)EditMode);
}

void APTSculptPlayerController::SetModeEyes()
{
    if (bIsStamping) return; // no cambiar a Ojos mid-trazo (ver SetMode)
    if (bAxisLock) SetAxisMode(false, bAxisHorizontal); // idem: la tool de ojos no usa el plano

    bEyesTool = true;
    bPreviewDirty = true;
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Mode: OJOS"));
}

void APTSculptPlayerController::PlaceEyeAtCursor()
{
    if (!Volume || !CanLocalPlayerSculpt()) return;
    FVector Normal;
    const FVector Pt = GetStampPoint(Normal);   // superficie (raymarch)
    if (!Volume->IsInsideCanvas(Pt)) return;    // fuera de la zona de modelado: no colocar

    // Cada ojo es una acción deshacible por sí sola: se envuelve en su propio trazo.
    Server_BeginStroke();
    Server_AddEye(Pt, StampSize * 0.5f);        // vía el controller (tiene owner) → server → volumen
    Server_EndStroke();
    if (SculptSounds) SculptSounds->PlayEyes(Pt); // sonido de colocar ojo (3D en el punto)
}

void APTSculptPlayerController::Server_AddEye_Implementation(FVector WorldPos, float Radius)
{
    // Mismo gating que el stamp: solo el escultor del turno modifica la escultura.
    if (const APTSculptGameState* G = GetWorld()->GetGameState<APTSculptGameState>())
    {
        if (G->TurnPhase != EPTTurnPhase::Drawing ||
            G->CurrentSculptor != GetPlayerState<APTPlayerState>())
            return;
    }
    if (!Volume)
        Volume = Cast<APTSculptVolume>(
            UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
    if (Volume) Volume->AddEye(WorldPos, Radius);
}

void APTSculptPlayerController::OnColorPickPressed()
{
    if (!ColorPickerClass || ColorPicker) return;
    ColorPicker = CreateWidget<UUserWidget>(this, ColorPickerClass);
    if (!ColorPicker) return;
    ColorPicker->AddToViewport(10);
    SetInputMode(FInputModeGameAndUI());
    bShowMouseCursor  = true;
    bQuickColorActive = true;
}

void APTSculptPlayerController::OnColorSavePressed()
{
    if (!bQuickColorActive) return;
    if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(ColorPicker))
        CP->SaveCurrentColor();
}

void APTSculptPlayerController::OnColorPickReleased()
{
    if (!bQuickColorActive) return;
    bQuickColorActive = false;

    // Confirmar según dónde soltó: swatch guardado, "+" (guardar) o rueda. ConfirmQuickPick
    // termina llamando a OnColorConfirmed, que aplica el color, restaura input y cierra.
    if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(ColorPicker))
    {
        CP->ConfirmQuickPick();
    }
    else if (ColorPicker)
    {
        ColorPicker->RemoveFromParent();
        ColorPicker = nullptr;
        SetInputMode(FInputModeGameOnly());
        bShowMouseCursor = false;
    }
}

void APTSculptPlayerController::OnColorConfirmed(FLinearColor NewColor)
{
    CurrentPaintColor = NewColor;

    // Al elegir color NO se fuerza Paint: si estabas en Erase pasás a Paint; si estabas
    // en Add te quedás en Add (esculpís directamente con el color); en Paint queda igual.
    if (EditMode == EPTEditMode::Erase || EditMode == EPTEditMode::Smooth)
        EditMode = EPTEditMode::Paint;
    ApplyPreviewMaterial();

    if (ColorPicker)
    {
        ColorPicker->RemoveFromParent();
        ColorPicker = nullptr;
    }

    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;

    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Paint color set, mode=Paint"));
}

// ── Partida (Sculpturillo) ───────────────────────────────────────────────────

void APTSculptPlayerController::Client_ReceiveWordChoices_Implementation(const TArray<FString>& Choices)
{
    CurrentWordChoices = Choices;
    UE_LOG(LogTemp, Log, TEXT("[SculptPC] Palabras para elegir: %s"), *FString::Join(Choices, TEXT(", ")));
    OnWordChoicesReceived.Broadcast(Choices);
}

void APTSculptPlayerController::Client_ReceiveSecretWord_Implementation(const FString& Word)
{
    CurrentSecretWord = Word;
    UE_LOG(LogTemp, Log, TEXT("[SculptPC] Tu palabra a esculpir: %s"), *Word);
    OnSecretWordReceived.Broadcast(Word);
}

void APTSculptPlayerController::Client_SystemLine_Implementation(FName Key, const FString& Arg0, int32 Arg1)
{
    if (APTSculptGameState* GS = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr)
        GS->EmitLocalSystemLine(Key, Arg0, Arg1);
}

void APTSculptPlayerController::Client_YouGuessed_Implementation(const FString& Word, int32 Points)
{
    if (APTSculptGameState* GS = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr)
        GS->OnYouGuessed.Broadcast(Word, Points);
}

void APTSculptPlayerController::Client_AllGuessed_Implementation()
{
    if (APTSculptGameState* GS = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr)
        GS->OnAllGuessed.Broadcast();
}

void APTSculptPlayerController::Server_ChooseWord_Implementation(int32 Index)
{
    if (APTSculptGameMode* GM = GetWorld()->GetAuthGameMode<APTSculptGameMode>())
        GM->HandleWordChosen(GetPlayerState<APTPlayerState>(), Index);
}

void APTSculptPlayerController::Server_SendChat_Implementation(const FString& Message)
{
    if (APTSculptGameMode* GM = GetWorld()->GetAuthGameMode<APTSculptGameMode>())
        GM->HandleChat(GetPlayerState<APTPlayerState>(), Message);
}

void APTSculptPlayerController::Server_RequestPlayAgain_Implementation()
{
    if (APTSculptGameMode* GM = GetWorld()->GetAuthGameMode<APTSculptGameMode>())
        GM->RequestPlayAgain(GetPlayerState<APTPlayerState>());
}

void APTSculptPlayerController::Server_RequestReturnToLobby_Implementation()
{
    if (APTSculptGameMode* GM = GetWorld()->GetAuthGameMode<APTSculptGameMode>())
        GM->RequestReturnToLobby(GetPlayerState<APTPlayerState>());
}

void APTSculptPlayerController::UpdateSculptorHighlights()
{
    APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
    if (!G) return;
    for (APlayerState* PS : G->PlayerArray)
    {
        ACharacter* Char = PS ? Cast<ACharacter>(PS->GetPawn()) : nullptr;
        if (!Char || !Char->GetMesh()) continue;
        const bool bIsSculptor = (PS == G->CurrentSculptor);
        Char->GetMesh()->SetOverlayMaterial(bIsSculptor ? SculptorOverlayMaterial : nullptr);
    }
}

bool APTSculptPlayerController::CanLocalPlayerSculpt() const
{
    const APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
    if (!G) return true; // sin partida (testeo del mapa solo) → esculpir libre
    return G->TurnPhase == EPTTurnPhase::Drawing && G->IsLocalPlayerSculptor();
}

void APTSculptPlayerController::Server_ApplyStamp_Implementation(FVector WorldPos, EPTStampShape Shape,
    float Size, EPTEditMode Mode, FLinearColor PaintColor, FRotator StampRot, bool bDetail, FVector InStampScale)
{
    // Gating de autoridad: solo el escultor del turno en curso modifica la escultura.
    if (const APTSculptGameState* G = GetWorld()->GetGameState<APTSculptGameState>())
    {
        if (G->TurnPhase != EPTTurnPhase::Drawing ||
            G->CurrentSculptor != GetPlayerState<APTPlayerState>())
            return;
    }
    if (!Volume)
        Volume = Cast<APTSculptVolume>(
            UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
    if (Volume)
        Volume->Multicast_ApplyStamp(WorldPos, Shape, Size, Mode, PaintColor, StampRot, bDetail, InStampScale);
}

void APTSculptPlayerController::Server_BeginDetailLayer_Implementation()
{
    // Mismo gating que el stamp: solo el escultor del turno arranca una capa de detalle.
    if (const APTSculptGameState* G = GetWorld()->GetGameState<APTSculptGameState>())
    {
        if (G->TurnPhase != EPTTurnPhase::Drawing ||
            G->CurrentSculptor != GetPlayerState<APTPlayerState>())
            return;
    }
    if (!Volume)
        Volume = Cast<APTSculptVolume>(
            UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
    if (Volume) Volume->Multicast_BeginDetailLayer();
}

// ── Snapshot de la escultura para (re)conexiones tardías ─────────────────────
// El campo SDF es grande; se COMPRIME (zlib) y se manda por ACK (un chunk confiable a la vez), así
// nunca se desborda el buffer confiable — que era lo que saturaba y cortaba la conexión al reconectar.
void APTSculptPlayerController::SendSculptSnapshot(const TArray<uint8>& Blob)
{
    if (Blob.Num() == 0) return;

    // Comprimir: SnapOut = [int32 tamañoCrudo][bytes comprimidos].
    const int32 RawSize = Blob.Num();
    int32 CompBound = FCompression::CompressMemoryBound(NAME_Zlib, RawSize);
    TArray<uint8> Comp; Comp.SetNumUninitialized(CompBound);
    int32 CompSize = CompBound;
    if (!FCompression::CompressMemory(NAME_Zlib, Comp.GetData(), CompSize, Blob.GetData(), RawSize))
        return;
    Comp.SetNum(CompSize);

    SnapOut.Reset();
    SnapOut.Append((const uint8*)&RawSize, sizeof(int32));
    SnapOut.Append(Comp);
    SnapSent = 0;
    SendNextSnapChunk();
}

void APTSculptPlayerController::SendNextSnapChunk()
{
    if (SnapSent >= SnapOut.Num()) { SnapOut.Reset(); return; }
    const int32 ChunkBytes = 16 * 1024;
    const int32 Take  = FMath::Min(ChunkBytes, SnapOut.Num() - SnapSent);
    const bool  bFirst = (SnapSent == 0);
    TArray<uint8> Chunk;
    Chunk.Append(SnapOut.GetData() + SnapSent, Take);
    SnapSent += Take;
    const bool bLast = (SnapSent >= SnapOut.Num());
    Client_SculptSnapshotChunk(Chunk, bFirst, bLast); // esperamos el ACK del cliente para el próximo
}

void APTSculptPlayerController::Server_AckSnapChunk_Implementation()
{
    SendNextSnapChunk(); // el cliente confirmó → mandar el siguiente
}

void APTSculptPlayerController::Client_SculptSnapshotChunk_Implementation(const TArray<uint8>& Data, bool bFirst, bool bLast)
{
    if (bFirst) SnapIn.Reset();
    SnapIn.Append(Data);
    if (!bLast) { Server_AckSnapChunk(); return; } // pedir el próximo chunk

    // Último chunk: descomprimir y aplicar. SnapIn = [int32 tamañoCrudo][comprimido].
    if (SnapIn.Num() > (int32)sizeof(int32))
    {
        int32 RawSize = 0;
        FMemory::Memcpy(&RawSize, SnapIn.GetData(), sizeof(int32));
        if (RawSize > 0)
        {
            TArray<uint8> Raw; Raw.SetNumUninitialized(RawSize);
            if (FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), RawSize,
                    SnapIn.GetData() + sizeof(int32), SnapIn.Num() - sizeof(int32)))
            {
                if (!Volume)
                    Volume = Cast<APTSculptVolume>(
                        UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
                if (Volume) Volume->LoadSnapshot(Raw); // geometría base + capas + pintura
            }
        }
    }
    SnapIn.Reset();
}

