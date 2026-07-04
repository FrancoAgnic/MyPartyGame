#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ProceduralMeshComponent.h"
#include "PTSculptVolume.h"
#include "PTSculptPlayerController.generated.h"

class UInputMappingContext;
class UUserWidget;

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

protected:
    virtual void BeginPlay()          override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;

private:
    APTSculptVolume* Volume = nullptr;
    UPROPERTY() class UPTLobbyEscapeMenuWidget* EscapeMenu = nullptr;
    void OnPausePressed();

    bool bIsStamping        = false;
    bool bPreviewDirty      = true;

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
