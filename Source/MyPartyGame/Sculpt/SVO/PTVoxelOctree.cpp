// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTVoxelOctree.h"

namespace
{
    // Offset del octante i (bits: x=1, y=2, z=4) en [0,1] por eje.
    FORCEINLINE FVector OctantOffset(int32 i)
    {
        return FVector((i & 1) ? 1.f : 0.f, (i & 2) ? 1.f : 0.f, (i & 4) ? 1.f : 0.f);
    }

    // ¿La esfera (Center,R) toca la caja [Min, Min+Size]?
    FORCEINLINE bool SphereHitsBox(const FVector& Center, float R, const FVector& Min, float Size)
    {
        const FVector Max = Min + FVector(Size);
        const FVector Closest(
            FMath::Clamp(Center.X, Min.X, Max.X),
            FMath::Clamp(Center.Y, Min.Y, Max.Y),
            FMath::Clamp(Center.Z, Min.Z, Max.Z));
        return FVector::DistSquared(Closest, Center) <= R * R;
    }
}

void FPTVoxelOctree::Init(const FVector& InOrigin, float InRootSize, int32 InMaxDepth)
{
    Origin   = InOrigin;
    RootSize = FMath::Max(InRootSize, 1.f);
    MaxDepth = FMath::Clamp(InMaxDepth, 0, 12);
    Root = MakeUnique<FPTOctreeNode>();
    MakeLeaf(*Root); // raíz = hoja gruesa, todo aire
}

void FPTVoxelOctree::Reset()
{
    Root.Reset();
}

int32 FPTVoxelOctree::DepthForRadius(float Radius) const
{
    // Queremos que la celda de la hoja sea ~ Radius / CellsPerRadius (más celdas = más redondo).
    const float CellsPerRadius = 3.0f;
    const float TargetCell = FMath::Max(Radius / CellsPerRadius, KINDA_SMALL_NUMBER);
    // cellSize(d) = RootSize / (2^d * LeafCells) = TargetCell  →  2^d = RootSize / (LeafCells * TargetCell)
    const float Ratio = RootSize / (float)(FPTOctreeNode::LeafCells) / TargetCell;
    const int32 d = (Ratio > 1.f) ? FMath::RoundToInt(FMath::Log2(Ratio)) : 0;
    return FMath::Clamp(d, 0, MaxDepth);
}

// ── Hojas ──────────────────────────────────────────────────────────────────────
void FPTVoxelOctree::MakeLeaf(FPTOctreeNode& Node)
{
    Node.LeafSDF.Init(-1.f, FPTOctreeNode::LeafCount); // todo aire (fuera del material)
}

float FPTVoxelOctree::SampleLeaf(const FPTOctreeNode& Leaf, const FVector& CellCoord)
{
    const int32 N = FPTOctreeNode::LeafCells;
    const FVector C(
        FMath::Clamp(CellCoord.X, 0.f, (float)N),
        FMath::Clamp(CellCoord.Y, 0.f, (float)N),
        FMath::Clamp(CellCoord.Z, 0.f, (float)N));
    const int32 x0 = FMath::Min((int32)C.X, N - 1);
    const int32 y0 = FMath::Min((int32)C.Y, N - 1);
    const int32 z0 = FMath::Min((int32)C.Z, N - 1);
    const float fx = C.X - x0, fy = C.Y - y0, fz = C.Z - z0;
    auto S = [&](int32 x, int32 y, int32 z) { return Leaf.LeafSDF[FPTOctreeNode::SampIdx(x, y, z)]; };
    return FMath::Lerp(
        FMath::Lerp(FMath::Lerp(S(x0,y0,z0),   S(x0+1,y0,z0),   fx), FMath::Lerp(S(x0,y0+1,z0),   S(x0+1,y0+1,z0),   fx), fy),
        FMath::Lerp(FMath::Lerp(S(x0,y0,z0+1), S(x0+1,y0,z0+1), fx), FMath::Lerp(S(x0,y0+1,z0+1), S(x0+1,y0+1,z0+1), fx), fy),
        fz);
}

void FPTVoxelOctree::RefineLeaf(FPTOctreeNode& Node)
{
    if (!Node.IsLeaf()) return;
    const int32 N = FPTOctreeNode::LeafCells;
    const FPTOctreeNode Parent = MoveTemp(Node); // copia del contenido (LeafSDF) para resamplear
    // Node queda vacío tras el move; reconstruimos como interno con 8 hijos hoja.
    Node.LeafSDF.Empty();
    for (int32 i = 0; i < 8; ++i)
    {
        TUniquePtr<FPTOctreeNode> Child = MakeUnique<FPTOctreeNode>();
        MakeLeaf(*Child);
        const FVector Base = OctantOffset(i) * 0.5f; // fracción [0..1] del padre donde arranca el hijo
        for (int32 z = 0; z <= N; ++z)
        for (int32 y = 0; y <= N; ++y)
        for (int32 x = 0; x <= N; ++x)
        {
            // Muestra (x,y,z) del hijo → fracción en el padre → coord de celda del padre [0..N].
            const FVector Frac = Base + FVector((float)x, (float)y, (float)z) / (float)N * 0.5f;
            const FVector ParentCell = Frac * (float)N;
            Child->LeafSDF[FPTOctreeNode::SampIdx(x, y, z)] = SampleLeaf(Parent, ParentCell);
        }
        Node.Children[i] = MoveTemp(Child);
    }
}

