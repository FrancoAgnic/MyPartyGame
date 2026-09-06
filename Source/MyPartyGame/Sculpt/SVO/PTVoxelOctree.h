// Copyright Epic Games, Inc. All Rights Reserved.
// Sparse Voxel Octree para esculpido ADAPTATIVO (tipo SculptrVR): la resolución depende del tamaño
// de la edición → brocha grande escribe en nodos grandes (pocos vértices → pocos tris), brocha chica
// subdivide a nodos finos (detalle alto). Solo se allocan nodos que se tocan (esparso).
//
// Mallado: DUAL CONTOURING (variante Surface Nets). Cada HOJA con cambio de signo aporta UN vértice
// (promedio de los cruces cero de sus 12 aristas). La conectividad se arma recorriendo las "aristas
// mínimas" del árbol: por cada arista con cambio de signo se conecta el vértice de las (hasta 4) hojas
// que la rodean. Como los vértices se COMPARTEN entre hojas de distinto nivel, la malla es
// CRACK-FREE por construcción: no hay costuras aunque una hoja grande toque una chica.
//
// Convención SDF: POSITIVO = dentro del material, 0 = superficie (igual que FPTSculptField).
// Es un struct plano C++ (sin UObject) para poder mesharlo en el ThreadPool más adelante.

#pragma once
#include "CoreMinimal.h"

// Formas analíticas del sello para el SVO (todas soportan escala no-uniforme + rotación).
enum class EPTSVOShape : uint8 { Sphere, Box, Cylinder, Torus, Cone };

// Un nodo del octree. HOJA = una sola celda cúbica con 8 valores SDF en las esquinas.
// INTERNO = 8 hijos (octantes) y sin esquinas propias. Nunca ambos.
struct FPTOctreeNode
{
    TUniquePtr<FPTOctreeNode> Children[8]; // hijos (octantes) si es interno
    float  Corner[8];                       // SDF en las 8 esquinas si es hoja
    FColor Col[8];                          // color de arcilla en las 8 esquinas
    bool   bLeaf = true;

    FPTOctreeNode()
    {
        for (int32 i = 0; i < 8; ++i) { Corner[i] = -1.f; Col[i] = FColor::White; } // aire, blanco
    }

    bool IsLeaf() const { return bLeaf; }

    // Esquina c: x=bit0, y=bit1, z=bit2. Posición = NodeMin + (x,y,z)*NodeSize.
    static FORCEINLINE FVector CornerOffset(int32 c)
    {
        return FVector((c & 1) ? 1.f : 0.f, (c & 2) ? 1.f : 0.f, (c & 4) ? 1.f : 0.f);
    }
};

class FPTVoxelOctree
{
public:
    // Origin = esquina mínima del cubo raíz; RootSize = lado del cubo (UU local); MaxDepth = profundidad
    // máxima (celda mínima = RootSize / 2^MaxDepth). Hojas de UNA celda: el detalle lo da la profundidad.
    void Init(const FVector& InOrigin, float InRootSize, int32 InMaxDepth);
    void Reset();
    bool IsInit() const { return Root.IsValid(); }

    // SDF interpolado (trilineal) en una posición LOCAL. >0 dentro, <0 fuera. Fuera de lo allocado = aire.
    float Sample(const FVector& LocalPos) const;

    // Edita una esfera (CSG). bAdd=true une (agrega arcilla), false resta (borra). La resolución se elige
    // según el RADIO: radio grande → nodos grandes (pocos tris), radio chico → subdivide fino (detalle).
    // PaintColor = color de la arcilla agregada (ignorado al borrar).
    void EditSphere(const FVector& Center, float Radius, bool bAdd, const FColor& PaintColor = FColor::White);

    // Sello genérico: shape analítica con escala NO-UNIFORME (HalfExtent por eje) y ROTACIÓN (Xf).
    // Xf aporta posición + rotación (su escala se ignora; el tamaño lo da HalfExtent en UU local).
    void EditShape(const FTransform& Xf, EPTSVOShape Shape, const FVector& HalfExtent,
                   bool bAdd, const FColor& PaintColor = FColor::White);

    // Balancea el árbol a 2:1 (hojas vecinas difieren máx. 1 nivel). Reduce muchísimo los artefactos
    // en transiciones con saltos grandes de nivel (brocha chica y después grande encima). Llamar
    // antes de BuildMesh.
    void Balance();

