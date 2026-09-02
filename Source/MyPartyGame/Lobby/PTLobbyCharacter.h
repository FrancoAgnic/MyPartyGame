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
class UTextureRenderTarget2D;
class UMaterialInstanceDynamic;
class UTexture2D;
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

    // Material de la cabeza (arcilla). Debe muestrear la textura de pintura "PaintTex" por UV0 igual
    // que el material del cuerpo: Base Color = lerp(color base, PaintTex.RGB, PaintTex.A).
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

    // Hornea la cabeza: arcilla del volumen + ojos + pintura. La APLICA al HeadMesh y devuelve el blob
    // combinado en OutBlob (geometría + centro + PNG cabeza + PNG cuerpo). NO guarda ni replica: de eso
    // se encarga el que llama (el Locker guarda en el slot + equipa + sube).
    void BakeAndReplicateHead(UProceduralMeshComponent* ClaySrc, APTSculptVolume* PaintSource,
                              const TArray<FVector4>& LocalEyes, TArray<uint8>& OutBlob);

    // Encodea la textura de pintura del cuerpo a PNG (para guardarla en un slot de cuerpo).
    bool GetBodyPaintPNG(TArray<uint8>& OutPNG);

    // Renderiza una MINIATURA del personaje (SceneCapture) a PNG. bHeadFocus=true → encuadra la cabeza y
    // el cuerpo se ve default/oscurecido; false → cuerpo entero con una cabeza esfera default/oscurecida.
    bool CaptureLookThumbnailPNG(TArray<uint8>& OutPNG, bool bHeadFocus, int32 Size = 256);
    // Crea una UTexture2D desde un PNG (para mostrar la miniatura en el Locker). Outer = dueño (GC).
    static UTexture2D* MakeTextureFromPNG(UObject* Outer, const TArray<uint8>& PNG);
    // Parámetros de encuadre de la miniatura (frente al personaje).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") float ThumbDistance = 220.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") float ThumbHeight   = 70.f;
    // Distancia de cámara para el encuadre de CABEZA (más cerca que el cuerpo entero).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") float ThumbHeadDistance = 95.f;
    // Encuadre fino: offset vertical del centro y ángulo (pitch) de la cámara, por separado cabeza/cuerpo.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") float ThumbHeadHeight = 0.f;   // sube/baja el centro (cabeza)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") float ThumbHeadPitch  = 0.f;   // inclina la cámara (cabeza)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") float ThumbBodyPitch  = 0.f;   // inclina la cámara (cuerpo)
    // Intensidad de la luz frontal limpia de la miniatura (sin sombras). 0 = sin luz extra.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") float ThumbLightIntensity = 6.f;
    // Color de fondo de la miniatura. Requiere ThumbBackdropMaterial (unlit, con parámetro vector "Color").
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") FLinearColor ThumbBgColor = FLinearColor(0.10f, 0.11f, 0.14f, 1.f);
    // Material del plano de fondo (unlit con param "Color"). Si es null, no se dibuja fondo (como antes).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") UMaterialInterface* ThumbBackdropMaterial = nullptr;
    // Material "apagado/oscuro" para la parte que NO es el foco (cuerpo default en thumb de cabeza; cabeza
    // esfera default en thumb de cuerpo). Si es null, esa parte se muestra con su material normal.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Locker") UMaterialInterface* ThumbDimMaterial = nullptr;
    // Re-arma un blob combinado tomando la CABEZA de HeadBlob pero con OTRO cuerpo (BodyPNG). Sirve
    // para equipar cabeza y cuerpo de slots distintos. Si BodyPNG está vacío, conserva el del HeadBlob.
    void AssembleReplicatedBlob(const TArray<uint8>& HeadBlob, const TArray<uint8>& BodyPNG, TArray<uint8>& Out);

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

    // Look BASE por defecto: una ESFERA blanca en el HeadSocket (para cuando no hay cabeza custom
    // equipada). Así el personaje default no queda sin cabeza. Radio ajustable en BP_LobbyCharacter.
    void ApplyDefaultSphereHead();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head") float DefaultHeadRadius = 20.f;

    // Persistencia local: guardar/cargar la cabeza (blob completo: geometría + texturas de pintura).
    void SaveHeadBlob(const TArray<uint8>& Blob);
    void LoadHead();

    /** Aplica LOCALMENTE (sin commitear el equipado ni replicar a otros) la combinación cabeza/cuerpo
     *  de los slots dados, para previsualizar al pasar el mouse por un slot del locker. Índice inválido
     *  o slot vacío en la cabeza → esfera default. Pasá el índice equipado en la parte que no cambia. */
    void ApplyLookPreview(int32 HeadIdx, int32 BodyIdx);

    // Pose de esculpido: true = recto/quieto (bypass del AnimBP → pose fija, sin baile ni jiggle
    // del physics asset) mientras esculpís la cabeza; false = restaura la animación normal.
    // PoseAnim (opcional) = animación de pose recta a forzar; si es null usa SculptPoseAnim.
    void SetSculptPose(bool bEnable, UAnimationAsset* PoseAnim = nullptr);

    // Flecha que indica hacia dónde mira el personaje (para orientarse al esculpir la cabeza).
    // Se muestra/oculta al entrar/salir del modo G (no se dibuja cuadro a cuadro).
    void SetFacingArrowVisible(bool bVisible);

    // (El envío de la cabeza al servidor vive en APTPlayerState::UploadHead: va troceado por RPC
    //  porque el blob se pasa del tamaño máximo de una propiedad replicada.)

    // Serializa+comprime / descomprime+deserializa las secciones de la cabeza a/desde bytes.
    static void SectionsToBlob(const TArray<FPTHeadSection>& Secs, TArray<uint8>& OutBlob);
    static bool BlobToSections(const TArray<uint8>& Blob, TArray<FPTHeadSection>& OutSecs);

    // Blob COMBINADO (geometría + centro de proyección + texturas PNG de cabeza y cuerpo). Es lo que
    // se replica y se guarda. Backward-compatible: si el blob no tiene el header nuevo, se lee como
    // geometría vieja (solo secciones).
    void BuildHeadBlob(const TArray<FPTHeadSection>& Secs, TArray<uint8>& OutBlob) const;
    bool ParseHeadBlob(const TArray<uint8>& Blob, TArray<FPTHeadSection>& OutSecs,
                       FVector& OutCenter, TArray<uint8>& OutHeadPNG, TArray<uint8>& OutBodyPNG) const;
    // Reconstruye las texturas de pintura desde PNG (para pawns remotos / al cargar del disco).
    void ApplyHeadPaintFromPNG(const TArray<uint8>& PNG, const FVector& CenterLocal);
    void ApplyBodyPaintFromPNG(const TArray<uint8>& PNG);
    // Aplica un blob combinado localmente (geometría + texturas + materiales). Remotos y carga de disco.
    void ApplyHeadBlobLocal(const TArray<uint8>& Blob);

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
    // Reconstruye el HeadMesh desde secciones. Arcilla: usa HeadColorMID (muestrea el atlas 3D, idéntico
    // al vivo) si existe; si no, HeadMaterial plano con vertex color. Ojos: EyeMaterial.
    void ApplyHeadSections(const TArray<FPTHeadSection>& Secs);
    // Material dinámico de la cabeza horneada que muestrea las copias del atlas 3D (lo crea el volumen).
    // Da el mismo resultado EXACTO que el clay en vivo. Null hasta hornear (o en pawns remotos, por ahora).
    UPROPERTY() UMaterialInstanceDynamic* HeadColorMID = nullptr;
    // El pawn local acaba de hornear su cabeza esta sesión → no pisarla al re-aplicar desde el blob.
    bool bLocallyBakedHead = false;
    // Aplica la cabeza del PlayerState si tiene; si no y es el pawn local, carga la guardada (disco).
    void TryApplyReplicatedHead();

    // Última versión de cabeza aplicada a este pawn (se compara con APTPlayerState::HeadVersion
    // en el Tick throttled). -1 = todavía ninguna.
    int32 AppliedHeadVersion = -1;

    // Cartel del nombre: actualizado throttled desde Tick.
    void  UpdateNameTag();
    float NameTagAccum = 0.f;
    float ChatBubbleUntil = 0.f; // tiempo (world) hasta el que se muestra el globo de chat
    bool  bSpectatorHiddenApplied = false; // último estado de ocultamiento por espectador dev aplicado