// ── Edición ─────────────────────────────────────────────────────────────────────
void FPTVoxelOctree::EditSphere(const FVector& Center, float Radius, bool bAdd)
{
    if (!Root.IsValid() || Radius <= 0.f) return;
    const int32 TargetDepth = DepthForRadius(Radius);
    EditNode(*Root, Origin, RootSize, 0, Center, Radius, bAdd, TargetDepth);
}

void FPTVoxelOctree::EditNode(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize, int32 Depth,
                              const FVector& Center, float Radius, bool bAdd, int32 TargetDepth)
{
    if (!SphereHitsBox(Center, Radius, NodeMin, NodeSize)) return;

    // Si llegamos a la profundidad objetivo (o más) y el nodo no está subdividido → escribir en la hoja.
    if (Depth >= TargetDepth && !Node.HasChildren())
    {
        if (!Node.IsLeaf()) MakeLeaf(Node);
        const int32 N = FPTOctreeNode::LeafCells;
        const float CellSize = NodeSize / (float)N;
        for (int32 z = 0; z <= N; ++z)
        for (int32 y = 0; y <= N; ++y)
        for (int32 x = 0; x <= N; ++x)
        {
            const FVector P = NodeMin + FVector((float)x, (float)y, (float)z) * CellSize;
            const float Dist = FVector::Dist(P, Center);
            const float SphereSDF = FMath::Clamp((Radius - Dist) / CellSize, -1.f, 1.f); // >0 dentro de la esfera
            float& V = Node.LeafSDF[FPTOctreeNode::SampIdx(x, y, z)];
            V = bAdd ? FMath::Max(V, SphereSDF)   // unión (agregar)
                     : FMath::Min(V, -SphereSDF); // resta  (borrar)
            V = FMath::Clamp(V, -1.f, 1.f);
        }
        return;
    }

    // Hay que bajar más: si era hoja, refinarla (preserva lo esculpido); si estaba vacía, crear hijos.
    if (Node.IsLeaf()) RefineLeaf(Node);
    const float Half = NodeSize * 0.5f;
    for (int32 i = 0; i < 8; ++i)
    {
        if (!Node.Children[i].IsValid())
        {
            Node.Children[i] = MakeUnique<FPTOctreeNode>();
            MakeLeaf(*Node.Children[i]); // hijo vacío (aire) que la recursión puede refinar/escribir
        }
        const FVector ChildMin = NodeMin + OctantOffset(i) * Half;
        EditNode(*Node.Children[i], ChildMin, Half, Depth + 1, Center, Radius, bAdd, TargetDepth);
    }
}

// ── Sample ──────────────────────────────────────────────────────────────────────
float FPTVoxelOctree::Sample(const FVector& LocalPos) const
{
    if (!Root.IsValid()) return -1.f;
    return SampleNode(*Root, Origin, RootSize, LocalPos);
}

float FPTVoxelOctree::SampleNode(const FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize, const FVector& P) const
{
    const FVector Max = NodeMin + FVector(NodeSize);
    if (P.X < NodeMin.X || P.Y < NodeMin.Y || P.Z < NodeMin.Z ||
        P.X > Max.X || P.Y > Max.Y || P.Z > Max.Z)
        return -1.f; // fuera del nodo = aire

    if (Node.IsLeaf())
    {
        const FVector CellCoord = (P - NodeMin) / NodeSize * (float)FPTOctreeNode::LeafCells;
        return SampleLeaf(Node, CellCoord);
    }
    // Interno: bajar al hijo que contiene el punto.
    const float Half = NodeSize * 0.5f;
    const int32 ix = (P.X >= NodeMin.X + Half) ? 1 : 0;
    const int32 iy = (P.Y >= NodeMin.Y + Half) ? 1 : 0;
    const int32 iz = (P.Z >= NodeMin.Z + Half) ? 1 : 0;
    const int32 idx = ix | (iy << 1) | (iz << 2);
    if (!Node.Children[idx].IsValid()) return -1.f; // octante vacío = aire
    const FVector ChildMin = NodeMin + FVector((float)ix, (float)iy, (float)iz) * Half;
    return SampleNode(*Node.Children[idx], ChildMin, Half, P);
}

// ── Métricas ──────────────────────────────────────────────────────────────────
int32 FPTVoxelOctree::CountLeavesRec(const FPTOctreeNode* N)
{
    if (!N) return 0;
    if (N->IsLeaf()) return 1;
    int32 c = 0;
    for (int32 i = 0; i < 8; ++i) c += CountLeavesRec(N->Children[i].Get());
    return c;
}

int32 FPTVoxelOctree::CountNodesRec(const FPTOctreeNode* N)
{
    if (!N) return 0;
    int32 c = 1;
    for (int32 i = 0; i < 8; ++i) c += CountNodesRec(N->Children[i].Get());
    return c;
}

int32 FPTVoxelOctree::CountLeaves() const { return CountLeavesRec(Root.Get()); }
int32 FPTVoxelOctree::CountNodes()  const { return CountNodesRec(Root.Get()); }
