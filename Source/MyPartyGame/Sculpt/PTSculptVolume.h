#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ProceduralMeshComponent.h"
#include "PTSculptVolume.generated.h"

UENUM(BlueprintType)
enum class EPTBrushMode : uint8
{
    Add,
    Remove,
    Smooth
};

UCLASS()
class MYPARTYGAME_API APTSculptVolume : public AActor
{
    GENERATED_BODY()
public:
    APTSculptVolume();

    static constexpr int32 GridSize = 64;

    UPROPERTY(EditAnywhere, Category="Sculpt")
    float VoxelSize = 15.f;

    void ApplyBrush(FVector WorldPos, EPTBrushMode Mode, float Radius, float Strength, float DeltaTime);
    void RebuildMesh();

    // ─── Replicación: el cliente llama Server_ApplyBrush, el servidor aplica y multicastea.
    UFUNCTION(Server, Reliable, WithValidation)
    void Server_ApplyBrush(FVector WorldPos, EPTBrushMode Mode, float Radius, float Strength, float DeltaTime);

    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ApplyBrush(FVector WorldPos, EPTBrushMode Mode, float Radius, float Strength, float DeltaTime);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;

private:
    UPROPERTY(VisibleAnywhere)
    UProceduralMeshComponent* Mesh;

    // Positive = inside material (solid). GridSize^3 floats (~1MB at 64^3).
    TArray<float> Grid;
    bool bDirty = false;
    float TimeSinceRebuild = 0.f;
    static constexpr float RebuildInterval = 0.05f;

    void InitGrid();

    bool  InBounds(int32 X, int32 Y, int32 Z) const;
    int32 Idx(int32 X, int32 Y, int32 Z)      const;
    float GetVal(int32 X, int32 Y, int32 Z)    const;
    void  SetVal(int32 X, int32 Y, int32 Z, float V);

    FVector WorldToGrid(FVector W)                   const;
    FVector GridToLocal(float X, float Y, float Z)   const;
    float   SampleDensity(float X, float Y, float Z) const;
    FVector SampleGradient(float X, float Y, float Z) const;

    void RunMarchingCubes(TArray<FVector>& OutVerts,
                          TArray<int32>&   OutTris,
                          TArray<FVector>& OutNormals);

    FVector Interp(FVector P1, FVector P2, float V1, float V2) const;
    FVector InterpNormal(FVector P1, FVector P2, FVector G1, FVector G2, float V1, float V2) const;

    static const int32 EdgeTable[256];
    static const int32 TriTable[256][16];
};
