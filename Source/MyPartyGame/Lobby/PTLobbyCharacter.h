// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — Personaje del lobby con movimiento replicado (Enhanced Input).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PTLobbyCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

UCLASS()
class MYPARTYGAME_API APTLobbyCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    APTLobbyCharacter();

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaSeconds) override;

    // Velocidad de vuelo (modo creativo).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fly")
    float FlySpeed = 900.f;

    // Ventana para el doble toque de espacio que activa el vuelo.
    UPROPERTY(EditAnywhere, Category="Fly")
    float DoubleTapWindow = 0.30f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    USpringArmComponent* SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    UCameraComponent* Camera;

    // Asignar en BP_LobbyCharacter o en el PlayerController/GameMode.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
    UInputAction* MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
    UInputAction* LookAction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
    UInputAction* JumpAction;

private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    // ── Vuelo (modo creativo Minecraft) ─────────────────────────────────────
    void OnJumpPressed();
    void OnJumpReleased();
    void OnDescendPressed()  { bDescend = true;  }
    void OnDescendReleased() { bDescend = false; }
    void ToggleFly();

    bool  bFlying   = false;
    bool  bAscend   = false;
    bool  bDescend  = false;
    float LastJumpTime = -10.f;
};
