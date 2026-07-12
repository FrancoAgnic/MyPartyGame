// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — Personaje del lobby con movimiento replicado (Enhanced Input).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PTHeadSaveGame.h"
#include "PTLobbyCharacter.generated.h"

class UMaterialInterface;

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UWidgetComponent;
class UAnimMontage;
class UNiagaraSystem;
class UProceduralMeshComponent;
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

    // Globo de chat: muestra el texto en el cartel del nombre ~2s (lo ven todos). Si
    // bGuess=true, va en verde ("adivinó la palabra") y spawnea el confetti. Lo llama
    // el GameMode (servidor). El mensaje NO es la palabra (anti-spoiler).
    UFUNCTION(NetMulticast, Reliable) void Multicast_ShowChatBubble(const FString& Text, bool bGuess);

    // Sistema de partículas de confetti al adivinar. Asignar en el BP.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="FX")
    UNiagaraSystem* ConfettiFX = nullptr;

    // ── Cabeza custom (esculpida por el jugador) ────────────────────────────────
    // Malla procedural pegada al hueso "HeadSocket" del mesh: baila con el personaje.
    // Se le copia la geometría de la cabeza que esculpe el jugador (ver SetHeadMeshFrom).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Head")
    UProceduralMeshComponent* HeadMesh;

    // Material de la cabeza (arcilla que lee vertex color). Asignar en BP_LobbyCharacter.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head")
    UMaterialInterface* HeadMaterial = nullptr;

    // Copia la escultura de un ProceduralMesh de origen (el volumen donde el jugador esculpió) al
    // HeadMesh del personaje y la GUARDA (SaveGame). Es la "horneada" de v1.
    void SetHeadMeshFrom(UProceduralMeshComponent* Src);

    // Borra la cabeza custom (vuelve a sin cabeza).
    void ClearHeadMesh();

    // Persistencia local (v1): guardar/cargar la cabeza esculpida en un slot de SaveGame.
    void SaveHead();
    void LoadHead();

    // Pose de esculpido: true = recto/quieto (bypass del AnimBP → pose base, sin baile ni jiggle
    // del physics asset) mientras esculpís la cabeza; false = restaura la animación normal.
    void SetSculptPose(bool bEnable);

protected:
    virtual void BeginPlay() override;

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

    // Montage de salto (asignar Jump_AnimMontage). Se reproduce replicado en todos.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Anim")
    UAnimMontage* JumpMontage = nullptr;

private:
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);

    // Salto replicado: cliente → server → multicast a todos.
    UFUNCTION(Server, Reliable)      void Server_PlayJump();
    UFUNCTION(NetMulticast, Reliable) void Multicast_PlayJump();

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

    // ── Cabeza custom: helpers de secciones ─────────────────────────────────
    // Lee las secciones (verts/tris/normales/UV/color) de un ProceduralMesh a structs guardables.
    TArray<FPTHeadSection> ExtractSections(UProceduralMeshComponent* Src) const;
    // Reconstruye el HeadMesh desde secciones (aplica HeadMaterial). Lo usan SetHeadMeshFrom y LoadHead.
    void ApplyHeadSections(const TArray<FPTHeadSection>& Secs);

    // Cartel del nombre: actualizado throttled desde Tick.
    void  UpdateNameTag();
    float NameTagAccum = 0.f;
    float ChatBubbleUntil = 0.f; // tiempo (world) hasta el que se muestra el globo de chat
};
