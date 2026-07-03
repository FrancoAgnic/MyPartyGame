#pragma once
#include "CoreMinimal.h"

// ─────────────────────────────────────────────────────────────────────────────
// Campo SDF disperso por "bricks" (Etapa 1: resolución uniforme fina, sin LOD).
//
// El mundo se divide en bricks de BrickSize³ celdas. Solo se allocan los bricks
// que contienen superficie (dispersión). Cada brick guarda SDF + color por celda.
//
// Convención SDF: valor POSITIVO = dentro del material (igual que el sistema
// anterior). La superficie está en value == 0.
//
// Es un struct plano C++ (sin UObject) para poder mesharlo en el ThreadPool.
// ─────────────────────────────────────────────────────────────────────────────

struct FPTBrick
{
    // BrickSize celdas + 1 borde para que cada celda de meshing tenga sus 8
    // esquinas dentro del mismo brick (evita saltar a bricks vecinos al mallar).
    static constexpr int32 BrickSize = 16;
    static constexpr int32 Stride    = BrickSize + 1; // 17 muestras por eje
    static constexpr int32 NumSamples = Stride * Stride * Stride;

    TArray<float>  SDF;    // NumSamples, init a -1 (vacío)
    TArray<FColor> Color;  // NumSamples, init a blanco

    FPTBrick()
    {
        SDF.Init(-1.f, NumSamples);
        Color.Init(FColor::White, NumSamples);
    }

    static FORCEINLINE int32 LocalIdx(int32 x, int32 y, int32 z)
    {
        return x + y * Stride + z * Stride * Stride;
    }
};

// Coordenada de brick (índice de bloque en el mundo de celdas).
using FPTBrickKey = FIntVector;

// Resultado de mallar un brick: se sube al ProceduralMesh en el GameThread.
struct FPTBrickMesh
{
    int32           Section = INDEX_NONE;
    TArray<FVector> Verts;
    TArray<FVector> Normals;
    TArray<int32>   Tris;
    TArray<FColor>  Colors;
};

// Campo disperso. Vive en APTSculptVolume. Las escrituras (brush) corren en el
// GameThread; el meshing toma snapshots y corre en ThreadPool.
class FPTSculptField
{
public:
    // Tamaño de celda en unidades de mundo (local del actor). Fijo en Etapa 1.
    float VoxelSize = 8.f;

    // ── Acceso a muestras (coordenadas globales de celda) ──────────────────
    float  GetSDF  (int32 X, int32 Y, int32 Z) const;
    FColor GetColor(int32 X, int32 Y, int32 Z) const;
    // Escribe allocando el brick si hace falta. Marca el brick (y vecinos de
    // borde) como dirty.
    void   SetSDF  (int32 X, int32 Y, int32 Z, float V);
    void   SetColor(int32 X, int32 Y, int32 Z, FColor C);

    // SDF interpolado trilineal en coordenadas de celda (float). Para raymarch.
    float  SampleSDF(float X, float Y, float Z) const;

    // ── Dirty tracking ─────────────────────────────────────────────────────
    bool HasDirty() const { return DirtyBricks.Num() > 0; }
    void TakeDirty(TArray<FPTBrickKey>& Out) { Out = DirtyBricks.Array(); DirtyBricks.Reset(); }
    void MarkDirty(const FPTBrickKey& Key)   { DirtyBricks.Add(Key); }

    // Índice de sección del ProceduralMesh para un brick (estable por key).
    int32 SectionIndex(const FPTBrickKey& Key);

    // ── Snapshot para meshing en thread ────────────────────────────────────
    // Copia el brick + su borde +1 en cada eje (lee de bricks vecinos) a un
    // buffer denso listo para Surface Nets, sin tocar el mapa desde el thread.
    struct FBrickSnapshot
    {
        FPTBrickKey Key;
        int32       Section = INDEX_NONE;
        float       VoxelSize = 8.f;
        TArray<float>  SDF;    // (BrickSize+1)³ ya está en el brick; usamos eso
        TArray<FColor> Color;
        bool           bEmpty = true; // sin superficie → sección vacía
    };
    void SnapshotBrick(const FPTBrickKey& Key, FBrickSnapshot& Out) const;

    // Convierte coord global de celda → (brick key, coord local dentro del brick).
    static void CellToBrick(int32 X, int32 Y, int32 Z, FPTBrickKey& OutKey, int32& lx, int32& ly, int32& lz);

    // ── Surface Nets (estático, corre en ThreadPool) ───────────────────────
    static void MeshBrick(const FBrickSnapshot& Snap, FPTBrickMesh& Out);

private:
    TMap<FPTBrickKey, TSharedPtr<FPTBrick>> Bricks;
    TSet<FPTBrickKey>                       DirtyBricks;
    TMap<FPTBrickKey, int32>                SectionOf; // key → sección estable
    int32                                   NextSection = 0;

    const FPTBrick* FindBrick(const FPTBrickKey& Key) const;
    FPTBrick&       FindOrAddBrick(const FPTBrickKey& Key);
};
