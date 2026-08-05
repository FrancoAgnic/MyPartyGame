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
};
