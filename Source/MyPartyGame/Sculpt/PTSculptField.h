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
    // Instante (segundos de mundo) en que se AGREGÓ arcilla en esta muestra. El mesher lo hornea
    // por vértice (UV0.x) y el material lo usa para que la arcilla NUEVA brille y se desvanezca.
    // Init muy negativo = "vieja" (no brilla).
    TArray<float>  AddTime;

    FPTBrick()
    {
        SDF.Init(-1.f, NumSamples);
        Color.Init(FColor::White, NumSamples);
        AddTime.Init(-1.e9f, NumSamples);
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
    int32              Section = INDEX_NONE;
    TArray<FVector>    Verts;
    TArray<FVector>    Normals;
    TArray<int32>      Tris;
    TArray<FColor>     Colors;
    // UV0.x = instante de agregado por vértice (para el brillo de la arcilla nueva). UV0.y sin usar.
    TArray<FVector2D>  UV0;
};

// Campo disperso. Vive en APTSculptVolume. Las escrituras (brush) corren en el
// GameThread; el meshing toma snapshots y corre en ThreadPool.
class FPTSculptField
{
public:
    // Tamaño de celda en unidades de mundo (local del actor). Fijo en Etapa 1.
    float VoxelSize = 8.f;

    // Suavizado de display al mallar (0 = off, 1 = máximo). No altera los datos.
    float DisplaySmoothing = 0.f;

    // ── Acceso a muestras (coordenadas globales de celda) ──────────────────
    float  GetSDF  (int32 X, int32 Y, int32 Z) const;
    FColor GetColor(int32 X, int32 Y, int32 Z) const;
    float  GetAddTime(int32 X, int32 Y, int32 Z) const;
    // Escribe allocando el brick si hace falta. Marca el brick (y vecinos de
    // borde) como dirty.
    void   SetSDF  (int32 X, int32 Y, int32 Z, float V);
    void   SetColor(int32 X, int32 Y, int32 Z, FColor C);
    // Marca la muestra como "recién agregada" en el instante T (segundos de mundo).
    void   SetAddTime(int32 X, int32 Y, int32 Z, float T);

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
        int32       Step = 1;         // paso de mallado (1=fino, 2=grueso)
        TArray<float>  SDF;
        TArray<FColor> Color;
        TArray<float>  AddTime;       // paralelo a Color: instante de agregado por muestra
        bool           bEmpty = true; // sin superficie → sección vacía
    };
    // Actualiza el cache de "flatness" del brick (por eso no es const).
    void SnapshotBrick(const FPTBrickKey& Key, FBrickSnapshot& Out);

    // Decide el paso de mallado: 2 solo si el brick y sus 6 vecinos son lisos
    // (evita costuras LOD en bordes de detalle).
    int32 DecideStep(const FPTBrickKey& Key) const;

    // Convierte coord global de celda → (brick key, coord local dentro del brick).
    static void CellToBrick(int32 X, int32 Y, int32 Z, FPTBrickKey& OutKey, int32& lx, int32& ly, int32& lz);

    // ── Undo por TRAZO ─────────────────────────────────────────────────────
    // Copy-on-write: durante un trazo, la primera vez que se toca un brick se guarda una copia.
    // Deshacer = restaurar esas copias. La memoria es proporcional a lo que tocó el trazo (no al
    // volumen entero), que es lo que hace viable tener varios niveles de undo.
    void BeginStroke();            // arranca a grabar (cierra cualquier grabación previa)
    void PushStroke();             // cierra el trazo y lo apila (si tocó algo)
    bool UndoStroke();             // restaura el último trazo apilado y lo marca dirty
    bool CanUndo() const { return UndoStack.Num() > 0; }
    void ClearUndo();              // se llama al limpiar todo (ya no hay a qué volver)

    // ── Surface Nets (estático, corre en ThreadPool) ───────────────────────
    static void MeshBrick(const FBrickSnapshot& Snap, FPTBrickMesh& Out);

private:
    TMap<FPTBrickKey, TSharedPtr<FPTBrick>> Bricks;
    TSet<FPTBrickKey>                       DirtyBricks;
    TMap<FPTBrickKey, int32>                SectionOf;  // key → sección estable
    TMap<FPTBrickKey, float>                Flatness;   // key → coherencia [0,1]
    int32                                   NextSection = 0;

    const FPTBrick* FindBrick(const FPTBrickKey& Key) const;
    FPTBrick&       FindOrAddBrick(const FPTBrickKey& Key);

    // Un trazo = los bricks que tocó, con su contenido ANTERIOR. Valor null = el brick no existía
    // antes (al deshacer hay que borrarlo).
    struct FStrokeBackup { TMap<FPTBrickKey, TSharedPtr<FPTBrick>> Bricks; };
    FStrokeBackup          CurrentStroke;
    TArray<FStrokeBackup>  UndoStack;
    bool                   bRecording = false;
    static constexpr int32 MaxUndoSteps = 8;

    // Guarda el estado previo del brick si es la primera vez que lo toca este trazo.
    void BackupBrick(const FPTBrickKey& Key);
};
