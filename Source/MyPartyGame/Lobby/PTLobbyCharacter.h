// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — Personaje del lobby con movimiento replicado (Enhanced Input).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PTLobbyCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UWidgetComponent;
struct FInputActionValue;

UCLASS()
class MYPARTYGAME_API APTLobbyCharacter : public ACharacter
{
    GENERATED_BODY()

public:
    APTLobbyCharacter();

    // Fuerza el modo vuelo creativo on/off (ej: el nivel de esculpido arranca volando,
    // porque es un vacío sin piso caminable). Reutiliza la lógica de ToggleFly.
    void SetFlyingMode(bool bEnable);

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void Tick(float DeltaSeconds) override;

    // Velocidad de vuelo (modo creativo).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fly")
    float FlySpeed = 900.f;

    // Aceleración en vuelo (menor = rampa de 0 a máxima más progresiva).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Fly")
    float FlyAcceleration = 1500.f;

    // Ventana para el doble toque de espacio que activa el vuelo.
    UPROPERTY(EditAnywhere, Category="Fly")
    float DoubleTapWindow = 0.30f;

    // Primera persona: cámara directo en el pawn (sin spring arm).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    UCameraComponent* Camera;

    // Cartel con el nombre sobre la cabeza. Asignale su Widget Class (WBP_NameTag,
    // reparentado a UPTNameTagWidget) en el Blueprint. El nombre lo pone C++.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="NameTag")
    UWidgetComponent* NameTag;

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
    float DefaultMaxAccel = 2048.f; // MaxAcceleration para caminar (se restaura al salir de vuelo)

    // Cartel del nombre: actualizado throttled desde Tick.
    void  UpdateNameTag();
    float NameTagAccum = 0.f;
};
