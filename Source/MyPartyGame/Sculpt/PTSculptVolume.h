#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
#include "PTSculptField.h"
#include "PTSculptVolume.generated.h"

class UTexture2D;
class UMaterialInstanceDynamic;

UENUM(BlueprintType)
enum class EPTStampShape : uint8 { Sphere, Cube, Cylinder, TriPrism };

UENUM(BlueprintType)
enum class EPTEditMode : uint8 { Add, Erase, Paint, Smooth };

UCLASS()
class MYPARTYGAME_API APTSculptVolume : public AActor
{
    GENERATED_BODY()
public:
    APTSculptVolume();

    // Resolución del campo (tamaño de celda en UU). Menor = más geometría/detalle.
    UPROPERTY(EditAnywhere, Category="Sculpt") float VoxelSize = 5.f;
    UPROPERTY(EditAnywhere, Category="Sculpt") UMaterialInterface* ClayMaterial = nullptr;

    // ── Modo Smooth (tuneables) ─────────────────────────────────────────────
    // Intensidad del suavizado por aplicación (0..1). Más alto = suaviza más rápido.
    UPROPERTY(EditAnywhere, Category="Sculpt|Smooth", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SmoothStrength = 0.35f;
    // Empuje por aplicación: >0 infla (crece), <0 encoge, 0 = neutro (recomendado).
    UPROPERTY(EditAnywhere, Category="Sculpt|Smooth", meta=(ClampMin="-0.2", ClampMax="0.2"))
    float SmoothBias = 0.f;
    // Tope de cambio por celda por aplicación (evita que se dispare al mantener).
    UPROPERTY(EditAnywhere, Category="Sculpt|Smooth", meta=(ClampMin="0.0", ClampMax="1.0"))
    float SmoothMaxDelta = 0.15f;

    // Suavizado visual de la malla al generarla (0 = off, ~0.5 = suave). No borra detalle guardado.
    UPROPERTY(EditAnywhere, Category="Sculpt", meta=(ClampMin="0.0", ClampMax="1.0"))
    float DisplaySmoothing = 0.5f;

    // Pintura por volumen 3D (per-pixel, independiente del VoxelSize).
    UPROPERTY(EditAnywhere, Category="Sculpt|Paint", meta=(ClampMin="32", ClampMax="128"))
    int32 PaintResolution = 128;

    // Dureza del borde del pincel: 0 = suave (degradé), 1 = duro (borde nítido).
    UPROPERTY(EditAnywhere, Category="Sculpt|Paint", meta=(ClampMin="0.0", ClampMax="1.0"))
    float PaintHardness = 0.6f;

    void ApplyStamp(FVector WorldPos, EPTStampShape Shape, float Size,
                    EPTEditMode Mode, FLinearColor PaintColor);

    float        SampleWorldDensity(FVector WorldPos) const;
    FLinearColor SampleWorldColor  (FVector WorldPos) const;

    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ApplyStamp(FVector WorldPos, EPTStampShape Shape, float Size,
                           EPTEditMode Mode, FLinearColor PaintColor);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ApplyStamp(FVector WorldPos, EPTStampShape Shape, float Size,
                              EPTEditMode Mode, FLinearColor PaintColor);

    // Preview de la forma del sello (malla fantasma que sigue al cursor).
    static void BuildStampPreview(EPTStampShape Shape, float Size, float VoxSz,
                                  TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                                  TArray<FVector>& OutNormals);

    // SDF de cada forma, centrado en origen. HalfSize en unidades de celda.
    static float StampSDF(EPTStampShape Shape, FVector LocalPos, float HalfSize);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    UPROPERTY(VisibleAnywhere) UProceduralMeshComponent* Mesh;
    UPROPERTY(VisibleAnywhere) UBoxComponent* BoundsBox;

    FPTSculptField Field;

    bool  bRebuildInProgress = false;
    float TimeSinceRebuild   = 0.f;
    static constexpr float RebuildInterval = 0.05f;

    void RebuildDirty();
    void MarkStampDirty(int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1);

    // ── Pintura por volumen ─────────────────────────────────────────────────
    UPROPERTY(Transient) UTexture2D* PaintTexture = nullptr;
    UPROPERTY(Transient) UMaterialInstanceDynamic* ClayMID = nullptr;
    TArray<FColor> PaintVolume;          // PaintResolution³, RGBA (A = cobertura)
    bool  bPaintDirty          = false;
    float TimeSincePaintUpload = 0.f;
    int32 PaintDirtyZMin       = INT32_MAX; // rango de slices Z pintados (subida parcial)
    int32 PaintDirtyZMax       = -1;
    static constexpr float PaintUploadInterval = 0.1f;
    FVector CanvasMinLocal  = FVector::ZeroVector; // esquina min del lienzo (UU local)
    FVector CanvasSizeLocal = FVector(960.f);       // tamaño del lienzo (UU local)

    void InitPaintVolume();
    void SetupClayMID();
    void WritePaintStamp(FVector WorldPos, EPTStampShape Shape, float Size, FLinearColor Color, bool bFull = false);
    void UploadPaintTexture();

    // Coordenadas: mundo → celda (float) en espacio local del actor.
    FVector WorldToCell(FVector W) const;
    // Rango de celdas del lienzo (definido por el BoundsBox).
    void    CellBounds(FIntVector& OutMin, FIntVector& OutMax) const;

    // ── Preview (marching cubes sobre un grid chico aislado) ────────────────
    static void RunMarchingCubes(const TArray<float>& G, const TArray<FLinearColor>& CG,
                                 int32 GS, float VoxSz,
                                 int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1,
                                 TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                                 TArray<FVector>& OutNormals, TArray<FColor>& OutColors);
    static FVector Interp     (FVector P1, FVector P2, float V1, float V2);
    static FColor  InterpColor(FLinearColor C1, FLinearColor C2, float V1, float V2);

    static const int32 EdgeTable[256];
    static const int32 TriTable[256][16];
};
