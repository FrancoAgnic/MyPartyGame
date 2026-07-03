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

    UPROPERTY(EditAnywhere, Category="Sculpt")
    float AirDepth = 400.f; // distancia brazo cuando no hay superficie

    // ── Materiales ──────────────────────────────────────────────────────────
    /** Decal del brush indicator. */
    UPROPERTY(EditAnywhere, Category="Sculpt")
    UMaterialInterface* BrushDecalMaterial = nullptr;

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

    // ── Color picker ────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category="UI")
    TSubclassOf<UUserWidget> ColorPickerClass;

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

    // ── Cámara ──────────────────────────────────────────────────────────────
    UPROPERTY(EditAnywhere, Category="Camera")
    bool bInvertPitch = true;

protected:
    virtual void BeginPlay()          override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void AddPitchInput(float Val)    override;

private:
    APTSculptVolume* Volume = nullptr;
    bool bIsStamping        = false;
    bool bPreviewDirty      = true;

    // Plano de esculpido bloqueado al inicio del trazo (evita que el stamp se acerque a la cámara)
    FVector SculptPlaneOrigin = FVector::ZeroVector;
    FVector SculptPlaneNormal = FVector::ForwardVector;

    EPTStampShape CachedPreviewShape = EPTStampShape::Sphere;
    float         CachedPreviewSize  = 0.f;
    EPTEditMode   CachedPreviewMode  = EPTEditMode::Add;

    UPROPERTY() AActor*                   PreviewActor      = nullptr;
    UPROPERTY() UProceduralMeshComponent* PreviewMesh       = nullptr;
    UPROPERTY() UStaticMeshComponent*     PreviewStaticMesh = nullptr;
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
    void SetShapeSphere()  { SetShape(EPTStampShape::Sphere);  }
    void SetShapeCube()    { SetShape(EPTStampShape::Cube);    }
    void SetShapeCylinder(){ SetShape(EPTStampShape::Cylinder);}
    void SetShapeTriPrism(){ SetShape(EPTStampShape::TriPrism);}
    void SetShape(EPTStampShape S);
    void CycleModes();
    void OpenColorPicker();
};
