#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"
#include "PTSculptVolume.h"
#include "PTSculptPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

// El HUD se suscribe a estos para reaccionar cuando llegan (solo al escultor) las 3
// palabras y la palabra confirmada. Todo el cableado del HUD vive en el propio WBP.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPTOnWordChoices, const TArray<FString>&, Choices);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPTOnSecretWord,  const FString&,         Word);

UCLASS()
class MYPARTYGAME_API APTSculptPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    APTSculptPlayerController();

    // Sonidos de esculpido (loops de herramienta + one-shots). Los assets viven en el GameInstance.
    UPROPERTY(VisibleAnywhere, Category="Sculpt|Sound") class UPTSculptSoundComponent* SculptSounds = nullptr;

    // ── Input ───────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
    UInputMappingContext* MovementMappingContext;

    // ── Stamp ───────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sculpt")
    float StampSize = 160.f;

    UPROPERTY(EditAnywhere, Category="Sculpt")
    float SizeStep = 20.f;

    // Mínimo de brocha: 100 para las tools de geometría, más chico para Paint.
    UPROPERTY(EditAnywhere, Category="Sculpt") float MinSize      = 100.f;
    UPROPERTY(EditAnywhere, Category="Sculpt") float PaintMinSize = 20.f;
    UPROPERTY(EditAnywhere, Category="Sculpt") float MaxSize      = 500.f;

    UPROPERTY(EditAnywhere, Category="Sculpt")
    float AirDepth = 400.f; // distancia brazo cuando no hay superficie

    // ── Materiales ──────────────────────────────────────────────────────────
    /** Decal del brush indicator. */
    UPROPERTY(EditAnywhere, Category="Sculpt")
    UMaterialInterface* BrushDecalMaterial = nullptr;

    /** Sombra falsa proyectada en el piso justo debajo del cursor (da sensación de
     *  profundidad). Asignar un material de DECAL tipo mancha/gradiente radial oscuro. */
    UPROPERTY(EditAnywhere, Category="Sculpt|Shadow")
    UMaterialInterface* ShadowDecalMaterial = nullptr;

    /** Tamaño FIJO de la sombra (radio en UU). No escala con la brocha; se ajusta a mano. */
    UPROPERTY(EditAnywhere, Category="Sculpt|Shadow")
    float ShadowSize = 60.f;

    /** Palito indicador de altura entre el piso y el cursor. Si es null, usa el cilindro
     *  básico del motor. Se estira/achica según la distancia al piso. */
    UPROPERTY(EditAnywhere, Category="Sculpt|Shadow")
    UStaticMesh* HeightStickMesh = nullptr;

    /** Material opcional del palito (si null, usa el material del mesh). */
    UPROPERTY(EditAnywhere, Category="Sculpt|Shadow")
    UMaterialInterface* HeightStickMaterial = nullptr;

    /** Grosor del palito (escala X/Y). */
    UPROPERTY(EditAnywhere, Category="Sculpt|Shadow")
    float HeightStickThickness = 0.08f;

    /** Largo nativo del mesh del palito en Z (cilindro básico del motor = 100). */
    UPROPERTY(EditAnywhere, Category="Sculpt|Shadow")
    float HeightStickMeshLength = 100.f;

    // ── Límite del área de esculpido (grilla tipo chaperone) ─────────────────
    /** Material de la grilla de límite. Debe tener un parámetro Vector "CursorPos"
     *  (posición del cursor en mundo). Two-Sided (se ve desde adentro); la grilla
     *  aparece cerca del cursor. Solo lo ve el escultor. */
    UPROPERTY(EditAnywhere, Category="Sculpt|Boundary")
    UMaterialInterface* BoundaryMaterial = nullptr;

    /** Mesh del box de límite (si null, usa el cubo básico del motor de 100³). */
    UPROPERTY(EditAnywhere, Category="Sculpt|Boundary")
    UStaticMesh* BoundaryBoxMesh = nullptr;

    /** Overlay que resalta al escultor del turno (amarillo). Se pone sobre su mesh en
     *  todos los clientes para que todos sepan quién esculpe. Asignar M_SculptorHighlight. */
    UPROPERTY(EditAnywhere, Category="Game")
    UMaterialInterface* SculptorOverlayMaterial = nullptr;

    /** Material overlay X-ray: se dibuja encima de todos los previews (sin tocar
     *  su material base) y muestra un color sólido donde el preview está tapado
     *  por otros objetos. Translucent + Disable Depth Test. */
    UPROPERTY(EditAnywhere, Category="Sculpt")
    UMaterialInterface* PreviewOverlayMaterial = nullptr;

    /** Meshes de preview del modo PAINT, por shape. Se alinean a la superficie,
     *  escalan con la brocha y toman el color del picker (material con param "Color"). */
    UPROPERTY(EditAnywhere, Category="Sculpt|PaintPreview") UStaticMesh* PaintMeshSphere   = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PaintPreview") UStaticMesh* PaintMeshCube     = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PaintPreview") UStaticMesh* PaintMeshCylinder = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PaintPreview") UStaticMesh* PaintMeshCone     = nullptr;

    /** Mesh de preview del modo SMOOTH (se alinea a la superficie, escala con la brocha). */
    UPROPERTY(EditAnywhere, Category="Sculpt|PaintPreview") UStaticMesh* SmoothRingMesh    = nullptr;

    /** Material semitransparente para la preview de la forma (fallback). */
    UPROPERTY(EditAnywhere, Category="Sculpt")
    UMaterialInterface* PreviewMeshMaterial = nullptr;

    /** Materiales de preview por modo, para diferenciar la tool activa. Si alguno
     *  es null, se usa PreviewMeshMaterial. Asignar en el Blueprint. */
    UPROPERTY(EditAnywhere, Category="Sculpt") UMaterialInterface* PreviewMatAdd    = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt") UMaterialInterface* PreviewMatErase  = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt") UMaterialInterface* PreviewMatSmooth = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt") UMaterialInterface* PreviewMatPaint  = nullptr;

    /** Meshes de preview propios. Si se asignan, reemplazan al mesh procedural.
     *  Prioridad: mesh por tool > mesh por stamp > procedural. Todos opcionales. */
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") float PreviewMeshBaseSize = 100.f; // tamaño nativo del mesh (UU)
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") UStaticMesh* PreviewMeshSphere   = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") UStaticMesh* PreviewMeshCube     = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") UStaticMesh* PreviewMeshCylinder = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") UStaticMesh* PreviewMeshTriPrism = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") UStaticMesh* PreviewToolMeshAdd    = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") UStaticMesh* PreviewToolMeshErase  = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") UStaticMesh* PreviewToolMeshSmooth = nullptr;
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") UStaticMesh* PreviewToolMeshPaint  = nullptr;

    /** Mesh indicador de ejes (se muestra en modo eje con la herramienta Add). */
    UPROPERTY(EditAnywhere, Category="Sculpt|PreviewMesh") UStaticMesh* AxisGizmoMesh = nullptr;

    /** BP del plano de eje CON COLISIÓN (crear BP_SculptPlane derivado de APTSculptPlane, con el
     *  static mesh de la grilla y su colisión —más grande/gruesa que el plano—). El servidor lo
     *  spawnea al activar Z/X: te frena solo a vos (el escultor), a los demás ni los toca. */
    UPROPERTY(EditAnywhere, Category="Sculpt|Axis")
    TSubclassOf<class APTSculptPlane> SculptPlaneClass;

    // ── HUD de la partida ───────────────────────────────────────────────────
    /** Asignar WBP_GameplayHUD (reparentado a UPTGameplayHUDWidget). Se crea solo en BeginPlay. */
    UPROPERTY(EditAnywhere, Category="UI")
    TSubclassOf<class UPTGameplayHUDWidget> GameplayHUDClass;

    // ── Color picker ────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category="UI")
    TSubclassOf<UUserWidget> ColorPickerClass;

    /** Menú de pausa (asignar WBP_LobbyEscapeMenu). Esc maneja la navegación de dos niveles. */
    UPROPERTY(EditAnywhere, Category="UI")
    TSubclassOf<class UPTLobbyEscapeMenuWidget> PauseMenuClass;

    /** Llamado desde el widget BP cuando el usuario confirma un color. */
    UFUNCTION(BlueprintCallable, Category="Sculpt")
    void OnColorConfirmed(FLinearColor NewColor);

    /** Color actual de pintura (leer desde el widget BP). */
    UPROPERTY(BlueprintReadOnly, Category="Sculpt")
    FLinearColor CurrentPaintColor = FLinearColor::White;

    /** Modo activo. Leer desde Blueprint para mostrar HUD. */
    UPROPERTY(BlueprintReadOnly, Category="Sculpt")
    EPTEditMode EditMode = EPTEditMode::Add;

    /** Forma activa. Leer desde Blueprint para mostrar HUD. */
    UPROPERTY(BlueprintReadOnly, Category="Sculpt")
    EPTStampShape StampShape = EPTStampShape::Sphere;

    /** Rotación actual del sello (rueda del mouse mantenida + arrastrar). Doble click de rueda
     *  la resetea. Se aplica al preview y viaja con el stamp (afecta la geometría, no el paint). */
    UPROPERTY(BlueprintReadOnly, Category="Sculpt")
    FRotator StampRotation = FRotator::ZeroRotator;

    /** Grados de rotación por unidad de movimiento del mouse (sensibilidad al rotar shapes). */
    UPROPERTY(EditAnywhere, Category="Sculpt")
    float ShapeRotateSpeed = 3.f;

    /** Forma efectiva: Add, Paint y Erase usan la seleccionada (se borra con cualquier forma,
     *  igual que se esculpe). Solo Smooth queda fijo en esfera. */
    EPTStampShape EffectiveShape() const
    {
        return (EditMode == EPTEditMode::Smooth) ? EPTStampShape::Sphere : StampShape;
    }

    /** true si la herramienta actual usa formas (y por lo tanto el HUD muestra los cuadritos).
     *  Ojos no (siempre esfera) y Smooth tampoco. */
    bool ToolUsesShapes() const
    {
        return !bEyesTool && (EditMode == EPTEditMode::Add
                           || EditMode == EPTEditMode::Erase
                           || EditMode == EPTEditMode::Paint);
    }

    // ── Snapshot de la escultura para los que se (re)conectan tarde ──────────
    // Los sellos viajan por multicast (solo a los conectados), así que quien entra tarde se pierde lo
    // ya esculpido. Al entrar, el server le manda el estado del volumen (geometría base + capas)
    // troceado y el cliente lo aplica. Los ojos ya se replican por propiedad aparte.
    void SendSculptSnapshot(const TArray<uint8>& Blob); // SERVER: arranca el envío (comprimido, por ACK) a este cliente
    UFUNCTION(Client, Reliable)
    void Client_SculptSnapshotChunk(const TArray<uint8>& Data, bool bFirst, bool bLast);
    // CLIENTE→SERVER: "recibí el chunk, mandá el próximo". Control de flujo por ACK: nunca hay más de UN
    // chunk confiable en vuelo → no se desborda el buffer confiable (que cortaba la conexión al reconectar).
    UFUNCTION(Server, Reliable)
    void Server_AckSnapChunk();

    // ── Partida (Sculpturillo) ──────────────────────────────────────────────
    /** El servidor manda las 3 palabras SOLO al escultor. */
    UFUNCTION(Client, Reliable)
    void Client_ReceiveWordChoices(const TArray<FString>& Choices);

    /** El servidor confirma la palabra elegida (solo la ve el escultor). */
    UFUNCTION(Client, Reliable)
    void Client_ReceiveSecretWord(const FString& Word);

    /** El servidor manda una línea de sistema SOLO a este jugador (para textos que dependen de su
     *  idioma, como "la palabra era X": cada uno la recibe con su traducción, no la de todos). */
    UFUNCTION(Client, Reliable)
    void Client_SystemLine(FName Key, const FString& Arg0, int32 Arg1);

    /** Al que adivina: la palabra (en SU idioma) + los puntos que sumó → popup grande. */
    UFUNCTION(Client, Reliable)
    void Client_YouGuessed(const FString& Word, int32 Points);

    /** Al escultor: ya adivinaron todos, el turno se cierra → aviso para que no sea de golpe. */
    UFUNCTION(Client, Reliable)
    void Client_AllGuessed();

    /** El escultor elige una de las 3 (cliente → servidor). Llamar desde el HUD. */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Game")
    void Server_ChooseWord(int32 Index);

    /** Envía un mensaje de chat / intento de adivinanza (cliente → servidor).
     *  El servidor decide si es acierto (anti-spoiler) o mensaje normal. Llamar desde el HUD. */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Game")
    void Server_SendChat(const FString& Message);

    /** Pantalla de fin de partida (solo el anfitrión): volver a jugar o volver al lobby.
     *  El servidor valida que el que lo pide sea el host. Llamar desde el HUD. */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Game")
    void Server_RequestPlayAgain();
    UFUNCTION(Server, Reliable, BlueprintCallable, Category="Game")
    void Server_RequestReturnToLobby();

    /** Aplica un sello: el cliente lo pide al server, que valida el rol y lo difunde.
     *  Reemplaza la llamada directa Volume->Server_ApplyStamp (que no funcionaba desde
     *  clientes: el Volume no tiene owner por jugador, así que su Server RPC se descartaba). */
    UFUNCTION(Server, Reliable)
    void Server_ApplyStamp(FVector WorldPos, EPTStampShape Shape, float Size,
                           EPTEditMode Mode, FLinearColor PaintColor, FRotator StampRot, bool bDetail);

    /** ALT: arranca una nueva CAPA de detalle (malla aparte) en el volumen. Solo el escultor. */
    UFUNCTION(Server, Reliable)
    void Server_BeginDetailLayer();

    /** Coloca un ojo (tecla 4). Igual que el stamp: el cliente lo pide al server (que sí tiene
     *  owner), y el server lo agrega al volumen (se replica a todos via la propiedad Eyes). */
    UFUNCTION(Server, Reliable)
    void Server_AddEye(FVector WorldPos, float Radius);

    /** Borra TODA la escultura (BACKSPACE mantenido). Solo el escultor del turno; el server
     *  valida y difunde el clear a todos (Multicast_ClearAll del volumen). */
    UFUNCTION(Server, Reliable)
    void Server_ClearSculpture();

    /** Deshace la última acción (BACKSPACE toque corto). Solo el escultor; el server difunde. */
    UFUNCTION(Server, Reliable)
    void Server_Undo();

    /** Marcas de trazo para el undo: el server las difunde para que TODOS graben/cierren igual. */
    UFUNCTION(Server, Reliable) void Server_BeginStroke();
    UFUNCTION(Server, Reliable) void Server_EndStroke();

    /** Palabra secreta del turno y las 3 opciones (solo se setean en el cliente del
     *  escultor). El HUD puede leerlas directo, o suscribirse a los delegates de abajo. */
    UPROPERTY(BlueprintReadOnly, Category="Game")
    FString CurrentSecretWord;
    UPROPERTY(BlueprintReadOnly, Category="Game")
    TArray<FString> CurrentWordChoices;

    /** El HUD se suscribe: llegan las 3 palabras (solo al escultor). */
    UPROPERTY(BlueprintAssignable, Category="Game")
    FPTOnWordChoices OnWordChoicesReceived;
    /** El HUD se suscribe: llega la palabra secreta confirmada (solo al escultor). */
    UPROPERTY(BlueprintAssignable, Category="Game")
    FPTOnSecretWord OnSecretWordReceived;

    // ── Cámara espectador dev (trailer/capturas) ─────────────────────────────
    // Comandos de consola (dev-only, todo local). Ver UPTSpectatorComponent.
    UFUNCTION(Exec) void PTSpectate();          // vuelo libre WASD+mouse on/off (+ nombres → Player N)
    UFUNCTION(Exec) void PTHideUI();            // oculta/muestra todo el HUD
    UFUNCTION(Exec) void PTHideNames();         // oculta/muestra los nombres flotantes
    UFUNCTION(Exec) void PTRevealWord();        // alterna la palabra secreta visible/máscara
    UFUNCTION(Exec) void PTHideHotbar();        // oculta/muestra la barra de herramientas
    UFUNCTION(Exec) void PTSpecSpeed(float N);  // multiplica la velocidad de la cámara
    UFUNCTION(Exec) void PTSpecSmooth(float N); // suavizado/lag de la cámara (bajo = más suave)

    // El cliente le avisa al server que entra/sale de espectador (para sacarlo de la partida).
    UFUNCTION(Server, Reliable) void Server_SetSpectator(bool bInSpectator);

