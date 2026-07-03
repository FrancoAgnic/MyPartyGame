#include "PTSculptPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputSubsystems.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Blueprint/UserWidget.h"

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

    // Color picker: C
    InputComponent->BindKey(EKeys::C, IE_Pressed, this, &APTSculptPlayerController::OpenColorPicker);
}

// ── Tick ─────────────────────────────────────────────────────────────────────

void APTSculptPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (!Volume) return;

    FVector Normal;
    FVector StampPos = GetStampPoint(Normal);

    // Preview: reconstruir solo cuando cambia forma o tamaño
    if (PreviewMesh && (bPreviewDirty || CachedPreviewShape != StampShape || CachedPreviewSize != StampSize))
    {
        RebuildPreviewMesh();
        CachedPreviewShape = StampShape;
        CachedPreviewSize  = StampSize;
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

FVector APTSculptPlayerController::GetStampPoint(FVector& OutNormal) const
{
    ACharacter* MyChar = Cast<ACharacter>(GetPawn());
    FVector Start, Dir;

    UCameraComponent* Cam = MyChar ? MyChar->FindComponentByClass<UCameraComponent>() : nullptr;
    if (Cam)
    {
        Start = Cam->GetComponentLocation();
        Dir   = Cam->GetForwardVector();
    }
    else
    {
        int32 W, H;
        GetViewportSize(W, H);
        if (!DeprojectScreenPositionToWorld(W * 0.5f, H * 0.5f, Start, Dir))
        {
            OutNormal = FVector::UpVector;
            return Start + Dir * AirDepth;
        }
    }

    // Cursor tipo SculptrVR: siempre a distancia fija (brazo extendido).
    // No se snapea a la superficie — el stamp se aplica justo donde está la preview.
    // La normal apunta hacia la cámara para orientar el decal.
    OutNormal = -Dir;
    return Start + Dir * AirDepth;
}

// ── Preview mesh ─────────────────────────────────────────────────────────────

void APTSculptPlayerController::RebuildPreviewMesh()
{
    if (!PreviewMesh || !Volume) return;

    TArray<FVector> Verts, Normals;
    TArray<int32>   Tris;
    APTSculptVolume::BuildStampPreview(StampShape, StampSize, Volume->VoxelSize, Verts, Tris, Normals);
    PreviewMesh->CreateMeshSection(0, Verts, Tris, Normals, {}, {}, {}, false);

    if (PreviewMeshMaterial)
        PreviewMesh->SetMaterial(0, PreviewMeshMaterial);
}

// ── Acciones de input ─────────────────────────────────────────────────────────

void APTSculptPlayerController::OnStampPressed()  { bIsStamping = true;  }
void APTSculptPlayerController::OnStampReleased() { bIsStamping = false; }

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
    case EPTEditMode::Add:   EditMode = EPTEditMode::Erase; break;
    case EPTEditMode::Erase: EditMode = EPTEditMode::Paint; break;
    case EPTEditMode::Paint: EditMode = EPTEditMode::Add;   break;
    }
    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Mode: %d"), (int32)EditMode);
}

void APTSculptPlayerController::OpenColorPicker()
{
    if (!ColorPickerClass) return;

    if (ColorPicker)
    {
        ColorPicker->RemoveFromParent();
        ColorPicker = nullptr;
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

    if (ColorPicker)
    {
        ColorPicker->RemoveFromParent();
        ColorPicker = nullptr;
    }

    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = false;

    UE_LOG(LogTemp, Log, TEXT("[Sculpt] Paint color set, mode=Paint"));
}

void APTSculptPlayerController::AddPitchInput(float Val)
{
    Super::AddPitchInput(bInvertPitch ? -Val : Val);
}
