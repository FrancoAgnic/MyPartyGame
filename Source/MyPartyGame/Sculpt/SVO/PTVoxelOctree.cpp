// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTVoxelOctree.h"

namespace
{
    // Offset del octante/esquina i (bits: x=1, y=2, z=4) en [0,1] por eje.
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

    FORCEINLINE bool Inside(float V) { return V > 0.f; } // POSITIVO = dentro

    // Las 12 aristas del cubo como pares de esquinas (numeración x=1,y=2,z=4).
    static const int32 EdgeCorners[12][2] = {
        {0,1},{2,3},{4,5},{6,7}, // aristas en X
        {0,2},{1,3},{4,6},{5,7}, // aristas en Y
        {0,4},{1,5},{2,6},{3,7}  // aristas en Z
    };
}

// ── Init / Reset ─────────────────────────────────────────────────────────────────
void FPTVoxelOctree::Init(const FVector& InOrigin, float InRootSize, int32 InMaxDepth)
{
    Origin   = InOrigin;
    RootSize = FMath::Max(InRootSize, 1.f);
    MaxDepth = FMath::Clamp(InMaxDepth, 0, 12);
    Root = MakeUnique<FPTOctreeNode>(); // hoja gruesa, todo aire
}

void FPTVoxelOctree::Reset()
{
    Root.Reset();
}

// ── Profundidad adaptativa por radio ──────────────────────────────────────────────
int32 FPTVoxelOctree::DepthForRadius(float Radius) const
{
    // Queremos varias celdas por radio (más celdas = más redondo). cell(d) = RootSize / 2^d.
    const float CellsPerRadius = 2.5f;
    const float TargetCell = FMath::Max(Radius / CellsPerRadius, KINDA_SMALL_NUMBER);
    const float Ratio = RootSize / TargetCell; // = 2^d
    const int32 d = (Ratio > 1.f) ? FMath::RoundToInt(FMath::Log2(Ratio)) : 0;
    return FMath::Clamp(d, 0, MaxDepth);
}

// ── Trilineal sobre 8 esquinas ─────────────────────────────────────────────────────
float FPTVoxelOctree::TrilinearCorners(const float C[8], float fx, float fy, float fz)
{
    const float c00 = FMath::Lerp(C[0], C[1], fx);
    const float c10 = FMath::Lerp(C[2], C[3], fx);
    const float c01 = FMath::Lerp(C[4], C[5], fx);
    const float c11 = FMath::Lerp(C[6], C[7], fx);
    const float c0  = FMath::Lerp(c00, c10, fy);
    const float c1  = FMath::Lerp(c01, c11, fy);
    return FMath::Lerp(c0, c1, fz);
}

// ── Refinar hoja → 8 hijos-hoja (preserva lo esculpido por interpolación) ───────────
void FPTVoxelOctree::RefineLeaf(FPTOctreeNode& Node)
{
    if (!Node.IsLeaf()) return;
    float P[8];
    for (int32 i = 0; i < 8; ++i) P[i] = Node.Corner[i];

    for (int32 c = 0; c < 8; ++c)
    {
        TUniquePtr<FPTOctreeNode> Child = MakeUnique<FPTOctreeNode>();
        const FVector Off = OctantOffset(c) * 0.5f; // esquina mínima del hijo dentro del padre [0..1]
        for (int32 k = 0; k < 8; ++k)
        {
            const FVector KF = Off + OctantOffset(k) * 0.5f; // esquina k del hijo en fracción del padre
            Child->Corner[k] = TrilinearCorners(P, KF.X, KF.Y, KF.Z);
        }
        Node.Children[c] = MoveTemp(Child);
    }
    Node.bLeaf = false;
}

// ── Edición ─────────────────────────────────────────────────────────────────────
void FPTVoxelOctree::EditSphere(const FVector& Center, float Radius, bool bAdd)
{
    if (!Root.IsValid() || Radius <= 0.f) return;
    const int32 TargetDepth = DepthForRadius(Radius);
    EditNode(*Root, Origin, RootSize, 0, Center, Radius, bAdd, TargetDepth);
}