public:
    // ── SPIKE: pintar la piel del personaje (Render Target por UV) ───────────
    // La pintura se guarda en una textura (RT) que el material del personaje muestrea por UV. Como
    // las UV no cambian con la animación, la pintura queda "pegada" a la piel y se mueve con ella.
    // El material M_SculperCharacter debe tener un Texture Parameter "PaintTex" (default negro) y
    // usarlo así: Base Color = lerp(ColorSólido, PaintTex.RGB, PaintTex.A).

    // Textura de brocha: un círculo suave (blanco con alpha, o blanco sobre negro si usás material).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paint") UTexture2D* PaintBrushTexture = nullptr;
    // Resolución de la textura de pintura del personaje.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paint") int32 PaintRTSize = 1024;
    // Nombre del parámetro de textura en M_SculperCharacter.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paint") FName PaintTexParam = TEXT("PaintTex");
    // Cuánto se "derrama" la pintura hacia afuera de cada isla UV (en téxeles), para tapar el pixel de
    // corte justo sobre la costura. 0 = sin dilatado.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paint") int32 PaintSeamDilation = 4;

    /** Crea la textura de pintura y engancha el material dinámico del personaje (idempotente). */
    UFUNCTION(BlueprintCallable, Category="Paint") void InitCharacterPaint();

    /** Pinta donde apunta el cursor del jugador local (raycast + esfera de mundo). */
    UFUNCTION(BlueprintCallable, Category="Paint")
    void PaintCharacterAtCursor(FLinearColor Color, float BrushPixels = 64.f);

    /** Rayo (Origin+Dir) contra los triángulos skinneados → UV, punto y normal del más cercano.
     *  Lo usa el controller para el preview del anillo y para saber dónde pintar. */
    bool RaycastSkinnedMeshUV(const FVector& Origin, const FVector& Dir,
                              FVector2D& OutUV, FVector& OutPoint, FVector& OutNormal) const;

    /** Pintado SIN COSTURAS: pinta todos los téxeles cuya posición 3D cae dentro de la esfera del
     *  pincel (centro world P, radio R en cm). Como trabaja en el mundo, los dos lados de una costura
     *  UV se pintan juntos → el trazo no se corta nunca. Acumula en un buffer; llamar FlushBodyPaint(). */
    void PaintBodyWorldSphere(const FVector& P, float R, FLinearColor Color);
    /** Sube al GPU lo pintado desde el último flush (una sola actualización por frame). */
    void FlushBodyPaint();

    // Color BASE del cuerpo (lo que se ve donde NO hay pintura). Se aplica al material dinámico del
    // cuerpo por este parámetro Vector. Default BLANCO. Requiere que M_SculperCharacter tenga un
    // parámetro Vector con este nombre enchufado al Base Color (tinte). Si no existe, es un no-op.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paint") FName BodyBaseColorParam = TEXT("Color");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Paint") FLinearColor BodyBaseColor = FLinearColor::White;

    // ── Pintado de la CABEZA con textura 2D (igual que el cuerpo, NO el atlas de vóxeles) ──
    // La cabeza se pinta en una textura 2D de 1024, mapeada por proyección ESFÉRICA desde un centro
    // FIJO (local). Como el mapeo depende solo de la posición, la pintura se preserva aunque re-esculpas
    // (el mismo punto 3D → mismo téxel). El material M_HeadPaint muestrea la textura con esa proyección.

    // Material de la cabeza pintada: muestrea "PaintTex" por proyección esférica desde "Center".
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head") UMaterialInterface* HeadPaintMaterial = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Head") int32 HeadPaintTexSize = 1024;

    /** Crea la textura 2D de pintura de la cabeza (transparente) y fija el centro de proyección. */
    void InitHeadPaint(const FVector& CenterLocal);
    /** MID de HeadPaintMaterial con PaintTex + Center seteados (para la malla viva y la horneada). */
    UMaterialInstanceDynamic* CreateHeadPaintMID();
    /** Pinta (world-space, sin costuras) sobre la textura de la cabeza: téxeles cuyo punto 3D cae en la
     *  esfera del pincel (centro world P, radio R). Mesh = la malla de arcilla (misma escala/espacio). */
    // bErase=true → pone transparente los téxeles cubiertos (borra la pintura) en vez de pintar. Se usa
    // al AGREGAR arcilla: la textura es direccional, así hay que limpiar la pintura vieja de esa zona
    // para que la arcilla nueva muestre su propio color. Borrar por los MISMOS triángulos que pinta
    // garantiza que la región borrada coincide exacto con dónde pintaría (sin aproximaciones).
    void PaintHeadWorldSphere(UProceduralMeshComponent* ClayMesh, const FVector& P, float R, FLinearColor Color, bool bErase = false);
    /** Borra la pintura 2D de la cabeza por DIRECCIÓN (cono desde el centro hacia P). No depende de la
     *  geometría ni del timing de mallado, así que al AGREGAR arcilla nueva limpia la pintura vieja de esa
     *  dirección aunque la malla nueva todavía no exista → sin rastros de téxeles. RefMesh solo aporta el
     *  transform para pasar P a espacio local. */
    void ClearHeadPaintCone(UProceduralMeshComponent* RefMesh, const FVector& P, float R);
    /** Sube al GPU lo pintado en la cabeza desde el último flush. */
    void FlushHeadPaint();
    UTexture2D* GetHeadPaintTex() const { return HeadPaintTex; }

    /** Borra toda la pintura de la CABEZA (textura transparente). */
    void ClearHeadPaint();
    /** Borra toda la pintura del CUERPO (textura transparente). */
    void ClearBodyPaint();

    // Undo de PINTURA (snapshot por trazo). El controller llama Push* al empezar un trazo de Paint y
    // Undo* al deshacer. Acotado a MaxPaintUndo niveles; se limpia al salir del modo G.
    void PushHeadPaintUndo();
    bool UndoHeadPaint();
    void PushBodyPaintUndo();
    bool UndoBodyPaint();
    void ClearPaintUndo() { HeadPaintUndoStack.Reset(); BodyPaintUndoStack.Reset(); }

    // Presupuesto de pintura: peso REAL (PNG) cacheado de cada textura, para limitar cuánto se puede
    // pintar y no pasarse del tamaño replicable. Se recalcula al pintar/deshacer/borrar (no por frame).
    void RecomputeHeadPaintBytes();
    void RecomputeBodyPaintBytes();
    int32 GetPaintPngBytes() const { return CachedHeadPngBytes + CachedBodyPngBytes; }

