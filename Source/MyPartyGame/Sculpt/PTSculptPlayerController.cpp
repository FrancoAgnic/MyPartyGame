#include "PTSculptPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "InputMappingContext.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DecalComponent.h"
#include "Components/BoxComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerState.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "../UI/PTColorPickerWidget.h"
#include "../Lobby/PTLobbyEscapeMenuWidget.h"
#include "../Lobby/PTPlayerState.h"
#include "PTSculptGameMode.h"
#include "PTSculptGameState.h"
#include "PTGameplayHUDWidget.h"

APTSculptPlayerController::APTSculptPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = false;
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

        // Mesh estático opcional (cuando el usuario asigna sus propios meshes).
        PreviewStaticMesh = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("PreviewStaticMesh"));
        PreviewStaticMesh->SetupAttachment(PreviewMesh);
        PreviewStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PreviewStaticMesh->SetCastShadow(false);
        PreviewStaticMesh->SetReceivesDecals(false);
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

        // Overlay X-ray: capa extra encima de cada preview (material base intacto).
        if (PreviewOverlayMaterial)
        {
            PreviewMesh->SetOverlayMaterial(PreviewOverlayMaterial);
            PreviewStaticMesh->SetOverlayMaterial(PreviewOverlayMaterial);
            AxisGizmo->SetOverlayMaterial(PreviewOverlayMaterial);
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
}

void APTSculptPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent) return;

    // Stamp
    InputComponent->BindAction("Sculpt", IE_Pressed,  this, &APTSculptPlayerController::OnStampPressed);
    InputComponent->BindAction("Sculpt", IE_Released, this, &APTSculptPlayerController::OnStampReleased);

    // Tamaño con rueda
    InputComponent->BindKey(EKeys::MouseScrollUp,   IE_Pressed, this, &APTSculptPlayerController::OnScrollUp);
    InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &APTSculptPlayerController::OnScrollDown);

    // Formas: 1 2 3 4
    InputComponent->BindKey(EKeys::One,   IE_Pressed, this, &APTSculptPlayerController::SetShapeSphere);
    InputComponent->BindKey(EKeys::Two,   IE_Pressed, this, &APTSculptPlayerController::SetShapeCube);
    InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &APTSculptPlayerController::SetShapeCylinder);
    InputComponent->BindKey(EKeys::Four,  IE_Pressed, this, &APTSculptPlayerController::SetShapeTriPrism);

    // Modos: Tab cicla Add→Erase→Paint
    InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &APTSculptPlayerController::CycleModes);
    InputComponent->BindKey(EKeys::X,   IE_Pressed, this, &APTSculptPlayerController::ToggleAxisLock);

    // Color picker: C
    InputComponent->BindKey(EKeys::C, IE_Pressed, this, &APTSculptPlayerController::OpenColorPicker);

    // Menú de pausa: Esc
    InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &APTSculptPlayerController::OnPausePressed);

    // Chat: Enter abre/enfoca el chat (al enviar vuelve el control al juego).
    InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &APTSculptPlayerController::OnOpenChat);
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

// ── Tick ─────────────────────────────────────────────────────────────────────

void APTSculptPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

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

    // Solo el escultor del turno ve los previews de las herramientas. Para el resto,
    // ocultar el actor de preview y no calcular nada de la brocha.
    if (!CanLocalPlayerSculpt())
    {
        if (PreviewActor) PreviewActor->SetActorHiddenInGame(true);
        bStrokeActive = false;
        return;
    }
    if (PreviewActor) PreviewActor->SetActorHiddenInGame(false);

    FVector Normal;
    FVector StampPos = GetStampPoint(Normal);

    // Preview: actualizar cuando cambia forma, tamaño o modo (tool).
    if (PreviewMesh && (bPreviewDirty
        || CachedPreviewShape != StampShape
        || CachedPreviewSize  != StampSize
        || CachedPreviewMode  != EditMode
        || CachedPreviewColor != CurrentPaintColor))
    {
        UpdatePreviewVisual();
        CachedPreviewShape = StampShape;
        CachedPreviewSize  = StampSize;
        CachedPreviewMode  = EditMode;
        CachedPreviewColor = CurrentPaintColor;
        bPreviewDirty      = false;
    }

    // Mover preview al punto de stamp
    if (PreviewActor)
        PreviewActor->SetActorLocation(StampPos);

    // Gizmo de ejes: visible solo en modo eje con la herramienta Add.
    if (AxisGizmo)
    {
        const bool bShowGizmo = bAxisLock && EditMode == EPTEditMode::Add;
        AxisGizmo->SetVisibility(bShowGizmo);
        if (bShowGizmo)
        {
            // Orientar al plano vertical activo (bloqueado si se esculpe, o el actual).
            FVector N = AxisPlaneN;
            if (!bIsStamping)
            {
                FVector S, D;
                if (GetCameraRay(S, D))
                {
                    N = FVector(D.X, D.Y, 0.f).GetSafeNormal();
                    if (N.IsNearlyZero()) N = FVector(1.f, 0.f, 0.f);
                }
            }
            AxisGizmo->SetWorldLocation(StampPos);
            AxisGizmo->SetWorldRotation(N.Rotation());
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
        bool bTint = false;
        if (EditMode == EPTEditMode::Paint)
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

        const bool bShow = (RingMesh != nullptr);
        PaintRing->SetVisibility(bShow);
        if (bShow)
        {
            if (CachedRingMesh != RingMesh)
            {
                PaintRing->SetStaticMesh(RingMesh);
                // MID solo para Paint (toma el color); Smooth usa su material tal cual.
                PaintRingMID = nullptr;
                if (bTint)
                {
                    if (UMaterialInterface* M = PaintRing->GetMaterial(0))
                        PaintRingMID = PaintRing->CreateDynamicMaterialInstance(0, M);
                }
                CachedRingMesh = RingMesh;
            }
            PaintRing->SetWorldLocation(StampPos);
            PaintRing->SetWorldRotation(FRotationMatrix::MakeFromZ(Normal).Rotator());
            const float Base = FMath::Max(PreviewMeshBaseSize, 1.f);
            PaintRing->SetWorldScale3D(FVector(StampSize / Base));
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
                Server_ApplyStamp(P, Sh, StampSize, EditMode, CurrentPaintColor);
            }
        }
        else
        {
            Server_ApplyStamp(StampPos, Sh, StampSize, EditMode, CurrentPaintColor);
            bStrokeActive = true;
        }
        LastStampPos = StampPos;
    }
    else
    {
        bStrokeActive = false;
    }
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

FVector APTSculptPlayerController::GetStampPoint(FVector& OutNormal) const
{
    FVector Start, Dir;
    if (!GetCameraRay(Start, Dir))
    {
        OutNormal = FVector::UpVector;
        return Start + Dir * AirDepth;
    }

    // ── Modo eje: trazo recto sobre el plano vertical bloqueado al inicio ───
    // Solo con la herramienta de esculpir (Add). El rayo se interseca con el
    // plano (contiene Z mundo): vertical plomada + diagonales verticales rectas.
    if (bAxisLock && bIsStamping && EditMode == EPTEditMode::Add)
    {
        const float denom = FVector::DotProduct(Dir, AxisPlaneN);
        FVector Pf = AxisOrigin;
        if (FMath::Abs(denom) > 1e-4f)
        {
            const float t = FVector::DotProduct(AxisOrigin - Start, AxisPlaneN) / denom;
            if (t > 0.f) Pf = Start + Dir * t;
        }
        OutNormal = AxisPlaneN;
        return Pf;
    }

    // ── Paint y Smooth: pegar el cursor a la superficie (raymarch) para trabajar
    // preciso sobre la malla donde apuntás. ────────────────────────────────────
    if ((EditMode == EPTEditMode::Paint || EditMode == EPTEditMode::Smooth) && Volume)
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
    return Start + Dir * AirDepth;
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
    APTSculptVolume::BuildStampPreview(EffectiveShape(), StampSize, Volume->VoxelSize, Verts, Tris, Normals);

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

    auto ApplyTo = [&](UMeshComponent* Comp)
    {
        if (!Comp) return;
        if (bTint)
        {
            UMaterialInstanceDynamic* MID = Comp->CreateDynamicMaterialInstance(0, Mat);
            if (MID) MID->SetVectorParameterValue(TEXT("Color"), CurrentPaintColor);
        }
        else
        {
            Comp->SetMaterial(0, Mat);
        }
    };
    ApplyTo(PreviewMesh);
    ApplyTo(PreviewStaticMesh);
}