void FPTVoxelOctree::WriteSphereCorners(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize,
                                        const FVector& Center, float Radius, bool bAdd) const
{
    for (int32 k = 0; k < 8; ++k)
    {
        const FVector P = NodeMin + OctantOffset(k) * NodeSize;
        const float Dist = FVector::Dist(P, Center);
        // SDF de la esfera normalizado por el tamaño de celda (>0 dentro de la esfera). El SIGNO es
        // consistente entre niveles (no depende del tamaño) → topología coherente = crack-free.
        const float SphereSDF = FMath::Clamp((Radius - Dist) / NodeSize, -1.f, 1.f);
        float& V = Node.Corner[k];
        V = bAdd ? FMath::Max(V, SphereSDF)   // unión (agregar)
                 : FMath::Min(V, -SphereSDF); // resta  (borrar)
        V = FMath::Clamp(V, -1.f, 1.f);
    }
}

void FPTVoxelOctree::EditNode(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize, int32 Depth,
                              const FVector& Center, float Radius, bool bAdd, int32 TargetDepth) const
{
    if (!SphereHitsBox(Center, Radius, NodeMin, NodeSize)) return;

    if (Node.IsLeaf())
    {
        if (Depth >= TargetDepth || Depth >= MaxDepth)
        {
            WriteSphereCorners(Node, NodeMin, NodeSize, Center, Radius, bAdd);
            return;
        }
        RefineLeaf(Node); // hay que bajar más: subdividir preservando lo esculpido
    }

    // Interno: recursión a los 8 hijos (siempre existen tras RefineLeaf).
    const float Half = NodeSize * 0.5f;
    for (int32 i = 0; i < 8; ++i)
    {
        if (!Node.Children[i].IsValid()) continue;
        const FVector ChildMin = NodeMin + OctantOffset(i) * Half;
        EditNode(*Node.Children[i], ChildMin, Half, Depth + 1, Center, Radius, bAdd, TargetDepth);
    }
}

// ── Sample ──────────────────────────────────────────────────────────────────────
float FPTVoxelOctree::Sample(const FVector& LocalPos) const
{
    if (!Root.IsValid()) return -1.f;
    const FLeafInfo LI = FindLeaf(LocalPos);
    if (!LI.Node) return -1.f; // región aire
    const FVector F = (LocalPos - LI.Min) / LI.Size; // [0..1] dentro de la hoja
    return TrilinearCorners(LI.Node->Corner,
                            FMath::Clamp((float)F.X, 0.f, 1.f),
                            FMath::Clamp((float)F.Y, 0.f, 1.f),
                            FMath::Clamp((float)F.Z, 0.f, 1.f));
}

FPTVoxelOctree::FLeafInfo FPTVoxelOctree::FindLeaf(const FVector& P) const
{
    FLeafInfo Out;
    if (!Root.IsValid()) return Out;
    const FVector Max = Origin + FVector(RootSize);
    if (P.X < Origin.X || P.Y < Origin.Y || P.Z < Origin.Z || P.X >= Max.X || P.Y >= Max.Y || P.Z >= Max.Z)
        return Out; // fuera de la raíz = aire

    const FPTOctreeNode* N = Root.Get();
    FVector Min = Origin;
    float Size = RootSize;
    while (N && !N->IsLeaf())
    {
        const float Half = Size * 0.5f;
        const int32 ix = (P.X >= Min.X + Half) ? 1 : 0;
        const int32 iy = (P.Y >= Min.Y + Half) ? 1 : 0;
        const int32 iz = (P.Z >= Min.Z + Half) ? 1 : 0;
        const int32 idx = ix | (iy << 1) | (iz << 2);
        Min += FVector((float)ix, (float)iy, (float)iz) * Half;
        Size = Half;
        const FPTOctreeNode* Child = N->Children[idx].Get();
        if (!Child) { Out.Min = Min; Out.Size = Size; return Out; } // octante aire (no allocado)
        N = Child;
    }
    Out.Node = N; Out.Min = Min; Out.Size = Size;
    return Out;
}

