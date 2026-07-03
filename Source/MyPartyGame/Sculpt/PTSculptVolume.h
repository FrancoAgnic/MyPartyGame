#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
#include "PTSculptField.h"
#include "PTSculptVolume.generated.h"

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

    // Resolución fina del campo (tamaño de celda en UU). Etapa 1: uniforme.
    UPROPERTY(EditAnywhere, Category="Sculpt") float VoxelSize = 8.f;
    UPROPERTY(EditAnywhere, Category="Sculpt") UMaterialInterface* ClayMaterial = nullptr;

    // Modo Smooth: intensidad del suavizado por aplicación y empuje anti-erosión.
    UPROPERTY(EditAnywhere, Category="Sculpt") float SmoothStrength = 0.4f;
    UPROPERTY(EditAnywhere, Category="Sculpt") float SmoothBias     = 0.05f; // hacia afuera, evita que encoja

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
