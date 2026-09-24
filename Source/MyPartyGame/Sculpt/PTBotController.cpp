// TRAILER-ONLY (rama trailer-bots). Ver PTBotController.h.

#include "PTBotController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"

APTBotController::APTBotController()
{
    // El bot necesita PlayerState para salir en el scoreboard (nombre + puntaje). Los AIController por
    // defecto NO lo crean; esto lo fuerza (el motor usa el PlayerStateClass del GameMode = APTPlayerState).
    bWantsPlayerState = true;
    PrimaryActorTick.bCanEverTick = true;
}

void APTBotController::InitWander(const FVector& InCenter, float InMinRadius, float InMaxRadius)
{
    Center    = InCenter;
    MinRadius = FMath::Max(50.f, InMinRadius);
    MaxRadius = FMath::Max(MinRadius + 50.f, InMaxRadius);
    PickNewTarget();
}

void APTBotController::PickNewTarget()
{
    // Punto random en una "cáscara" esférica alrededor del cubo (fuera de él → no intentan entrar).
    const float R = FMath::FRandRange(MinRadius, MaxRadius);
    const float Theta = FMath::FRandRange(0.f, 2.f * PI);          // azimut
    const float Phi   = FMath::FRandRange(-0.55f, 0.55f);          // banda vertical (no muy arriba/abajo)
    const FVector Dir(FMath::Cos(Theta) * FMath::Cos(Phi),
                      FMath::Sin(Theta) * FMath::Cos(Phi),
                      FMath::Sin(Phi));
    CurrentTarget = Center + Dir * R;
    RepathAccum   = 0.f;
    RepathEvery   = FMath::FRandRange(2.5f, 5.0f); // cambia de rumbo cada tanto aunque no llegue
    bHasTarget    = true;
}

void APTBotController::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    APawn* P = GetPawn();
    if (!P) return;
    if (!bHasTarget) PickNewTarget();

    const FVector Loc = P->GetActorLocation();
    FVector ToTarget  = CurrentTarget - Loc;
    const float Dist  = ToTarget.Size();

    // Llegó (o venció el tiempo del tramo) → nuevo destino.
    RepathAccum += DeltaSeconds;
    if (Dist < 120.f || RepathAccum >= RepathEvery)
    {
        PickNewTarget();
        ToTarget = CurrentTarget - Loc;
    }

    // Empuje suave hacia el destino (el pawn está en modo vuelo → AddMovementInput mueve en 3D).
    const FVector InputDir = ToTarget.GetSafeNormal();
    if (!InputDir.IsNearlyZero())
    {
        P->AddMovementInput(InputDir, 0.65f); // <1 para que floten tranquilos, no a full velocidad
        // Orientar el personaje hacia donde va (el pawn de juego no orienta al movimiento solo).
        const FRotator Face = FRotationMatrix::MakeFromX(FVector(InputDir.X, InputDir.Y, 0.f)).Rotator();
        P->SetActorRotation(FMath::RInterpTo(P->GetActorRotation(), Face, DeltaSeconds, 4.f));
    }
}
