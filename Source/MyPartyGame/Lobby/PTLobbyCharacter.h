// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — Personaje del lobby con movimiento replicado (Enhanced Input).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "PTHeadSaveGame.h"
#include "PTLobbyCharacter.generated.h"

class UMaterialInterface;
class UAnimationAsset;
class UArrowComponent;
class UStaticMesh;
class APTSculptVolume;

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

    // true = vuelo OBLIGATORIO: no se puede caminar ni apagarlo con el doble-espacio. Lo activa
    // el gameplay (Lvl-01), donde caminar se sentía con lag y el nivel no tiene piso.
    bool bForceFlying = false;

    // Configura el pawn para el GAMEPLAY (Lvl-01), a diferencia del lobby:
    //  - vuelo obligatorio (el nivel es un vacío sin piso y caminar se sentía con lag),
    //  - SIN orientar la rotación al movimiento (molesta al esculpir; en el lobby queda lindo).
    // Se llama en el servidor (APTSculptGameMode::StartPawnFlying) y en el cliente local
    // (APTSculptPlayerController::AcknowledgePossession), porque el input corre en el cliente.
    void ApplyGameplayMovementMode();


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

    // Flecha de "hacia dónde mira" (visible sólo en modo esculpir-cabeza). Ajustá su transform
    // (altura/largo) y color en BP_LobbyCharacter; C++ sólo la muestra/oculta.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Head")
    UArrowComponent* FacingArrow;

    // Material de la cabeza (arcilla que lee vertex color). Asignar en BP_LobbyCharacter.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head")
    UMaterialInterface* HeadMaterial = nullptr;

    // Material de los OJOS. Asignar el material de "ojos locos". Si es null usa HeadMaterial.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head")
    UMaterialInterface* EyeMaterial = nullptr;

    // Mesh del ojo que se hornea en la cabeza (si tiene "Allow CPU Access"). Si es null, esferas.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head")
    UStaticMesh* HeadEyeMesh = nullptr;

    // Radio "natural" del HeadEyeMesh (para escalarlo al tamaño del ojo colocado).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head")
    float HeadEyeBaseSize = 50.f;

    // Pose de esculpido: animación quieta (idealmente 1 frame) que se fuerza mientras esculpís la
    // cabeza para que el personaje quede TOTALMENTE inmóvil (sin el loop de salto). Asignar en BP.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head")
    UAnimationAsset* SculptPoseAnim = nullptr;

    // Hueso del physics asset cuyo cuerpo se escala para la colisión de la cabeza.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head")
    FName HeadPhysicsBone = TEXT("Bone_008_end");

    // Radio "de referencia" del cuerpo físico de la cabeza (escala 1). El cuerpo se escala por
    // radioDeLaMalla / este valor. Subilo/bajalo hasta que la colisión calce con la cabeza.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head")
    float HeadCollisionRefSize = 50.f;

    // Hornea la cabeza final: arcilla del volumen ClaySrc (con la pintura del atlas de PaintSource)
    // + los OJOS (esferas en posiciones locales LocalEyes: XYZ=centro, W=radio). La aplica al
    // HeadMesh, la guarda localmente y la REPLICA (via PlayerState) para que todos la vean.
    void BakeAndReplicateHead(UProceduralMeshComponent* ClaySrc, APTSculptVolume* PaintSource,
                              const TArray<FVector4>& LocalEyes);

    // Aplica al HeadMesh la cabeza replicada guardada en el PlayerState (para los demás jugadores
    // y al viajar a Lvl-01). No hace nada si el PlayerState no tiene cabeza.
    void ApplyReplicatedHead();

    // Construye una sección de OJOS desde posiciones locales (XYZ=centro, W=radio). Si EyeMesh
    // tiene datos CPU, copia su geometría por cada ojo (escalada a BaseSize); si no, esferas UV.
    static FPTHeadSection BuildEyesSection(const TArray<FVector4>& LocalEyes,
                                           UStaticMesh* EyeMesh = nullptr, float BaseSize = 50.f, int32 Segments = 12);

    // Ajusta la colisión de la cabeza (cuerpo físico del hueso HeadPhysicsBone) al tamaño de la
    // malla esculpida, para que la cabeza "choque" y no atraviese paredes/techos.
    void UpdateHeadCollision();

    // Borra la cabeza custom (vuelve a sin cabeza).
    void ClearHeadMesh();

    // Persistencia local: guardar (secciones dadas) / cargar la cabeza esculpida en un SaveGame.
    void SaveHead(const TArray<FPTHeadSection>& Secs);
    void LoadHead();

    // Pose de esculpido: true = recto/quieto (bypass del AnimBP → pose fija, sin baile ni jiggle
    // del physics asset) mientras esculpís la cabeza; false = restaura la animación normal.
    // PoseAnim (opcional) = animación de pose recta a forzar; si es null usa SculptPoseAnim.
    void SetSculptPose(bool bEnable, UAnimationAsset* PoseAnim = nullptr);

    // Flecha que indica hacia dónde mira el personaje (para orientarse al esculpir la cabeza).
    // Se muestra/oculta al entrar/salir del modo G (no se dibuja cuadro a cuadro).
    void SetFacingArrowVisible(bool bVisible);

    // Envía al servidor la cabeza horneada (comprimida) → la guarda en el PlayerState → se replica.
    UFUNCTION(Server, Reliable)
    void Server_SetHeadBlob(const TArray<uint8>& Blob);

    // Serializa+comprime / descomprime+deserializa las secciones de la cabeza a/desde bytes.
    static void SectionsToBlob(const TArray<FPTHeadSection>& Secs, TArray<uint8>& OutBlob);
    static bool BlobToSections(const TArray<uint8>& Blob, TArray<FPTHeadSection>& OutSecs);

protected:
    virtual void BeginPlay() override;
    // Cuando el pawn recibe su PlayerState (clientes), intentar aplicar la cabeza replicada.
    virtual void OnRep_PlayerState() override;

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

    // Cuánto queda el globo de chat sobre la cabeza (segundos). Ajustable en BP_LobbyCharacter.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="NameTag")
    float ChatBubbleDuration = 4.5f;

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
    TArray<FPTHeadSection> ExtractSections(UProceduralMeshComponent* Src, const APTSculptVolume* PaintSource = nullptr) const;
    // Reconstruye el HeadMesh desde secciones (HeadMaterial en arcilla, EyeMaterial en ojos).
    void ApplyHeadSections(const TArray<FPTHeadSection>& Secs);
    // Aplica la cabeza del PlayerState si tiene; si no y es el pawn local, carga la guardada (disco).
    void TryApplyReplicatedHead();

    // Última versión de cabeza aplicada a este pawn (se compara con APTPlayerState::HeadVersion
    // en el Tick throttled). -1 = todavía ninguna.
    int32 AppliedHeadVersion = -1;

    // Cartel del nombre: actualizado throttled desde Tick.
    void  UpdateNameTag();
    float NameTagAccum = 0.f;
    float ChatBubbleUntil = 0.f; // tiempo (world) hasta el que se muestra el globo de chat
};
