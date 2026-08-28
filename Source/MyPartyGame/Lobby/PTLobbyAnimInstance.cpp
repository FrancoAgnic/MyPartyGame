// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyAnimInstance.h"
#include "GameFramework/Pawn.h"
#include "../PTGameInstance.h"
#include "Engine/World.h"

void UPTLobbyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);

    // Energías de la música (del GameInstance): MusicEnergy = general; MusicHighEnergy = solo agudos.
    // El baile usa la AGUDA. Es local/cosmético: no se replica.
    float High = 0.f;
    if (const UWorld* W = GetWorld())
        if (const UPTGameInstance* GI = W->GetGameInstance<UPTGameInstance>())
        {
            MusicEnergy = GI->GetMusicEnergy();
            High        = GI->GetMusicHighEnergy();
        }

    // Detección por FLUJO: un golpe agudo = la energía aguda SUBE de golpe (High - PrevHigh grande) y
    // está por encima del piso. Así detecta cada nota alta nueva aunque los agudos sean sostenidos
    // (el umbral simple fallaba: disparaba una vez y se quedaba de un lado). Cada golpe alterna de lado.
    HitTimer  += DeltaSeconds;
    NoHitTime += DeltaSeconds;
    const float Flux = High - PrevHigh;
    if (Flux > HighFluxThreshold && High > HighMinLevel && HitTimer >= HitMinInterval)
    {
        SwayTarget = SwaySide * SwayMaxDeg; // golpe → al lado que toca
        SwaySide   = -SwaySide;             // el próximo golpe, al OTRO lado (uno y uno)
        HitTimer   = 0.f;
        NoHitTime  = 0.f;
    }
    PrevHigh = High;

    // Se sostiene en el lado del último golpe (para que se vea el cruce completo L→R); si hace rato que
    // no hay agudos (silencio), vuelve suave al centro.
    if (NoHitTime > 0.4f) SwayTarget = FMath::FInterpTo(SwayTarget, 0.f, DeltaSeconds, SwayReturnSpeed);

    SwayRollDeg  = FMath::FInterpTo(SwayRollDeg, SwayTarget, DeltaSeconds, SwaySnapSpeed);
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