    // Mallado por Dual Contouring (crack-free entre niveles). Verts en espacio LOCAL.
    void BuildMesh(TArray<FVector>& OutVerts, TArray<int32>& OutTris, TArray<FVector>& OutNormals,
                   TArray<FColor>& OutColors) const;

    // ── Baking / persistencia (serialización del octree a bytes) ─────────────
    // Snapshot compacto del árbol para guardar (SaveGame), bakear escenografía o mandar por red.
    void Serialize(TArray<uint8>& OutBytes) const;
    bool LoadFromBytes(const TArray<uint8>& InBytes);

    // ── Undo por trazo (snapshots) ──────────────────────────────────────────
    // Llamá PushUndoSnapshot() ANTES de cada trazo; Undo() vuelve al estado previo.
    void PushUndoSnapshot();
    bool Undo();
    int32 UndoDepth() const { return UndoStack.Num(); }
    int32 MaxUndo = 20;

    // ── Métricas / debug ────────────────────────────────────────────────────
    int32 CountLeaves() const;   // hojas allocadas (≈ cuánto detalle hay)
    int32 CountNodes()  const;   // nodos totales

    FVector GetOrigin()   const { return Origin; }
    float   GetRootSize() const { return RootSize; }
    int32   GetMaxDepth() const { return MaxDepth; }
    float   MinCellSize() const { return RootSize / (float)(1 << MaxDepth); }

private:
    FVector Origin = FVector::ZeroVector;
    float   RootSize = 1024.f;
    int32   MaxDepth = 8;
    TUniquePtr<FPTOctreeNode> Root;
    TArray<TUniquePtr<FPTOctreeNode>> UndoStack; // snapshots (clones) para deshacer

    static TUniquePtr<FPTOctreeNode> CloneNode(const FPTOctreeNode* N);
    static void SerializeNode(FArchive& Ar, FPTOctreeNode& N);
    static TUniquePtr<FPTOctreeNode> DeserializeNode(FArchive& Ar);

    // Profundidad objetivo para un radio dado (adaptativo). Celda ≈ Radius/CellsPerRadius.
    int32 DepthForRadius(float Radius) const;

    // SDF genérico (POSITIVO = dentro, en UU). Descenso recursivo de edición.
    using FSDFFunc = TFunctionRef<float(const FVector&)>;
    void EditField(const FBox& WorldBounds, int32 TargetDepth, bool bAdd, const FColor& PaintColor, FSDFFunc SDF);
    void EditFieldNode(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize, int32 Depth,
                       const FBox& WorldBounds, int32 TargetDepth, bool bAdd, const FColor& PaintColor, FSDFFunc SDF) const;
    void WriteCorners(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize,
                      bool bAdd, const FColor& PaintColor, FSDFFunc SDF) const;

    // Refina una HOJA en 8 hijos-hoja, resampleando (trilineal) sus esquinas a cada hijo.
    static void RefineLeaf(FPTOctreeNode& Node);
    static float TrilinearCorners(const float C[8], float fx, float fy, float fz);

    // ── Consulta espacial ────────────────────────────────────────────────────
    struct FLeafInfo
    {
        const FPTOctreeNode* Node = nullptr; // null si la región es aire (no allocada)
        FVector Min = FVector::ZeroVector;
        float   Size = 0.f;
    };
    FLeafInfo FindLeaf(const FVector& P) const;

    // ── Mallado ───────────────────────────────────────────────────────────────
    struct FLeafRef { const FPTOctreeNode* Node; FVector Min; float Size; };
    void CollectLeaves(const FPTOctreeNode* N, const FVector& NodeMin, float NodeSize, TArray<FLeafRef>& Out) const;
    // Vértice Surface Nets de una hoja (promedio de cruces de sus 12 aristas). Devuelve false si no hay cruce.
    // OutColor = color interpolado en los cruces.
    static bool LeafVertex(const FPTOctreeNode& Leaf, const FVector& Min, float Size, FVector& OutLocal, FColor& OutColor);
    FVector FieldNormal(const FVector& P) const; // normal = -grad(SDF)

    static int32 CountLeavesRec(const FPTOctreeNode* N);
    static int32 CountNodesRec(const FPTOctreeNode* N);
};
