// Copyright Epic Games, Inc. All Rights Reserved.
// Sparse Voxel Octree para esculpido ADAPTATIVO (tipo SculptrVR): la resolución depende del tamaño
// de la edición → brocha grande escribe en nodos grandes (pocas muestras → pocos tris), brocha chica
// subdivide a nodos finos (detalle alto). Solo se allocan nodos que se tocan (esparso).
//
// Fase 1.1 (este archivo): estructura + subdivisión adaptativa + sample SDF trilineal + CSG de esfera.
// El mallado (dual contouring) va en el incremento siguiente. NO está conectado al juego todavía.
//
// Convención SDF: POSITIVO = dentro del material, 0 = superficie (igual que FPTSculptField).
// Es un struct plano C++ (sin UObject) para poder mesharlo en el ThreadPool más adelante.

#pragma once
#include "CoreMinimal.h"

// Un nodo del octree: INTERNO (tiene hijos) u HOJA (tiene una grilla densa de SDF). Nunca ambos.
struct FPTOctreeNode
{
    // Celdas por eje dentro de una hoja (8 celdas → 9 muestras de esquina por eje).
    static constexpr int32 LeafCells = 8;
    static constexpr int32 LeafSamp  = LeafCells + 1;                 // muestras por eje (esquinas)
    static constexpr int32 LeafCount = LeafSamp * LeafSamp * LeafSamp; // muestras totales de una hoja

    TUniquePtr<FPTOctreeNode> Children[8]; // hijos (octantes). Interno si alguno es válido.
    TArray<float>             LeafSDF;      // grilla de esquinas si es HOJA; vacía si es interno.

    bool IsLeaf() const { return LeafSDF.Num() == LeafCount; }
    bool HasChildren() const
    {
        for (int32 i = 0; i < 8; ++i) if (Children[i].IsValid()) return true;
        return false;
    }

    static FORCEINLINE int32 SampIdx(int32 x, int32 y, int32 z)
    {
        return x + y * LeafSamp + z * LeafSamp * LeafSamp;
    }
};

class FPTVoxelOctree
{
public:
    // Origin = esquina mínima del cubo raíz; RootSize = lado del cubo (UU local); MaxDepth = profundidad
    // máxima de subdivisión (define el detalle más fino: celda mínima = RootSize / (2^MaxDepth * LeafCells)).
    void Init(const FVector& InOrigin, float InRootSize, int32 InMaxDepth);
    void Reset();
    bool IsInit() const { return Root.IsValid(); }

    // SDF interpolado (trilineal) en una posición LOCAL. >0 dentro, <0 fuera. Fuera de lo allocado = aire.
    float Sample(const FVector& LocalPos) const;

    // Edita una esfera (CSG). bAdd=true une (agrega arcilla), false resta (borra). La resolución se elige
    // según el RADIO: radio grande → nodos grandes (pocos tris), radio chico → subdivide fino (detalle).
    void EditSphere(const FVector& Center, float Radius, bool bAdd);

    // Fase 1.2 — Mallado: recorre las hojas y mallar cada una con Marching Cubes (reusa el MC del
    // proyecto). Verts en espacio LOCAL. Zonas grandes = hojas grandes = pocos tris; zonas finas =
    // muchas hojas chicas = detalle. NOTA: por ahora cada hoja se mallar por separado → puede haber
    // costuras finas entre hojas de DISTINTO nivel (se cose en el incremento 1.3).
    void BuildMesh(TArray<FVector>& OutVerts, TArray<int32>& OutTris, TArray<FVector>& OutNormals) const;

    // ── Métricas / debug ────────────────────────────────────────────────────
    int32 CountLeaves() const;   // hojas allocadas (≈ cuánto detalle hay)
    int32 CountNodes()  const;   // nodos totales

    // Geometría útil para el mesher (más adelante) y tests.
    FVector GetOrigin()   const { return Origin; }
    float   GetRootSize() const { return RootSize; }
    int32   GetMaxDepth() const { return MaxDepth; }
    // Tamaño de celda de una hoja a profundidad d.
    float   LeafCellSize(int32 Depth) const { return RootSize / (float)((1 << Depth) * FPTOctreeNode::LeafCells); }

private:
    FVector Origin = FVector::ZeroVector;
    float   RootSize = 1024.f;
    int32   MaxDepth = 8;
    TUniquePtr<FPTOctreeNode> Root;

    // Profundidad objetivo para un radio dado (adaptativo). Celda ≈ Radius/CellsPerRadius.
    int32 DepthForRadius(float Radius) const;

    // Descenso recursivo de edición. NodeMin/NodeSize = cubo del nodo. Devuelve nada; muta el árbol.
    void EditNode(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize, int32 Depth,
                  const FVector& Center, float Radius, bool bAdd, int32 TargetDepth);

    // Convierte un nodo (vacío/hoja) en HOJA con SDF inicializado. Si venía de un padre, se puede
    // resamplear desde su SDF (para no perder lo ya esculpido al refinar).
    static void MakeLeaf(FPTOctreeNode& Node);
    // Refina una HOJA en 8 hijos-hoja, resampleando su SDF a cada hijo (trilineal), y la vuelve interna.
    static void RefineLeaf(FPTOctreeNode& Node);

    // Sample trilineal DENTRO de una hoja, en coords locales de celda [0..LeafCells].
    static float SampleLeaf(const FPTOctreeNode& Leaf, const FVector& CellCoord);

    // Descenso recursivo de sample.
    float SampleNode(const FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize, const FVector& P) const;

    // Descenso recursivo de mallado (una hoja = un MC).
    static void MeshRec(const FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize,
                        TArray<FVector>& V, TArray<int32>& T, TArray<FVector>& N);

    static int32 CountLeavesRec(const FPTOctreeNode* N);
    static int32 CountNodesRec(const FPTOctreeNode* N);
};
