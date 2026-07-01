#include "PTSculptPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/SpringArmComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

APTSculptPlayerController::APTSculptPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
    bShowMouseCursor = false;
}

void APTSculptPlayerController::BeginPlay()
{
    Super::BeginPlay();

    SetInputMode(FInputModeGameOnly());

    // Find the sculpt volume placed in the level.
    Volume = Cast<APTSculptVolume>(UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass()));
    if (!Volume)
        UE_LOG(LogTemp, Warning, TEXT("[PTSculptPC] No APTSculptVolume found in level!"));
}

void APTSculptPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    if (!InputComponent) return;

    InputComponent->BindAction("SculptAdd",    IE_Pressed, this, &APTSculptPlayerController::SetModeAdd);
    InputComponent->BindAction("SculptRemove", IE_Pressed, this, &APTSculptPlayerController::SetModeRemove);
    InputComponent->BindAction("SculptSmooth", IE_Pressed, this, &APTSculptPlayerController::SetModeSmooth);
    InputComponent->BindAction("Sculpt",       IE_Pressed,  this, &APTSculptPlayerController::OnSculptPressed);
    InputComponent->BindAction("Sculpt",       IE_Released, this, &APTSculptPlayerController::OnSculptReleased);
    InputComponent->BindAction("SculptRadiusUp",   IE_Pressed, this, &APTSculptPlayerController::OnScrollUp);
    InputComponent->BindAction("SculptRadiusDown", IE_Pressed, this, &APTSculptPlayerController::OnScrollDown);
}

void APTSculptPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);

    if (!bIsSculpting || !Volume) return;

    FVector HitPoint;
    if (GetSculptHitPoint(HitPoint))
    {
        Volume->ApplyBrush(HitPoint, CurrentMode, BrushRadius, BrushStrength, DeltaTime);
#if WITH_EDITOR
        DrawDebugSphere(GetWorld(), HitPoint, BrushRadius, 12, FColor::Green, false, 0.05f);
#endif
    }
}

bool APTSculptPlayerController::GetSculptHitPoint(FVector& OutHit) const
{
    ACharacter* MyChar = Cast<ACharacter>(GetPawn());
    if (!MyChar) return false;

    // Find camera component on the pawn.
    UCameraComponent* Cam = MyChar->FindComponentByClass<UCameraComponent>();
    FVector  Start, Dir;
    if (Cam)
    {
        Start = Cam->GetComponentLocation();
        Dir   = Cam->GetForwardVector();
    }
    else
    {
        // Fallback: deproject screen center.
        int32 W, H;
        GetViewportSize(W, H);
        if (!DeprojectScreenPositionToWorld(W * 0.5f, H * 0.5f, Start, Dir))
            return false;
    }

    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(GetPawn());
    if (GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Dir * TraceDistance,
                                             ECC_Visibility, Params))
    {
        OutHit = Hit.ImpactPoint;
        return true;
    }
    return false;
}
