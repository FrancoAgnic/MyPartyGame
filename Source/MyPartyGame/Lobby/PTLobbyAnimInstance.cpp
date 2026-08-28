// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyAnimInstance.h"
#include "GameFramework/Pawn.h"
#include "../PTGameInstance.h"
#include "Engine/World.h"

void UPTLobbyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // Energía de la música (0..1) para que el personaje reaccione al beat. Vive en el GameInstance
    // (el envelope follower del submix de música la calcula). Es local/cosmético: no se replica.
    if (const UWorld* W = GetWorld())
        if (const UPTGameInstance* GI = W->GetGameInstance<UPTGameInstance>())
            MusicEnergy = GI->GetMusicEnergy();

    // Baile por FASE de BPM = sincronía perfecta con la música (sin latencia ni detección de beats).
    // La fase la da el GameInstance: beats transcurridos desde que arrancó la música (posición × BPM).
    // cos(π·beats/beatsPerSide) → +Max en un beat, -Max en el siguiente → vaivén de lado a lado clavado
    // al ritmo. Las físicas de los huesos de abajo cuelgan de este vaivén y lo siguen solas.
    float TargetRoll = 0.f;
    if (const UWorld* W = GetWorld())
        if (const UPTGameInstance* GI = W->GetGameInstance<UPTGameInstance>())
        {
            const float Beats = GI->GetMenuMusicBeats();
            if (Beats >= 0.f)
                TargetRoll = SwayMaxDeg * FMath::Cos(PI * Beats / FMath::Max(0.01f, SwayBeatsPerSide));
        }
    // Follow alto = prácticamente exacto, pero suaviza el snap del arranque y la costura del loop.
    SwayRollDeg  = FMath::FInterpTo(SwayRollDeg, TargetRoll, DeltaSeconds, SwayFollow);
    SwayPitchDeg = -FMath::Abs(SwayRollDeg) * 0.2f; // leve inclinación al frente en los extremos (opcional)

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
