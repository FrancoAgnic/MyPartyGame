#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Materials/MaterialInterface.h"
#include "PTSculptVolume.generated.h"

UENUM(BlueprintType)
enum class EPTStampShape : uint8 { Sphere, Cube, Cylinder, TriPrism };

UENUM(BlueprintType)
enum class EPTEditMode : uint8 { Add, Erase, Paint };

UCLASS()
class MYPARTYGAME_API APTSculptVolume : public AActor
{
    GENERATED_BODY()
public:
    APTSculptVolume();

    static constexpr int32 GridSize      = 120; // resolución alta para detalle fino
    static constexpr int32 ChunkSize     = 20;  // celdas por chunk (120/20 = 6 por eje)
    static constexpr int32 ChunksPerAxis = GridSize / ChunkSize; // 6 → 216 chunks

    UPROPERTY(EditAnywhere, Category="Sculpt") float VoxelSize = 8.f; // 960/120, mantiene el volumen total
    UPROPERTY(EditAnywhere, Category="Sculpt") UMaterialInterface* ClayMaterial = nullptr;

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

    static void BuildStampPreview(EPTStampShape Shape, float Size, float VoxSz,
                                  TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                                  TArray<FVector>& OutNormals);

    static float StampSDF(EPTStampShape Shape, FVector LocalPos, float HalfSize);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;
    virtual void OnConstruction(const FTransform& Transform) override;

private:
    UPROPERTY(VisibleAnywhere) UProceduralMeshComponent* Mesh;
    UPROPERTY(VisibleAnywhere) UBoxComponent* BoundsBox;

    TArray<float>        Grid;
    TArray<FLinearColor> ColorGrid;

    TSet<int32> DirtyChunks;         // chunks que necesitan remallarse
    bool  bRebuildInProgress = false;
    float TimeSinceRebuild   = 0.f;
    static constexpr float RebuildInterval = 0.05f;

    void  InitGrid();
    int32 ChunkIndex(int32 CX, int32 CY, int32 CZ) const;
    void  MarkDirtyRegion(int32 X0, int32 Y0, int32 Z0, int32 X1, int32 Y1, int32 Z1);
    void  RebuildDirtyChunks();

    bool  InBounds(int32 X, int32 Y, int32 Z) const;
    int32 Idx    (int32 X, int32 Y, int32 Z)  const;
    float GetVal (int32 X, int32 Y, int32 Z)  const;
    void  SetVal (int32 X, int32 Y, int32 Z, float V);

    FLinearColor GetColor(int32 X, int32 Y, int32 Z)              const;
    void         SetColor(int32 X, int32 Y, int32 Z, FLinearColor C);

    FVector WorldToGrid(FVector W)                 const;
    FVector GridToLocal(float X, float Y, float Z) const;
    float   SampleGrid (const TArray<float>& G, float X, float Y, float Z) const;

    // Remalla solo los cubos [X0,X1) × [Y0,Y1) × [Z0,Z1) del grid (rango en celdas).
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
