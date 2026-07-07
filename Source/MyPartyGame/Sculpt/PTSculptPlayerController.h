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

private:
    APTSculptVolume* Volume = nullptr;
    UPROPERTY() class UPTLobbyEscapeMenuWidget* EscapeMenu = nullptr;
    void OnPausePressed();
    void OnOpenChat();

    bool bIsStamping        = false;
    bool bPreviewDirty      = true;
    bool bDiagLogged        = false; // log de diagnóstico una sola vez

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

    // ¿El jugador local puede esculpir ahora? (es el escultor y la fase es Drawing).
    // Sin partida en curso (testeo solo del mapa) devuelve true → esculpir libre.
    bool CanLocalPlayerSculpt() const;

    void OnStampPressed();
    void OnStampReleased();
    void OnScrollUp();
    void OnScrollDown();
    float MinForMode() const { return (EditMode == EPTEditMode::Paint) ? PaintMinSize : MinSize; }
    void  ClampStampSize()   { StampSize = FMath::Clamp(StampSize, MinForMode(), MaxSize); }
    void SetShapeSphere()  { SetShape(EPTStampShape::Sphere);  }
    void SetShapeCube()    { SetShape(EPTStampShape::Cube);    }
    void SetShapeCylinder(){ SetShape(EPTStampShape::Cylinder);}
    void SetShapeTriPrism(){ SetShape(EPTStampShape::TriPrism);}
    void SetShape(EPTStampShape S);
    void CycleModes();
    void OpenColorPicker();
};
