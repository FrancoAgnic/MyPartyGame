#include "PTSculptPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "../UI/PTColorPickerWidget.h"

APTSculptPlayerController::APTSculptPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = false;
}

void APTSculptPlayerController::BeginPlay()
{
    Super::BeginPlay();

    SetInputMode(FInputModeGameOnly());

    if (UEnhancedInputLocalPlayerSubsystem* Sub =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (MovementMappingContext)
            Sub->AddMappingContext(MovementMappingContext, 0);
    }

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
        PreviewMesh->RegisterComponent();
        PreviewActor->SetRootComponent(PreviewMesh);
        if (PreviewMeshMaterial)
            PreviewMesh->SetMaterial(0, PreviewMeshMaterial);

        // Mesh estático opcional (cuando el usuario asigna sus propios meshes).
        PreviewStaticMesh = NewObject<UStaticMeshComponent>(PreviewActor, TEXT("PreviewStaticMesh"));
        PreviewStaticMesh->SetupAttachment(PreviewMesh);
        PreviewStaticMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        PreviewStaticMesh->SetCastShadow(false);
        PreviewStaticMesh->RegisterComponent();
        PreviewStaticMesh->SetVisibility(false);
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
}

void APTSculptPlayerController::OnPausePressed()
{
    // Toggle directo del menú de pausa (WBP_Settings u otro).
    if (PauseMenu && PauseMenu->IsInViewport())
    {
        PauseMenu->RemoveFromParent();
        PauseMenu = nullptr;
        SetInputMode(FInputModeGameOnly());
        bShowMouseCursor = false;
        return;
    }

    if (!PauseMenuClass) return;
    PauseMenu = CreateWidget<UUserWidget>(this, PauseMenuClass);
    if (PauseMenu)
    {
        PauseMenu->AddToViewport(10);
        SetInputMode(FInputModeGameAndUI());
        bShowMouseCursor = true;
    }
}

// ── Tick ─────────────────────────────────────────────────────────────────────

void APTSculptPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    // Si el menú de pausa se cerró desde adentro (ej. botón Back), restaurar input.
    if (PauseMenu && !PauseMenu->IsInViewport())
    {
        PauseMenu = nullptr;
        SetInputMode(FInputModeGameOnly());
        bShowMouseCursor = false;
    }

    if (!Volume) return;

    FVector Normal;
    FVector StampPos = GetStampPoint(Normal);

    // Preview: actualizar cuando cambia forma, tamaño o modo (tool).
    if (PreviewMesh && (bPreviewDirty
        || CachedPreviewShape != StampShape
        || CachedPreviewSize  != StampSize
        || CachedPreviewMode  != EditMode))
    {
        UpdatePreviewVisual();
        CachedPreviewShape = StampShape;
        CachedPreviewSize  = StampSize;
        CachedPreviewMode  = EditMode;
        bPreviewDirty      = false;
    }

    // Mover preview al punto de stamp
    if (PreviewActor)
        PreviewActor->SetActorLocation(StampPos);

    // Decal indicador
    if (BrushDecalMaterial)
    {
        FRotator DecalRot = (-Normal).Rotation();
        UGameplayStatics::SpawnDecalAtLocation(
            GetWorld(), BrushDecalMaterial,
            FVector(StampSize * 0.5f),
            StampPos, DecalRot, 0.12f);
    }

    // Aplicar stamp si se está presionando
    if (bIsStamping)
        Volume->Server_ApplyStamp(StampPos, StampShape, StampSize, EditMode, CurrentPaintColor);
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
    // El rayo se interseca con el plano (contiene Z mundo). Movimiento libre y
    // recto dentro del plano: vertical plomada + diagonales verticales rectas.
    if (bAxisLock && bIsStamping)
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
    APTSculptVolume::BuildStampPreview(StampShape, StampSize, Volume->VoxelSize, Verts, Tris, Normals);
    PreviewMesh->CreateMeshSection(0, Verts, Tris, Normals, {}, {}, {}, false);

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
    if (PreviewMesh)       PreviewMesh->SetMaterial(0, Mat);
    if (PreviewStaticMesh) PreviewStaticMesh->SetMaterial(0, Mat);
}

// Elige el mesh de preview: override por tool > override por stamp > procedural.
void APTSculptPlayerController::UpdatePreviewVisual()
{
    if (!PreviewMesh) return;

    UStaticMesh* ToolMesh = nullptr;
    switch (EditMode)
    {
    case EPTEditMode::Add:    ToolMesh = PreviewToolMeshAdd;    break;
    case EPTEditMode::Erase:  ToolMesh = PreviewToolMeshErase;  break;
    case EPTEditMode::Smooth: ToolMesh = PreviewToolMeshSmooth; break;
    case EPTEditMode::Paint:  ToolMesh = PreviewToolMeshPaint;  break;
    }
    UStaticMesh* ShapeMesh = nullptr;
    switch (StampShape)
    {
    case EPTStampShape::Sphere:   ShapeMesh = PreviewMeshSphere;   break;
    case EPTStampShape::Cube:     ShapeMesh = PreviewMeshCube;     break;
    case EPTStampShape::Cylinder: ShapeMesh = PreviewMeshCylinder; break;
    case EPTStampShape::TriPrism: ShapeMesh = PreviewMeshTriPrism; break;
    }
    UStaticMesh* Chosen = ToolMesh ? ToolMesh : ShapeMesh;

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

    // En modo eje: bloquear un plano VERTICAL (que contiene el eje Z global) y
    // que mira hacia la cámara. Dentro del plano el movimiento es libre y recto:
    // vertical = plomada exacta (Z mundo), y diagonales verticales rectas entre
    // el horizontal y Z. Sin snap a un solo eje, sin curvatura de cámara.
    if (bAxisLock)
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
    StampSize = FMath::Max(20.f, StampSize + SizeStep);
    bPreviewDirty = true;
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Size: %.0f"), StampSize);
}

void APTSculptPlayerController::OnScrollDown()
{
    StampSize = FMath::Max(20.f, StampSize - SizeStep);
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

