// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyAnimInstance.h"
#include "GameFramework/Pawn.h"

void UPTLobbyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    if (const APawn* Owner = TryGetPawnOwner())
    {
        // Solo la componente HORIZONTAL: en el Lvl-01 se vuela, y no queremos que subir/bajar
        // dispare la animación de caminar.
        const FVector V = Owner->GetVelocity();
        Speed = FVector(V.X, V.Y, 0.f).Size();
        bIsMoving = Speed > MoveSpeedThreshold;

        // Pitch de la mirada (aim base): normalizado a [-90, 90] para curvar la columna hacia
        // donde mira el personaje. Se usa el aim base porque en Lvl-01 el yaw del actor sigue a la
        // cámara (bUseControllerRotationYaw) y el pitch de la mirada refleja arriba/abajo.
        const float P = FRotator::NormalizeAxis(Owner->GetBaseAimRotation().Pitch);
        AimPitch = FMath::Clamp(P, -90.f, 90.f);
    }
    else
    {
        Speed = 0.f;
        bIsMoving = false;
        AimPitch = 0.f;
    }
}
