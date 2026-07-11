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

    /** Forma efectiva: Add/Paint usan la seleccionada; Erase/Smooth siempre esfera. */
    EPTStampShape EffectiveShape() const
    {
        return (EditMode == EPTEditMode::Add || EditMode == EPTEditMode::Paint)
             ? StampShape : EPTStampShape::Sphere;
    }

    // ── Partida (Sculpturillo) ──────────────────────────────────────────────
    /** El servidor manda las 3 palabras SOLO al escultor. */
    UFUNCTION(Client, Reliable)
    void Client_ReceiveWordChoices(const TArray<FString>& Choices);

    /** El servidor confirma la palabra elegida (solo la ve el escultor). */
    UFUNCTION(Client, Reliable)
    void Client_ReceiveSecretWord(const FString& Word);

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
                           EPTEditMode Mode, FLinearColor PaintColor);

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

protected:
    virtual void BeginPlay()          override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    // Al poseer el pawn, arrancar en modo vuelo (Lvl-01 es un vacío sin piso). Corre en el
    // cliente local para activar el bFlying local del input; el servidor ya lo dejó volando
    // en APTSculptGameMode::StartPawnFlying.
    virtual void AcknowledgePossession(APawn* P) override;

public:
    /** true si el menú de pausa (ESC) está abierto. Lo consulta el HUD para no pisar el
     *  cursor con su ApplyInputMode mientras el menú necesita el mouse para la UI. */
    bool IsEscapeMenuOpen() const;

private:
    APTSculptVolume* Volume = nullptr;
    UPROPERTY() class UPTLobbyEscapeMenuWidget* EscapeMenu = nullptr;
    void OnPausePressed();
    void OnOpenChat();

    bool bIsStamping        = false;
    bool bPreviewDirty      = true;
    bool bMovementCtxReady  = false; // mapping context de movimiento ya agregado
    bool bMenuInputLocked   = false; // look/move pausados por el menú de pausa (transición)

    // Interpolación de trazo (sellos continuos entre frames).
    bool    bStrokeActive = false;
    FVector LastStampPos  = FVector::ZeroVector;

    // Plano de esculpido bloqueado al inicio del trazo (evita que el stamp se acerque a la cámara)
    FVector SculptPlaneOrigin = FVector::ZeroVector;
    FVector SculptPlaneNormal = FVector::ForwardVector;

    EPTStampShape CachedPreviewShape = EPTStampShape::Sphere;
    float         CachedPreviewSize  = 0.f;
    EPTEditMode   CachedPreviewMode  = EPTEditMode::Add;
    FLinearColor  CachedPreviewColor = FLinearColor::White;

    UPROPERTY() AActor*                   PreviewActor      = nullptr;
    UPROPERTY() UProceduralMeshComponent* PreviewMesh       = nullptr;
    UPROPERTY() UStaticMeshComponent*     PreviewStaticMesh = nullptr;
    UPROPERTY() UStaticMeshComponent*     AxisGizmo         = nullptr;
    UPROPERTY() UStaticMeshComponent*     PaintRing         = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* PaintRingMID = nullptr;
    UPROPERTY() UStaticMesh*              CachedRingMesh    = nullptr;
    UPROPERTY() UUserWidget*              ColorPicker       = nullptr;
    UPROPERTY() class UPTGameplayHUDWidget* GameplayHUD     = nullptr;
    UPROPERTY() class UDecalComponent*    ShadowDecal       = nullptr;
    UPROPERTY() class UStaticMeshComponent* HeightStick     = nullptr;
    UPROPERTY() class UStaticMeshComponent* BoundaryMesh    = nullptr;
    UPROPERTY() class UMaterialInstanceDynamic* BoundaryMID = nullptr;

    void RebuildPreviewMesh();
    void UpdatePreviewVisual();  // elige mesh (tool/stamp/procedural) y material
    void ApplyPreviewMaterial(); // aplica el material según EditMode
    bool    GetCameraRay(FVector& Start, FVector& Dir) const;
    FVector GetStampPoint(FVector& OutNormal) const;
    float   VoxelHint() const;

    // ── Modo eje (tecla X): trazos rectos sin curvatura de cámara ───────────
    bool            bAxisLock = false;
    FVector         AxisOrigin = FVector::ZeroVector;
    FVector         AxisPlaneN = FVector::ForwardVector;
    FVector         AxisU      = FVector::RightVector;
    FVector         AxisV      = FVector::UpVector;
    mutable int32   AxisChosen = -1; // -1 sin definir, 0=U, 1=V
    void ToggleAxisLock();

    // Congelar plano (Parte D): con modo eje ON, mantener LeftShift fija el plano actual
    // (no se recalcula desde la cámara) y bloquea caminar (la cámara sigue libre).
    bool bPlaneFrozen = false;
    void OnFreezePressed();
    void OnFreezeReleased();

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
    float MinForMode() const { return (EditMode == EPTEditMode::Paint) ? PaintMinSize : MinSize; }
    void  ClampStampSize()   { StampSize = FMath::Clamp(StampSize, MinForMode(), MaxSize); }
    void SetShape(EPTStampShape S);
    void CycleShapes();                                    // Tab: cicla Sphere→Cube→Cylinder→TriPrism
    void SetMode(EPTEditMode M);                           // 1/2/3: Add/Erase/Paint
    void SetModeAdd()   { SetMode(EPTEditMode::Add);   }
    void SetModeErase() { SetMode(EPTEditMode::Erase); }
    void SetModePaint() { SetMode(EPTEditMode::Paint); }
};
