// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 1 — Pawn de la silla-jugador (v0: caminar y saltar; sprint/stamina/empujón
// llegan en Fase 3). A la vista debe ser IDÉNTICO a ASillasSenuelo — el camuflaje
// es la mecánica central (D9). La cámara es tercera persona con spring arm.
// El "mind game" de D4: moverse está siempre permitido, ser visto es sentencia.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SillasPawnSilla.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class MYPARTYGAME_API ASillasPawnSilla : public ACharacter
{
    GENERATED_BODY()

public:
    ASillasPawnSilla();

    virtual void BeginPlay() override;
    virtual void PawnClientRestart() override;

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Caja idéntica al señuelo, colgada de la cápsula.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sillas")
    TObjectPtr<UStaticMeshComponent> ChairMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> Camera;

    // Input (defaults: los assets del template ThirdPerson; reemplazables en BP).
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputMappingContext> MappingContext;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputMappingContext> MouseLookMappingContext;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> MouseLookAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> JumpAction;

private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
};