protected:
    virtual void BeginPlay()          override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    // Al poseer el pawn, arrancar en modo vuelo (Lvl-01 es un vacío sin piso). Corre en el
    // cliente local para activar el bFlying local del input; el servidor ya lo dejó volando
    // en APTSculptGameMode::StartPawnFlying.
    virtual void AcknowledgePossession(APawn* P) override;

    /** Manda al servidor el idioma elegido en Settings (para ver la palabra en MI idioma).
        Reintenta solo si el PlayerState todavía no replicó. */
    void PushLanguageToServer();

    // Sincronización de cabezas en el Lvl-01: heartbeat gateado que re-pide SOLO lo que falte (anti-flood).
    void HeadSyncHeartbeat();
    FTimerHandle HeadSyncTimer;

public:
    /** true si el menú de pausa (ESC) está abierto. Lo consulta el HUD para no pisar el
     *  cursor con su ApplyInputMode mientras el menú necesita el mouse para la UI. */
    bool IsEscapeMenuOpen() const;

    /** true mientras la rueda de color (mantener RMB) está activa. Igual que arriba: el HUD lo
     *  consulta para mantener el cursor visible y poder elegir color mientras se esculpe. */
    bool IsColorPickerOpen() const { return bQuickColorActive; }

    // ── Estado de las tools, para la barra de herramientas del HUD ───────────
    UFUNCTION(BlueprintPure, Category="Sculpt") bool IsEyesToolActive() const   { return bEyesTool; }
    /** true mientras se mantiene ALT (detalle en capa aparte pegado a la superficie). Para el HUD. */
    UFUNCTION(BlueprintPure, Category="Sculpt") bool IsSurfaceSnapActive() const { return bSurfaceSnap; }
    UFUNCTION(BlueprintPure, Category="Sculpt") bool IsAxisLockActive() const   { return bAxisLock; }
    UFUNCTION(BlueprintPure, Category="Sculpt") bool IsAxisHorizontal() const   { return bAxisHorizontal; }

    /** ¿El punto donde apuntás cae FUERA de la zona de modelado? El HUD muestra el ícono de
     *  "prohibido construir" en el centro cuando esto es true. */
    UFUNCTION(BlueprintPure, Category="Sculpt") bool IsStampOutsideCanvas() const { return bStampOutsideCanvas; }

    /** ¿Se está manteniendo BACKSPACE (borrar todo)? Para el círculo de progreso del HUD. */
    UFUNCTION(BlueprintPure, Category="Sculpt") bool IsClearHeld() const { return bClearHeld; }
    /** Progreso 0..1 del "borrar todo" mientras se mantiene. Devuelve 0 durante la ventana del
     *  toque corto (que es "deshacer"), así el círculo no parpadea en cada undo. */
    UFUNCTION(BlueprintPure, Category="Sculpt")
    float GetClearHoldProgress() const
    {
        if (!bClearHeld || ClearHoldTime <= UndoTapMaxTime) return 0.f;
        return FMath::Clamp((ClearHoldTime - UndoTapMaxTime) / (ClearHoldDuration - UndoTapMaxTime), 0.f, 1.f);
    }
    /** Segundos que faltan para que se borre todo (cuenta regresiva del cuadrito). */
    UFUNCTION(BlueprintPure, Category="Sculpt")
    float GetClearHoldRemaining() const
    { return bClearHeld ? FMath::Max(0.f, ClearHoldDuration - ClearHoldTime) : ClearHoldDuration; }