private:
    // Estado del pintado 2D de la cabeza.
    UPROPERTY() UTexture2D* HeadPaintTex = nullptr;
    TArray<FColor> HeadPaintPixels;
    int32 HeadPaintN = 0;
    FVector HeadPaintCenterLocal = FVector::ZeroVector; // centro fijo de la proyección esférica (local)
    int32 HDirtyMinX = 0, HDirtyMinY = 0, HDirtyMaxX = -1, HDirtyMaxY = -1;

    // Pilas de undo de pintura (copias de los buffers antes de cada trazo). Acotadas.
    TArray<TArray<FColor>> HeadPaintUndoStack;
    TArray<TArray<FColor>> BodyPaintUndoStack;
    static constexpr int32 MaxPaintUndo = 5;

    // Peso PNG cacheado de cada textura de pintura (para el presupuesto).
    int32 CachedHeadPngBytes = 0;
    int32 CachedBodyPngBytes = 0;


    UPROPERTY() UTexture2D*               PaintTex     = nullptr; // textura de pintura del cuerpo
    UPROPERTY() UMaterialInstanceDynamic* CharPaintMID = nullptr;

    // Buffer CPU de la textura de pintura (BGRA, alpha recto). Se pinta acá y se sube por regiones.
    TArray<FColor> PaintPixels;
    int32 PaintTexN = 0;                       // lado de la textura (= PaintRTSize)
    // Rectángulo "sucio" a subir en el próximo FlushBodyPaint.
    int32 DirtyMinX = 0, DirtyMinY = 0, DirtyMaxX = -1, DirtyMaxY = -1;
    void MarkDirty(int32 X, int32 Y);

    // Cache de posiciones skinneadas (mundo) para no recomputarlas por cada estampa dentro del frame.
    TArray<FVector> CachedWorldPos;
    double CachedPosTime = -1.0;
    const class FSkeletalMeshLODRenderData* EnsureSkinnedCache();
};
