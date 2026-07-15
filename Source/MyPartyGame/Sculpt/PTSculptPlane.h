// Copyright Epic Games, Inc. All Rights Reserved.
// Plano de esculpido en eje (teclas Z/X): la grilla que ves + una colisión que te impide
// atravesarlo mientras esculpís sobre él.
//
// POR QUÉ ES UN ACTOR REPLICADO Y NO SOLO LOCAL:
// el movimiento del personaje es autoritativo del servidor (CharacterMovement con predicción +
// corrección). Si la colisión existiera solo en la máquina del escultor, el servidor no la vería,
// simularía el movimiento sin bloqueo y CORREGIRÍA al cliente atravesando el plano igual. Así que
// el plano vive en el servidor y se replica; para que NO moleste a los que adivinan, cada pawn
// lo ignora al moverse (IgnoreActorWhenMoving) EXCEPTO el del escultor (TargetPawn). Esa regla es
// determinística en todos lados (servidor y clientes) → predicción y autoridad coinciden.
//
// El mesh (grilla + su colisión) lo asigna el usuario en un BP derivado: crear BP_SculptPlane,
// poner el static mesh de la grilla con SU colisión (más grande/gruesa que el plano) y asignar
// ese BP en APTSculptPlayerController::SculptPlaneClass.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PTSculptPlane.generated.h"

class UStaticMeshComponent;

UCLASS()
class MYPARTYGAME_API APTSculptPlane : public AActor
{
    GENERATED_BODY()

public:
    APTSculptPlane();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Mesh de la grilla + su colisión (asignar en el BP derivado).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sculpt")
    UStaticMeshComponent* PlaneMesh;

    // Pawn del escultor: el ÚNICO al que este plano bloquea. Lo setea el servidor al spawnearlo.
    UPROPERTY(ReplicatedUsing=OnRep_TargetPawn)
    APawn* TargetPawn = nullptr;

    // Servidor: fija a quién bloquea (y refresca los ignores locales).
    void SetTargetPawn(APawn* InPawn);

protected:
    virtual void BeginPlay() override;
    UFUNCTION() void OnRep_TargetPawn();

    // Hace que TODOS los pawns ignoren este plano al moverse, menos TargetPawn. Corre en el
    // servidor y en cada cliente con la misma regla → sin desacuerdos de colisión.
    void RefreshMoveIgnores();

private:
    // Los pawns pueden llegar/replicar después que el plano: refrescar los ignores un rato.
    FTimerHandle RefreshTimer;
};