private:
    // UPROPERTY: si no, al destruirse el SculptVolume (p.ej. seamless travel de vuelta al lobby) el
    // puntero queda COLGADO (dangling) y los chequeos "Volume ?" pasan sobre memoria liberada →
    // crash en GetStampPoint/ClampInsideCanvas durante el tick del travel. Con UPROPERTY el GC lo
    // pone en null al destruirse el actor.
    UPROPERTY()
    APTSculptVolume* Volume = nullptr;
    UPROPERTY() class UPTLobbyEscapeMenuWidget* EscapeMenu = nullptr;
    void OnPausePressed();
    void OnOpenChat();

    // Snapshot de la escultura (envío troceado server→cliente al (re)conectar).
    TArray<uint8> SnapOut;              // SERVER: bytes COMPRIMIDOS pendientes de enviar (con int32 tamaño crudo al inicio)
    int32         SnapSent = 0;         // SERVER: cuántos bytes ya se mandaron
    void          SendNextSnapChunk();  // SERVER: manda UN chunk y espera el ACK del cliente
    TArray<uint8> SnapIn;               // CLIENTE: bytes comprimidos recibidos (se ensamblan hasta bLast)

    bool bIsStamping        = false;
    // El cursor apunta fuera del lienzo (BoundsBox del volumen) → no se puede construir ahí.
    bool bStampOutsideCanvas = false;
    bool bPreviewDirty      = true;
    bool bMovementCtxReady  = false; // mapping context de movimiento ya agregado
    bool bMenuInputLocked   = false; // look/move pausados por el menú de pausa (transición)
    // look/move pausados mientras hay una UI de partida que necesita el foco (elección de palabra
    // del escultor / scoreboard de fin de partida). Sin esto, mover la cámara le sacaba el foco a
    // esos paneles. Transición aparte para no desbalancear el contador de IgnoreLook/MoveInput.
    bool bGamePhaseInputLocked = false;

    // Interpolación de trazo (sellos continuos entre frames).
    bool    bStrokeActive = false;
    FVector LastStampPos  = FVector::ZeroVector;

    // Plano de esculpido bloqueado al inicio del trazo (evita que el stamp se acerque a la cámara).
    // Se usa en ALT+Add: al primer sello se congela un plano perpendicular a la vista y el resto del
    // trazo se dibuja sobre él (profundidad constante) → trazos laterales/verticales sin trepar.
    FVector SculptPlaneOrigin = FVector::ZeroVector;
    FVector SculptPlaneNormal = FVector::ForwardVector;
    bool    bStrokePlaneLocked = false;

    EPTStampShape CachedPreviewShape = EPTStampShape::Sphere;
    float         CachedPreviewSize  = 0.f;
    EPTEditMode   CachedPreviewMode  = EPTEditMode::Add;
    FLinearColor  CachedPreviewColor = FLinearColor::White;

    UPROPERTY() AActor*                   PreviewActor      = nullptr;
    UPROPERTY() UProceduralMeshComponent* PreviewMesh       = nullptr;
    UPROPERTY() UStaticMeshComponent*     PreviewStaticMesh = nullptr;
    // MIDs del preview tintado (Add/Paint): se guardan para poder subirles el brillo mientras
    // esculpís (Add) sin reconstruir el mesh cada frame.
    UPROPERTY() class UMaterialInstanceDynamic* PreviewMID       = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* PreviewStaticMID = nullptr;
    /** Multiplicador de brillo del preview mientras mantenés el sello en Agregar. */
    UPROPERTY(EditAnywhere, Category="Sculpt") float PreviewSculptBrightness = 2.f;
    // Cuánto brillo tiene el preview AHORA (0..1). Sube al apretar, baja suave al soltar (fade).
    float PreviewGlowAmt = 0.f;
    UPROPERTY() UStaticMeshComponent*     AxisGizmo         = nullptr;
    UPROPERTY() UStaticMeshComponent*     PaintRing         = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* PaintRingMID = nullptr;
    UPROPERTY() UStaticMesh*              CachedRingMesh    = nullptr;
    // Si el preview de superficie está tintado (Paint) o no (Smooth/Ojos). Sin esto, al pasar de
    // Paint a Ojos con el mismo mesh el MID tintado quedaba puesto y el ojo salía del color de Paint.
    bool CachedRingTint = false;
    UPROPERTY() UUserWidget*              ColorPicker       = nullptr;
    UPROPERTY() class UPTGameplayHUDWidget* GameplayHUD     = nullptr;
    UPROPERTY() class UPTSpectatorComponent* Spectator      = nullptr;
    bool bHudHidden = false;
    UPROPERTY() class UDecalComponent*    ShadowDecal       = nullptr;
    UPROPERTY() class UStaticMeshComponent* HeightStick     = nullptr;
    UPROPERTY() class UStaticMeshComponent* BoundaryMesh    = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* BoundaryMID = nullptr;

    void RebuildPreviewMesh();
    void UpdatePreviewVisual();  // elige mesh (tool/stamp/procedural) y material
    void ApplyPreviewMaterial(); // aplica el material según EditMode
    // Overlay X-ray (M_PreviewXray, el contorno al solaparse con la arcilla): se apaga mientras se
    // agrega arcilla (Add + click apretado) y se repone al soltar. Guardado en bXrayOverlayOn para no
    // re-setear el overlay cada frame.
    void SetPreviewXrayEnabled(bool bOn);
    bool bXrayOverlayOn = true;
    void UpdatePreviewBrightness(float Dt); // brillo del preview (sube al esculpir, fade al soltar)
    bool    GetCameraRay(FVector& Start, FVector& Dir) const;
    FVector GetStampPoint(FVector& OutNormal) const;
    float   VoxelHint() const;

    // ── Modo eje: trazos rectos sobre un plano CONGELADO (sin Shift) ─────────
    // El plano se fija al activar el modo (tecla) y NO se recalcula: podés soltar/re-presionar el
    // esculpido y sigue en el mismo plano. Mientras está activo: rotación de cámara bloqueada y
    // movimiento restringido a A/D (strafe paralelo al plano) para no cruzar/rodear el plano.
    //  - Z (bAxisHorizontal=false): plano VERTICAL (contiene el eje Z) → trazos verticales.
    //  - X (bAxisHorizontal=true):  plano HORIZONTAL (normal = Z)      → trazos horizontales.
    bool            bAxisLock       = false;
    bool            bAxisHorizontal = false;
    FVector         AxisOrigin = FVector::ZeroVector;
    FVector         AxisPlaneN = FVector::ForwardVector;
    FVector         AxisU      = FVector::RightVector;
    FVector         AxisV      = FVector::UpVector;
    mutable int32   AxisChosen = -1; // -1 sin definir, 0=U, 1=V
    // Ejes = HOLD: mantener la tecla activa el plano; soltarla vuelve a Add. (Z = vertical, X = horizontal)
    void OnAxisVerticalPressed();
    void OnAxisVerticalReleased();
    void OnAxisHorizontalPressed();
    void OnAxisHorizontalReleased();
    void SetAxisMode(bool bEnable, bool bHorizontal); // fija/limpia el plano

    // El plano con colisión lo spawnea el SERVIDOR (el movimiento es autoritativo: si viviera solo
    // en el cliente, el server lo corregiría atravesándolo). El cliente lo pide por este RPC.
    UFUNCTION(Server, Reliable)
    void Server_SetSculptPlane(bool bEnable, FVector Origin, FVector Normal);

    UPROPERTY() class APTSculptPlane* ActivePlane = nullptr; // solo válido en el servidor

    // Color rápido (Parte B): mantener RMB abre la rueda; arrastrar elige; soltar confirma.
    bool bQuickColorActive = false;
    void OnColorPickPressed();
    void OnColorPickReleased();
    void OnColorSavePressed();  // E con la rueda abierta: guarda el color en la paleta

    // ¿El jugador local puede esculpir ahora? (es el escultor y la fase es Drawing).
    // Sin partida en curso (testeo solo del mapa) devuelve true → esculpir libre.
    bool CanLocalPlayerSculpt() const;

    // Pone/saca el overlay amarillo en el mesh de cada jugador según quién esculpe.
    void UpdateSculptorHighlights();
    TWeakObjectPtr<class APlayerState> LastSculptorHighlight;

    void OnStampPressed();
    void OnStampReleased();
    void OnScrollUp();
    void OnScrollDown();

    // ── ALT: pegar el sello a la superficie de la arcilla (solo Add) para detallar de cerca ──
    // Con Alt mantenido, en Add el sello deja de ir al "brazo extendido" y se pega a la superficie de
    // la malla existente (raymarch), como Paint/Smooth. Sirve para agregar detalle fino sobre la arcilla.
    bool bSurfaceSnap = false;
    // Z (eje vertical), X (eje horizontal) y Alt (snap a superficie) son 3 holds MUTUAMENTE EXCLUYENTES:
    // la última tecla apretada "quema" a la anterior (usarlas juntas causaba bugs raros, p.ej. la arcilla
    // escalando hacia la cámara). Ver OnSurfaceSnapPressed / OnAxis*Pressed.
    void OnSurfaceSnapPressed();
    void OnSurfaceSnapReleased() { bSurfaceSnap = false; }
    // Latcheado al iniciar el trazo (= Add + Alt en ese momento): mientras dura el trazo, sus sellos
    // van a la capa de detalle y usan el plano congelado, aunque sueltes Alt a mitad de camino.
    bool bStrokeIsDetail = false;

    // ── Rotar shapes (mantener la RUEDA del mouse + arrastrar) ──────────────
    // Mientras se mantiene, la cámara NO se mueve: el mouse rota el sello. Doble click de
    // rueda = volver a la rotación default (sin rotar).
    bool  bRotatingShape   = false;
    float LastWheelPressTime = -10.f;
    static constexpr float WheelDoubleClickWindow = 0.30f;
    void OnShapeRotatePressed();
    void OnShapeRotateReleased();

    // ── Borrar TODO (mantener BACKSPACE 3s) ────────────────────────────────
    // Para empezar de cero rápido sin gastar el turno borrando con Erase. Se ignora mientras
    // el chat está abierto (ahí BACKSPACE es para escribir).
    bool  bClearHeld    = false;
    float ClearHoldTime = 0.f;
    static constexpr float ClearHoldDuration = 3.0f;
    // Toque corto (soltar antes de esto) = deshacer la última acción, en vez de borrar todo.
    static constexpr float UndoTapMaxTime = 0.35f;
    void OnClearAllPressed();
    void OnClearAllReleased();
    float MinForMode() const { return (EditMode == EPTEditMode::Paint) ? PaintMinSize : MinSize; }
    void  ClampStampSize()   { StampSize = FMath::Clamp(StampSize, MinForMode(), MaxSize); }
    void SetShape(EPTStampShape S);
    void CycleShapes();                                    // Tab: cicla Sphere→Cube→Cylinder→TriPrism
    void SetMode(EPTEditMode M);                           // 1/2/3: Add/Erase/Paint
    // bResetAxis=false lo usa el modo eje al auto-equipar Add (si no, se apagaría a sí mismo).
    void SetModeInternal(EPTEditMode M, bool bResetAxis);
    void SetModeAdd()   { SetMode(EPTEditMode::Add);   }
    void SetModeErase() { SetMode(EPTEditMode::Erase); }
    void SetModePaint() { SetMode(EPTEditMode::Paint); }

    // ── Ojos (tecla 4): igual que en modo G del lobby, pero replicado en el volumen ──
    bool bEyesTool = false;
    void SetModeEyes();   // tecla 4: activar la herramienta de ojos
    void PlaceEyeAtCursor(); // coloca un ojo (sobre la superficie) → Volume->Server_AddEye
};