// Elige el mesh de preview: override por tool > override por stamp > procedural.
void APTSculptPlayerController::UpdatePreviewVisual()
{
    if (!PreviewMesh) return;

    // Smooth (decal) y Paint (cursor 2D): sin malla de preview 3D.
    if (EditMode == EPTEditMode::Smooth || EditMode == EPTEditMode::Paint)
    {
        PreviewMesh->SetVisibility(false);
        if (PreviewStaticMesh) PreviewStaticMesh->SetVisibility(false);
        return;
    }

    UStaticMesh* ToolMesh = nullptr;
    switch (EditMode)
    {
    case EPTEditMode::Add:    ToolMesh = PreviewToolMeshAdd;    break;
    case EPTEditMode::Erase:  ToolMesh = PreviewToolMeshErase;  break;
    case EPTEditMode::Smooth: ToolMesh = PreviewToolMeshSmooth; break;
    case EPTEditMode::Paint:  ToolMesh = PreviewToolMeshPaint;  break;
    }
    UStaticMesh* ShapeMesh = nullptr;
    switch (EffectiveShape()) // Erase siempre esfera
    {
    case EPTStampShape::Sphere:   ShapeMesh = PreviewMeshSphere;   break;
    case EPTStampShape::Cube:     ShapeMesh = PreviewMeshCube;     break;
    case EPTStampShape::Cylinder: ShapeMesh = PreviewMeshCylinder; break;
    case EPTStampShape::TriPrism: ShapeMesh = PreviewMeshTriPrism; break;
    }
    // Prioridad SHAPE > TOOL: así al cambiar de shape el preview cambia (bug fix).
    // Si no hay mesh por shape ni por tool, se usa el procedural (también por shape).
    UStaticMesh* Chosen = ShapeMesh ? ShapeMesh : ToolMesh;

    if (Chosen && PreviewStaticMesh)
    {
        // Usar el mesh estático del usuario, escalado al tamaño de brocha.
        PreviewStaticMesh->SetStaticMesh(Chosen);
        const float Base = FMath::Max(PreviewMeshBaseSize, 1.f);
        PreviewStaticMesh->SetWorldScale3D(FVector(StampSize / Base));
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
}

// ── Acciones de input ─────────────────────────────────────────────────────────

void APTSculptPlayerController::OnStampPressed()
{
    bIsStamping = true;

    // En modo eje (solo Add): bloquear un plano VERTICAL (que contiene el eje Z
    // global) que mira hacia la cámara. Movimiento libre y recto dentro del plano.
    if (bAxisLock && EditMode == EPTEditMode::Add)
    {
        FVector Start, Dir;
        if (GetCameraRay(Start, Dir))
        {
            AxisOrigin = Start + Dir * AirDepth;
            FVector N(Dir.X, Dir.Y, 0.f);            // normal horizontal → plano vertical
            AxisPlaneN = N.GetSafeNormal();
            if (AxisPlaneN.IsNearlyZero()) AxisPlaneN = FVector(1, 0, 0); // mirando recto arriba/abajo
        }
    }
}

void APTSculptPlayerController::OnStampReleased() { bIsStamping = false; AxisChosen = -1; }

void APTSculptPlayerController::ToggleAxisLock()
{
    bAxisLock = !bAxisLock;
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Axis lock: %s"), bAxisLock ? TEXT("ON") : TEXT("OFF"));
}

void APTSculptPlayerController::OnScrollUp()
{
    StampSize += SizeStep;
    ClampStampSize();
    bPreviewDirty = true;
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Size: %.0f"), StampSize);
}

void APTSculptPlayerController::OnScrollDown()
{
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

void APTSculptPlayerController::CycleModes()
{
    switch (EditMode)
    {
    case EPTEditMode::Add:    EditMode = EPTEditMode::Erase;  break;
    case EPTEditMode::Erase:  EditMode = EPTEditMode::Smooth; break;
    case EPTEditMode::Smooth: EditMode = EPTEditMode::Paint;  break;
    case EPTEditMode::Paint:  EditMode = EPTEditMode::Add;    break;
    }
    ClampStampSize(); // respetar el mínimo del nuevo modo (Paint permite más chico)
    bPreviewDirty = true;
    ApplyPreviewMaterial();
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Mode: %d"), (int32)EditMode);
}

void APTSculptPlayerController::OpenColorPicker()
{
    if (!ColorPickerClass) return;

    // Segunda C con el menú abierto → confirma el color actual y cierra (como Apply).
    if (ColorPicker)
    {
        if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(ColorPicker))
        {
            CP->Confirm(); // aplica color, pasa a Paint, cierra y restaura input
        }
        else
        {
            ColorPicker->RemoveFromParent();
            ColorPicker = nullptr;
            SetInputMode(FInputModeGameOnly());
            bShowMouseCursor = false;
        }
        return;
    }

    ColorPicker = CreateWidget<UUserWidget>(this, ColorPickerClass);
    if (ColorPicker)
    {
        ColorPicker->AddToViewport(10);
        SetInputMode(FInputModeGameAndUI());
        bShowMouseCursor = true;
    }
}

void APTSculptPlayerController::OnColorConfirmed(FLinearColor NewColor)
{
    CurrentPaintColor = NewColor;
    EditMode          = EPTEditMode::Paint;
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
    float Size, EPTEditMode Mode, FLinearColor PaintColor)
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
        Volume->Multicast_ApplyStamp(WorldPos, Shape, Size, Mode, PaintColor);
}

