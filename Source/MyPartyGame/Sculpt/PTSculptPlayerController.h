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

    /** Material semitransparente para la preview de la forma. */
    UPROPERTY(EditAnywhere, Category="Sculpt")
    UMaterialInterface* PreviewMeshMaterial = nullptr;

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

    UPROPERTY() AActor*                   PreviewActor = nullptr;
    UPROPERTY() UProceduralMeshComponent* PreviewMesh  = nullptr;
    UPROPERTY() UUserWidget*              ColorPicker  = nullptr;

    void RebuildPreviewMesh();
    FVector GetStampPoint(FVector& OutNormal) const;

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
