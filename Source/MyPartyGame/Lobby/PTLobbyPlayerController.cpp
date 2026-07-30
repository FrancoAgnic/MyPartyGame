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
    InputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &APTLobbyPlayerController::OnHeadModeEyes);
    // TAB = ciclar la forma del sello (esfera/cubo/cilindro/cono), igual que el gameplay.
    InputComponent->BindKey(EKeys::Tab,   IE_Pressed, this, &APTLobbyPlayerController::OnHeadCycleShape);
    // RMB mantenido = color picker (igual que el gameplay).
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed,  this, &APTLobbyPlayerController::OnHeadColorPickPressed);
    InputComponent->BindKey(EKeys::RightMouseButton, IE_Released, this, &APTLobbyPlayerController::OnHeadColorPickReleased);
}

void APTLobbyPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!bHeadSculptMode) return;

    // Mantener el resaltado de la hotbar al día con la herramienta equipada (1/2/3/4).
    if (HeadHUD) HeadHUD->Refresh(this);

    // Órbita de cámara con WASD (A/D = yaw, W/S = pitch).
    const float dYaw   = (IsInputKeyDown(EKeys::D) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::A) ? 1.f : 0.f);
    const float dPitch = (IsInputKeyDown(EKeys::W) ? 1.f : 0.f) - (IsInputKeyDown(EKeys::S) ? 1.f : 0.f);
    if (dYaw != 0.f || dPitch != 0.f)
    {
        HeadOrbitYaw   += dYaw   * HeadOrbitSpeed * DeltaTime;
        HeadOrbitPitch  = FMath::Clamp(HeadOrbitPitch + dPitch * HeadOrbitSpeed * DeltaTime, -85.f, 85.f);
        UpdateHeadCam();
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

    // Punto de la brocha (cursor) — sirve para el preview y para esculpir. Normal para el preview.
    FVector Pt, Nrm = FVector::UpVector;
    const bool bHavePt = GetHeadStampPoint(Pt, Nrm);

    // ¿La brocha quedó fuera del área de esculpido? → icono 🚫 y no se sella ahí.
    bHeadStampOutside = bHavePt && HeadVolume && !HeadVolume->IsInsideCanvas(Pt);

    // Preview de la herramienta siguiendo el cursor.
    UpdateHeadPreview(bHavePt ? &Pt : nullptr, Nrm);

    // Stamp continuo mientras se mantiene el LMB (con la forma elegida; nunca fuera del área).
    if (HeadVolume && bHeadStamping && bHavePt && !bHeadStampOutside)
        HeadVolume->ApplyStamp(Pt, EffectiveHeadShape(), HeadBrushSize, HeadEditMode, HeadPaintColor);
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

    // Reconstruir/elegir el mesh de la brocha sólo si cambió tamaño, modo o el toggle de ojos.
    if (HeadPreviewSize != HeadBrushSize || HeadPreviewMode != HeadEditMode || bHeadPreviewEyesCached != bEyes)
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
        default:                 ToolMesh = HeadPreviewMeshAdd;   Mat = HeadPreviewMatAdd;   break; // Add
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
    }

    HeadPreviewActor->SetActorLocation(*At);
    // Paint/Ojos: orientar a la normal de la superficie (se "apoyan" sobre la malla). Add/Erase sin rotar.
    HeadPreviewActor->SetActorRotation((bPaint || bEyes) ? Normal.Rotation() : FRotator::ZeroRotator);
    // Color en vivo del preview (Add/Paint): mostrar el color seleccionado, incluso mientras lo editás.
    if (HeadPreviewMID) HeadPreviewMID->SetVectorParameterValue(TEXT("Color"), HeadPaintColor);
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

