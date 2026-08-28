// Copyright Epic Games, Inc. All Rights Reserved.
// AnimInstance del personaje del lobby/juego: calcula en C++ la velocidad para elegir Idle/Caminar,
// así en el AnimBP solo hay que ELEGIR las animaciones (no hace falta el nodo de velocidad a mano).

#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PTLobbyAnimInstance.generated.h"

UCLASS()
class MYPARTYGAME_API UPTLobbyAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    virtual void NativeUpdateAnimation(float DeltaSeconds) override;

    /** Rapidez horizontal actual del personaje (cm/s). Leerla en el AnimGraph para Idle↔Caminar. */
    UPROPERTY(BlueprintReadOnly, Category="Locomotion")
    float Speed = 0.f;

    /** true si se está moviendo (Speed por encima del umbral). Cómodo para "Blend Poses by bool". */
    UPROPERTY(BlueprintReadOnly, Category="Locomotion")
    bool bIsMoving = false;

    /** Umbral (cm/s) para considerar que camina. Ajustable en los Class Defaults del AnimBP. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category="Locomotion")
    float MoveSpeedThreshold = 10.f;

    /** Pitch de la MIRADA en grados [-90, 90] (negativo = mirando arriba, positivo = abajo, según el
     *  aim base del pawn). Úsalo en el AnimGraph (Transform Modify Bone sobre la columna) para que el
     *  personaje se agache/curve según hacia dónde mira. 0 si no hay pawn. */
    UPROPERTY(BlueprintReadOnly, Category="Aim")
    float AimPitch = 0.f;

    /** Energía de la música (0..1, del GameInstance). Para que el personaje "baile" tipo muñeco
     *  inflable: usala en el AnimGraph para modular la fuerza de los AnimDynamics o un sway senoidal. */
    UPROPERTY(BlueprintReadOnly, Category="Music")
    float MusicEnergy = 0.f;

    // ── Sway listo para usar (ya combina tiempo + MusicEnergy) ──────────────────────────────
    // Enchufá estos dos ángulos (grados) en un "Transform (Modify) Bone" sobre la columna:
    // Roll = SwayRollDeg, Pitch = SwayPitchDeg → bamboleo tipo muñeco inflable que crece con la música.
    UPROPERTY(BlueprintReadOnly, Category="Music") float SwayRollDeg = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Music") float SwayPitchDeg = 0.f;

    // Tuneables del baile (Class Defaults del AnimBP). Detecta cada NOTA ALTA nueva por FLUJO (subida
    // brusca de la energía aguda) → alterna SÍ O SÍ de lado (uno y uno). Funciona con cualquier música
    // (no hay BPM ni nada timeado). En silencio queda quieto en el centro.
    UPROPERTY(EditDefaultsOnly, Category="Music|Sway") float HighFluxThreshold = 0.06f; // cuánto tiene que SUBIR el agudo para contar como golpe (bajar = más sensible)
    UPROPERTY(EditDefaultsOnly, Category="Music|Sway") float HighMinLevel      = 0.05f; // piso: por debajo no cuenta (gate de silencio)
    UPROPERTY(EditDefaultsOnly, Category="Music|Sway") float HitMinInterval    = 0.09f; // seg mínimos entre golpes (bajo = permite cruces muy rápidos)
    UPROPERTY(EditDefaultsOnly, Category="Music|Sway") float SwayMaxDeg        = 45.f;  // inclinación a cada lado (±). Neutro del hueso = su rest (p.ej. 90°)
    UPROPERTY(EditDefaultsOnly, Category="Music|Sway") float SwaySnapSpeed     = 22.f;  // qué tan rápido cruza al lado en el golpe
    UPROPERTY(EditDefaultsOnly, Category="Music|Sway") float SwayReturnSpeed   = 5.f;   // qué tan rápido vuelve al centro cuando no hay agudos

private:
    float SwaySide   = 1.f;       // lado del próximo golpe (+1 / -1), alterna sí o sí
    float SwayTarget = 0.f;       // objetivo (se fija en el golpe y se sostiene; vuelve al centro en silencio)
    float PrevHigh   = 0.f;       // energía aguda del frame anterior (para el flujo)
    float HitTimer   = 0.f;       // cooldown entre golpes
    float NoHitTime  = 0.f;       // hace cuánto que no hay golpe (para volver al centro)
};