// ── Vértice Surface Nets de una hoja ───────────────────────────────────────────────
bool FPTVoxelOctree::LeafVertex(const FPTOctreeNode& Leaf, const FVector& Min, float Size, FVector& OutLocal)
{
    FVector Sum(0.f);
    int32 Count = 0;
    for (int32 e = 0; e < 12; ++e)
    {
        const int32 a = EdgeCorners[e][0];
        const int32 b = EdgeCorners[e][1];
        const float va = Leaf.Corner[a];
        const float vb = Leaf.Corner[b];
        if (Inside(va) == Inside(vb)) continue; // sin cruce en esta arista
        float t = va / (va - vb); // cruce cero (va + t*(vb-va) = 0)
        t = FMath::Clamp(t, 0.f, 1.f);
        const FVector Pa = Min + FPTOctreeNode::CornerOffset(a) * Size;
        const FVector Pb = Min + FPTOctreeNode::CornerOffset(b) * Size;
        Sum += FMath::Lerp(Pa, Pb, t);
        ++Count;
    }
    if (Count == 0) return false;
    OutLocal = Sum / (float)Count;
    return true;
}

FVector FPTVoxelOctree::FieldNormal(const FVector& P) const
{
    const float e = FMath::Max(MinCellSize() * 0.5f, 0.01f);
    const float gx = Sample(P + FVector(e, 0, 0)) - Sample(P - FVector(e, 0, 0));
    const float gy = Sample(P + FVector(0, e, 0)) - Sample(P - FVector(0, e, 0));
    const float gz = Sample(P + FVector(0, 0, e)) - Sample(P - FVector(0, 0, e));
    FVector G(gx, gy, gz);
    // grad apunta hacia MÁS material (dentro); la normal exterior es -grad.
    if (!G.Normalize()) return FVector::UpVector;
    return -G;
}

// ── Recolección de hojas ───────────────────────────────────────────────────────────
void FPTVoxelOctree::CollectLeaves(const FPTOctreeNode* N, const FVector& NodeMin, float NodeSize,
                                   TArray<FLeafRef>& Out) const
{
    if (!N) return;
    if (N->IsLeaf()) { Out.Add({ N, NodeMin, NodeSize }); return; }
    const float Half = NodeSize * 0.5f;
    for (int32 i = 0; i < 8; ++i)
        if (N->Children[i].IsValid())
            CollectLeaves(N->Children[i].Get(), NodeMin + OctantOffset(i) * Half, Half, Out);
}