void APTLobbyPlayerController::OnHeadStampPressed()
{
    if (!bHeadSculptMode) return;
    if (bHeadEyesTool) { PlaceEyeAtCursor(); return; } // ojos: un ojo por click (no continuo)
    bHeadStamping = true;
}
void APTLobbyPlayerController::OnHeadStampReleased() { bHeadStamping = false; }
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
void APTLobbyPlayerController::OnHeadModeAdd()   { if (bHeadSculptMode) { HeadEditMode = EPTEditMode::Add;   bHeadEyesTool = false; } }
void APTLobbyPlayerController::OnHeadModeErase()
{
    if (!bHeadSculptMode) return;
    // Entrar en Borrar arranca siempre en Esfera (igual que el gameplay): es la forma esperada
    // para corregir, y una forma rara heredada de Agregar haría el primer borrado confuso.
    if (HeadEditMode != EPTEditMode::Erase) HeadStampShape = EPTStampShape::Sphere;
    HeadEditMode = EPTEditMode::Erase; bHeadEyesTool = false;
}
void APTLobbyPlayerController::OnHeadModePaint() { if (bHeadSculptMode) { HeadEditMode = EPTEditMode::Paint; bHeadEyesTool = false; } }
void APTLobbyPlayerController::OnHeadModeEyes()  { if (bHeadSculptMode) { bHeadEyesTool = true; } }
void APTLobbyPlayerController::OnHeadCycleShape()
{
    if (!bHeadSculptMode || bHeadEyesTool) return; // los ojos siempre son esferas
    switch (HeadStampShape)
    {
    case EPTStampShape::Sphere:   HeadStampShape = EPTStampShape::Cube;     break;
    case EPTStampShape::Cube:     HeadStampShape = EPTStampShape::Cylinder; break;
    case EPTStampShape::Cylinder: HeadStampShape = EPTStampShape::TriPrism; break;
    default:                      HeadStampShape = EPTStampShape::Sphere;   break;
    }
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

    // PAINT y OJOS: pegar el cursor a la SUPERFICIE (raymarch sobre la densidad), como el gameplay,
    // para trabajar preciso sobre la malla. Devuelve la normal (para apoyar el preview en el mesh).
    if (HeadEditMode == EPTEditMode::Paint || bHeadEyesTool)
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
        return false; // sin superficie bajo el cursor → no pinta ni muestra preview
    }

    // ADD/ERASE — IGUAL QUE EL GAMEPLAY (estilo SculptrVR): el sello NO cae sobre la superficie
    // —eso la haría crecer hacia la cámara y no dejaría hacer trazos laterales—, sino sobre un
    // PLANO a la profundidad de la cabeza, perpendicular a la cámara. Arrastrar desliza el sello.
    const FVector C = HeadVolume->GetActorLocation();      // centro de la cabeza (punto del plano)
    const FVector N = HeadCam->GetActorForwardVector();    // normal del plano = mirada de la cámara
    const float denom = FVector::DotProduct(Dir, N);
    if (FMath::Abs(denom) < 1e-4f) return false;           // rayo casi paralelo al plano
    const float t = FVector::DotProduct(C - Origin, N) / denom;
    if (t <= 0.f) return false;                            // el plano está detrás del cursor
    OutWorld  = Origin + Dir * t;
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

void APTLobbyPlayerController::EnterHeadSculpt()
{
    APawn* P = GetPawn();
    if (bHeadSculptMode || !HeadVolumeClass || !P || !GetWorld()) return;
    bHeadSculptMode = true;

    APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(P);

    // Personaje recto y quieto (sin baile ni jiggle del physics asset) mientras esculpís.
    // SetSculptPose evalúa la pose YA (RefreshBoneTransforms) → el socket queda recto ANTES de leerlo.
    if (Char) Char->SetSculptPose(true, HeadSculptPoseAnim);
    // Borrar la cabeza que ya tenía asignada: molesta para modelar una nueva desde cero.
    if (Char) Char->ClearHeadMesh();
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

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    HeadVolume = GetWorld()->SpawnActor<APTSculptVolume>(HeadVolumeClass, Center, SpawnRot, SP);

    // Bolita inicial de arcilla para que haya algo que esculpir (color piel neutro).
    if (HeadVolume)
        HeadVolume->ApplyStamp(Center, EPTStampShape::Sphere, 40.f, EPTEditMode::Add, FLinearColor(0.95f, 0.78f, 0.66f));

    // Reset de la herramienta de ojos + malla viva de ojos (pegada al volumen para verlos colocados).
    bHeadEyesTool = false;
    HeadEyes.Reset();
    if (HeadVolume)
    {
        HeadEyesLiveMesh = NewObject<UProceduralMeshComponent>(HeadVolume, TEXT("HeadEyesLiveMesh"));
        HeadEyesLiveMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        HeadEyesLiveMesh->SetCastShadow(false);
        HeadEyesLiveMesh->SetupAttachment(HeadVolume->GetRootComponent());
        HeadEyesLiveMesh->RegisterComponent();
    }

    // Actor de preview de la brocha (fantasma que sigue al cursor, como el gameplay).
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
        // Hornear arcilla + pintura + ojos, aplicar, guardar y REPLICAR (para que todos la vean).
        if (HeadVolume) Char->BakeAndReplicateHead(HeadVolume->GetMeshComponent(), HeadVolume, HeadEyes);
        Char->SetSculptPose(false);         // restaura el baile + jiggle
        Char->SetFacingArrowVisible(false); // ocultar la flecha
    }

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