// ── Mallado: Dual Contouring por aristas mínimas ────────────────────────────────────
void FPTVoxelOctree::BuildMesh(TArray<FVector>& OutVerts, TArray<int32>& OutTris, TArray<FVector>& OutNormals) const
{
    OutVerts.Reset(); OutTris.Reset(); OutNormals.Reset();
    if (!Root.IsValid()) return;

    // 1) Un vértice por hoja con cambio de signo.
    TArray<FLeafRef> Leaves;
    CollectLeaves(Root.Get(), Origin, RootSize, Leaves);

    TMap<const FPTOctreeNode*, int32> VertOf;
    VertOf.Reserve(Leaves.Num());
    for (const FLeafRef& L : Leaves)
    {
        FVector VLocal;
        if (!LeafVertex(*L.Node, L.Min, L.Size, VLocal)) continue;
        const int32 Idx = OutVerts.Add(VLocal);
        OutNormals.Add(FieldNormal(VLocal));
        VertOf.Add(L.Node, Idx);
    }

    // 2) Conectividad: por cada "arista mínima" con cambio de signo, unir los vértices de las hojas
    //    que la rodean. La hoja "dueña" (la más fina; a igualdad, la de esquina menor) la emite una vez.
    for (const FLeafRef& L : Leaves)
    {
        if (!VertOf.Contains(L.Node)) continue; // hoja sin superficie
        const float s = L.Size;

        for (int32 axis = 0; axis < 3; ++axis)
        {
            const int32 u = (axis + 1) % 3;
            const int32 v = (axis + 2) % 3;

            for (int32 cv = 0; cv < 2; ++cv)
            for (int32 cu = 0; cu < 2; ++cu)
            {
                // Recta de la arista (paralela a 'axis') en (pu,pv); extremos en axis = [min, min+s].
                FVector A0 = L.Min, A1 = L.Min;
                A0[u] = A1[u] = L.Min[u] + cu * s;
                A0[v] = A1[v] = L.Min[v] + cv * s;
                A0[axis] = L.Min[axis];
                A1[axis] = L.Min[axis] + s;

                const float f0 = Sample(A0);
                const float f1 = Sample(A1);
                if (Inside(f0) == Inside(f1)) continue; // sin cruce → no hay quad

                // Las 4 hojas alrededor de la recta, en orden CCW mirando hacia +axis:
                // (σu,σv) = (-,-),(+,-),(+,+),(-,+).
                static const int32 SU[4] = { -1, +1, +1, -1 };
                static const int32 SV[4] = { -1, -1, +1, +1 };
                const float aMid = L.Min[axis] + 0.5f * s;
                const float eps  = 0.25f * s;

                const FPTOctreeNode* Ring[4] = { nullptr, nullptr, nullptr, nullptr };
                FVector RingMin[4];
                float   RingSize[4] = { 0,0,0,0 };
                bool    bValid = true;
                float   finest = s;
                for (int32 q = 0; q < 4; ++q)
                {
                    FVector Q; Q[axis] = aMid;
                    Q[u] = L.Min[u] + cu * s + SU[q] * eps;
                    Q[v] = L.Min[v] + cv * s + SV[q] * eps;
                    const FLeafInfo LI = FindLeaf(Q);
                    if (!LI.Node) { bValid = false; break; }
                    Ring[q] = LI.Node; RingMin[q] = LI.Min; RingSize[q] = LI.Size;
                    finest = FMath::Min(finest, LI.Size);
                }
                if (!bValid) continue;

                // Sólo la hoja MÁS FINA de la arista la procesa (evita duplicados). Si hay un vecino más
                // fino que L, esta arista no es mínima para L → la emitirá el vecino fino.
                if (finest < s - KINDA_SMALL_NUMBER) continue;

                // A igualdad de tamaño (== s), dueña = la de esquina (min) menor lexicográfica.
                const FPTOctreeNode* Owner = nullptr;
                FVector OwnerMin(FLT_MAX);
                for (int32 q = 0; q < 4; ++q)
                {
                    if (FMath::Abs(RingSize[q] - s) > KINDA_SMALL_NUMBER) continue; // sólo las de tamaño s
                    const FVector& M = RingMin[q];
                    if (M.X < OwnerMin.X - KINDA_SMALL_NUMBER ||
                        (FMath::IsNearlyEqual(M.X, OwnerMin.X) && (M.Y < OwnerMin.Y - KINDA_SMALL_NUMBER ||
                        (FMath::IsNearlyEqual(M.Y, OwnerMin.Y) && M.Z < OwnerMin.Z - KINDA_SMALL_NUMBER))))
                    {
                        OwnerMin = M; Owner = Ring[q];
                    }
                }
                if (Owner != L.Node) continue; // no soy la dueña

                // Vértices únicos alrededor del anillo (una hoja grande puede ocupar 2 cuadrantes).
                int32 Ord[4]; int32 NUnique = 0;
                for (int32 q = 0; q < 4; ++q)
                {
                    const FPTOctreeNode* Nd = Ring[q];
                    if (NUnique > 0 && Nd == Ring[(q + 3) % 4]) continue; // igual al anterior en el anillo
                    const int32* Found = VertOf.Find(Nd);
                    if (!Found) { NUnique = -1; break; }
                    Ord[NUnique++] = *Found;
                }
                if (NUnique < 3) continue;
                // dedup cierre del anillo (primer == último)
                if (NUnique == 4 && Ord[0] == Ord[3]) NUnique = 3;
                if (NUnique < 3) continue;

                // Orientación: si el material está del lado -axis (f0 dentro, f1 fuera) la normal va +axis
                // y el anillo CCW ya es correcto; si no, invertimos.
                const bool bFlip = (Inside(f0) && !Inside(f1));

                auto EmitTri = [&](int32 i0, int32 i1, int32 i2)
                {
                    if (bFlip) { OutTris.Add(i0); OutTris.Add(i2); OutTris.Add(i1); }
                    else       { OutTris.Add(i0); OutTris.Add(i1); OutTris.Add(i2); }
                };

                EmitTri(Ord[0], Ord[1], Ord[2]);
                if (NUnique == 4) EmitTri(Ord[0], Ord[2], Ord[3]);
            }
        }
    }
}

// ── Métricas ──────────────────────────────────────────────────────────────────────
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
