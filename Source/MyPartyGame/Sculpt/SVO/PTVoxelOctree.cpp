// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTVoxelOctree.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
    // Offset del octante/esquina i (bits: x=1, y=2, z=4) en [0,1] por eje.
    FORCEINLINE FVector OctantOffset(int32 i)
    {
        return FVector((i & 1) ? 1.f : 0.f, (i & 2) ? 1.f : 0.f, (i & 4) ? 1.f : 0.f);
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
    UndoStack.Reset();
    PendingBalanceBounds = FBox(ForceInit);
}

void FPTVoxelOctree::Reset()
{
    Root.Reset();
    PendingBalanceBounds = FBox(ForceInit);
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

    FColor PC[8];
    for (int32 i = 0; i < 8; ++i) PC[i] = Node.Col[i];

    for (int32 c = 0; c < 8; ++c)
    {
        TUniquePtr<FPTOctreeNode> Child = MakeUnique<FPTOctreeNode>();
        const FVector Off = OctantOffset(c) * 0.5f; // esquina mínima del hijo dentro del padre [0..1]
        for (int32 k = 0; k < 8; ++k)
        {
            const FVector KF = Off + OctantOffset(k) * 0.5f; // esquina k del hijo en fracción del padre
            Child->Corner[k] = TrilinearCorners(P, KF.X, KF.Y, KF.Z);
            // Color: esquina del padre más cercana (evita mezclar float↔FColor; las zonas editadas se repintan).
            const int32 nx = KF.X >= 0.5f ? 1 : 0, ny = KF.Y >= 0.5f ? 1 : 0, nz = KF.Z >= 0.5f ? 1 : 0;
            Child->Col[k] = PC[nx | (ny << 1) | (nz << 2)];
        }
        Node.Children[c] = MoveTemp(Child);
    }
    Node.bLeaf = false;
}

// ── Edición ─────────────────────────────────────────────────────────────────────
// SDFs analíticas en espacio LOCAL de la shape (POSITIVO = dentro, en UU). h = semiejes.
namespace
{
    float SdfEllipsoid(const FVector& p, const FVector& h)
    {
        const FVector hs(FMath::Max(h.X, 1e-3f), FMath::Max(h.Y, 1e-3f), FMath::Max(h.Z, 1e-3f));
        const float k0 = FVector(p.X / hs.X, p.Y / hs.Y, p.Z / hs.Z).Size();
        const float k1 = FVector(p.X / (hs.X * hs.X), p.Y / (hs.Y * hs.Y), p.Z / (hs.Z * hs.Z)).Size();
        if (k1 < KINDA_SMALL_NUMBER) return hs.GetMin(); // en el centro
        return -(k0 * (k0 - 1.f) / k1); // negado: >0 dentro
    }
    float SdfBox(const FVector& p, const FVector& h)
    {
        const FVector q = FVector(FMath::Abs(p.X), FMath::Abs(p.Y), FMath::Abs(p.Z)) - h;
        const float outside = FVector(FMath::Max(q.X, 0.f), FMath::Max(q.Y, 0.f), FMath::Max(q.Z, 0.f)).Size();
        const float inside  = FMath::Min(FMath::Max3(q.X, q.Y, q.Z), 0.f);
        return -(outside + inside); // >0 dentro
    }
    float SdfCylinder(const FVector& p, const FVector& h) // eje Z, radio h.X (elíptico con h.Y), semialtura h.Z
    {
        const float rx = FMath::Max(h.X, 1e-3f), ry = FMath::Max(h.Y, 1e-3f);
        const float radial = FVector2D(p.X / rx, p.Y / ry).Size() * FMath::Min(rx, ry) - FMath::Min(rx, ry);
        const float dz = FMath::Abs(p.Z) - h.Z;
        const float outside = FVector2D(FMath::Max(radial, 0.f), FMath::Max(dz, 0.f)).Size();
        const float inside  = FMath::Min(FMath::Max(radial, dz), 0.f);
        return -(outside + inside);
    }
    float SdfTorus(const FVector& p, const FVector& h) // plano XY, R mayor=h.X, r menor=h.Z
    {
        const FVector2D q(FVector2D(p.X, p.Y).Size() - h.X, p.Z);
        return -(q.Size() - FMath::Max(h.Z, 1e-3f));
    }
    float SdfCone(const FVector& p, const FVector& h) // base radio h.X en z=-h.Z, ápice en z=+h.Z
    {
        const float tHeight = 2.f * FMath::Max(h.Z, 1e-3f);
        const float frac = FMath::Clamp((h.Z - p.Z) / tHeight, 0.f, 1.f); // 0 en ápice, 1 en base
        const float allowed = h.X * frac;
        const float radial = allowed - FVector2D(p.X, p.Y).Size();
        const float dz = h.Z - FMath::Abs(p.Z);
        return FMath::Min(radial, dz); // aprox: >0 dentro
    }

    float ShapeSDF(EPTSVOShape S, const FVector& p, const FVector& h)
    {
        switch (S)
        {
        case EPTSVOShape::Box:      return SdfBox(p, h);
        case EPTSVOShape::Cylinder: return SdfCylinder(p, h);
        case EPTSVOShape::Torus:    return SdfTorus(p, h);
        case EPTSVOShape::Cone:     return SdfCone(p, h);
        case EPTSVOShape::Sphere:
        default:                    return SdfEllipsoid(p, h);
        }
    }
}

void FPTVoxelOctree::EditSphere(const FVector& Center, float Radius, bool bAdd, const FColor& PaintColor)
{
    if (!Root.IsValid() || Radius <= 0.f) return;
    const int32 TargetDepth = DepthForRadius(Radius);
    const FBox Bounds(Center - FVector(Radius), Center + FVector(Radius));
    EditField(Bounds, TargetDepth, bAdd, PaintColor,
              [&](const FVector& P) { return Radius - FVector::Dist(P, Center); }); // >0 dentro
}

void FPTVoxelOctree::EditShape(const FTransform& Xf, EPTSVOShape Shape, const FVector& HalfExtent,
                               bool bAdd, const FColor& PaintColor)
{
    if (!Root.IsValid()) return;
    const FVector H = HalfExtent.ComponentMax(FVector(1e-2f));

    // Resolución por el semieje MÁS CHICO (features finos → más detalle).
    const int32 TargetDepth = DepthForRadius(H.GetMin());

    // AABB mundo = caja de los 8 vértices del box [-H,H] transformados por Xf (rotación + posición).
    FBox Bounds(ForceInit);
    for (int32 c = 0; c < 8; ++c)
    {
        const FVector Local(((c & 1) ? H.X : -H.X), ((c & 2) ? H.Y : -H.Y), ((c & 4) ? H.Z : -H.Z));
        Bounds += Xf.TransformPosition(Local);
    }

    const FTransform Inv = Xf.Inverse();
    EditField(Bounds, TargetDepth, bAdd, PaintColor,
              [&](const FVector& P) { return ShapeSDF(Shape, Inv.TransformPosition(P), H); });
}

bool FPTVoxelOctree::PaintShape(const FTransform& Xf, EPTSVOShape Shape, const FVector& HalfExtent, const FColor& Color)
{
    if (!Root.IsValid()) return false;
    const FVector H = HalfExtent.ComponentMax(FVector(1e-2f));
    FBox Bounds(ForceInit);
    for (int32 c = 0; c < 8; ++c)
    {
        const FVector Local(((c & 1) ? H.X : -H.X), ((c & 2) ? H.Y : -H.Y), ((c & 4) ? H.Z : -H.Z));
        Bounds += Xf.TransformPosition(Local);
    }
    const FTransform Inv = Xf.Inverse();
    return PaintNode(*Root, Origin, RootSize, Bounds, Color,
                     [&](const FVector& P) { return ShapeSDF(Shape, Inv.TransformPosition(P), H); });
}

bool FPTVoxelOctree::PaintNode(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize,
                               const FBox& WorldBounds, const FColor& Color, FSDFFunc SDF) const
{
    const FBox NodeBox(NodeMin, NodeMin + FVector(NodeSize));
    if (!NodeBox.Intersect(WorldBounds)) return false;

    if (Node.IsLeaf())
    {
        // Solo hojas de superficie (tienen cambio de signo). Pinta las esquinas dentro de la shape.
        bool bIn = false, bOut = false;
        for (int32 k = 0; k < 8; ++k) { if (Node.Corner[k] > 0.f) bIn = true; else bOut = true; }
        if (!(bIn && bOut)) return false; // no es superficie
        bool bPainted = false;
        for (int32 k = 0; k < 8; ++k)
        {
            const FVector P = NodeMin + OctantOffset(k) * NodeSize;
            if (SDF(P) > -NodeSize) { Node.Col[k] = Color; bPainted = true; } // dentro de la shape + 1 celda
        }
        return bPainted;
    }

    bool bAny = false;
    const float Half = NodeSize * 0.5f;
    for (int32 i = 0; i < 8; ++i)
        if (Node.Children[i].IsValid())
            bAny |= PaintNode(*Node.Children[i], NodeMin + OctantOffset(i) * Half, Half, WorldBounds, Color, SDF);
    return bAny;
}

void FPTVoxelOctree::WriteCorners(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize,
                                  bool bAdd, const FColor& PaintColor, FSDFFunc SDF) const
{
    for (int32 k = 0; k < 8; ++k)
    {
        const FVector P = NodeMin + OctantOffset(k) * NodeSize;
        const float d = SDF(P); // >0 dentro (UU)
        // Normalizado por el tamaño de celda. El SIGNO no depende del tamaño → crack-free entre niveles.
        const float SN = FMath::Clamp(d / NodeSize, -1.f, 1.f);
        float& V = Node.Corner[k];
        V = bAdd ? FMath::Max(V, SN) : FMath::Min(V, -SN);
        V = FMath::Clamp(V, -1.f, 1.f);
        // Recorte al lienzo: las esquinas en el límite o fuera del box = AIRE, así la arcilla contra la
        // pared genera tapa (cambio de signo) y queda cerrada. Tolerancia chica para incluir la cara.
        if (bClamp)
        {
            const float Tol = NodeSize * 0.01f;
            if (P.X <= ClampBox.Min.X + Tol || P.Y <= ClampBox.Min.Y + Tol || P.Z <= ClampBox.Min.Z + Tol ||
                P.X >= ClampBox.Max.X - Tol || P.Y >= ClampBox.Max.Y - Tol || P.Z >= ClampBox.Max.Z - Tol)
                V = -1.f;
        }
        // Pintar la banda afectada (dentro + 1 celda) para que las 2 esquinas de cada arista de
        // superficie queden del color del sello → color sólido.
        if (bAdd && d > -NodeSize) Node.Col[k] = PaintColor;
    }
}

// Valor y color en la esquina k del nodo = esquina k de la hoja más profunda en el octante k.
void FPTVoxelOctree::GetCornerDeep(const FPTOctreeNode& N, int32 k, float& OutV, FColor& OutC)
{
    const FPTOctreeNode* Cur = &N;
    while (!Cur->IsLeaf())
    {
        const FPTOctreeNode* Ch = Cur->Children[k].Get();
        if (!Ch) { OutV = -1.f; OutC = FColor::White; return; } // octante aire
        Cur = Ch;
    }
    OutV = Cur->Corner[k];
    OutC = Cur->Col[k];
}

// Colapsa un nodo interno a HOJA, downsampleando: cada esquina toma el valor/color del campo en esa
// esquina (esquina de la hoja más profunda del octante). Descarta la subdivisión fina.
void FPTVoxelOctree::CollapseToLeaf(FPTOctreeNode& Node)
{
    if (Node.IsLeaf()) return;
    float NC[8]; FColor NCol[8];
    for (int32 k = 0; k < 8; ++k) GetCornerDeep(Node, k, NC[k], NCol[k]);
    for (int32 i = 0; i < 8; ++i) Node.Children[i].Reset();
    for (int32 k = 0; k < 8; ++k) { Node.Corner[k] = NC[k]; Node.Col[k] = NCol[k]; }
    Node.bLeaf = true;
}

void FPTVoxelOctree::EditField(const FBox& WorldBounds, int32 TargetDepth, bool bAdd,
                               const FColor& PaintColor, FSDFFunc SDF)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SVO_Edit);
    EditFieldNode(*Root, Origin, RootSize, 0, WorldBounds, TargetDepth, bAdd, PaintColor, SDF);
}

void FPTVoxelOctree::EditFieldNode(FPTOctreeNode& Node, const FVector& NodeMin, float NodeSize, int32 Depth,
                                   const FBox& WorldBounds, int32 TargetDepth, bool bAdd,
                                   const FColor& PaintColor, FSDFFunc SDF)
{
    const FBox NodeBox(NodeMin, NodeMin + FVector(NodeSize));
    if (!NodeBox.Intersect(WorldBounds)) return; // el AABB de la shape no toca el nodo

    if (Node.IsLeaf())
    {
        // Refining a leaf also changes the siblings outside the brush AABB.
        PendingBalanceBounds += NodeBox;
        if (Depth >= TargetDepth || Depth >= MaxDepth)
        {
            WriteCorners(Node, NodeMin, NodeSize, bAdd, PaintColor, SDF);
            return;
        }
        RefineLeaf(Node); // subdividir preservando lo esculpido
    }
    else if (Depth >= TargetDepth)
    {
        // El nodo ya está subdividido MÁS FINO que la brocha (esculpiste chico y ahora tapás con una
        // brocha grande). La brocha grande GANA: colapsamos la subdivisión fina a una hoja a ESTA
        // resolución (downsampleando el campo → se preserva lo que haya de otras ediciones, pero se borra
        // el detalle fino) y escribimos acá. Solo si la shape realmente llega a este nodo (para no
        // coarsear detalle fino vecino que quede afuera del sello).
        const FVector Center = NodeMin + FVector(NodeSize * 0.5f);
        if (SDF(Center) > -NodeSize)
        {
            // El colapso crea un salto de resolución con los vecinos finos que quedan alrededor. Marcamos
            // una región AMPLIADA (nodo + 1 tamaño de nodo por lado) para que el balance 2:1 regrade toda
            // la orilla de la transición → anillos intermedios graduales, sin salto brusco ni grietas.
            PendingBalanceBounds += FBox(NodeMin - FVector(NodeSize), NodeMin + FVector(NodeSize * 2.f));
            CollapseToLeaf(Node);
            WriteCorners(Node, NodeMin, NodeSize, bAdd, PaintColor, SDF);
            return;
        }
    }

    const float Half = NodeSize * 0.5f;
    for (int32 i = 0; i < 8; ++i)
    {
        if (!Node.Children[i].IsValid()) continue;
        const FVector ChildMin = NodeMin + OctantOffset(i) * Half;
        EditFieldNode(*Node.Children[i], ChildMin, Half, Depth + 1, WorldBounds, TargetDepth, bAdd, PaintColor, SDF);
    }
}

// ── Serialización / baking ──────────────────────────────────────────────────────────
void FPTVoxelOctree::SerializeNode(FArchive& Ar, FPTOctreeNode& N)
{
    Ar << N.bLeaf;
    if (N.bLeaf)
    {
        for (int32 k = 0; k < 8; ++k) { Ar << N.Corner[k]; Ar << N.Col[k]; }
    }
    else
    {
        uint8 Mask = 0;
        for (int32 i = 0; i < 8; ++i) if (N.Children[i].IsValid()) Mask |= (1 << i);
        Ar << Mask;
        for (int32 i = 0; i < 8; ++i)
            if (Mask & (1 << i)) SerializeNode(Ar, *N.Children[i]);
    }
}

TUniquePtr<FPTOctreeNode> FPTVoxelOctree::DeserializeNode(FArchive& Ar)
{
    TUniquePtr<FPTOctreeNode> N = MakeUnique<FPTOctreeNode>();
    Ar << N->bLeaf;
    if (N->bLeaf)
    {
        for (int32 k = 0; k < 8; ++k) { Ar << N->Corner[k]; Ar << N->Col[k]; }
    }
    else
    {
        uint8 Mask = 0;
        Ar << Mask;
        for (int32 i = 0; i < 8; ++i)
            if (Mask & (1 << i)) N->Children[i] = DeserializeNode(Ar);
    }
    return N;
}

void FPTVoxelOctree::Serialize(TArray<uint8>& OutBytes) const
{
    OutBytes.Reset();
    if (!Root.IsValid()) return;
    FMemoryWriter Ar(OutBytes, /*bIsPersistent=*/true);
    int32 Version = 1;
    Ar << Version;
    FVector O = Origin; float RS = RootSize; int32 MD = MaxDepth;
    Ar << O; Ar << RS; Ar << MD;
    SerializeNode(Ar, *Root);
}

bool FPTVoxelOctree::LoadFromBytes(const TArray<uint8>& InBytes)
{
    if (InBytes.Num() == 0) return false;
    FMemoryReader Ar(InBytes, /*bIsPersistent=*/true);
    int32 Version = 0;
    Ar << Version;
    if (Version != 1) return false;
    Ar << Origin; Ar << RootSize; Ar << MaxDepth;
    Root = DeserializeNode(Ar);
    UndoStack.Empty();
    PendingBalanceBounds = FBox(Origin, Origin + FVector(RootSize));
    return Root.IsValid();
}

// ── Undo (snapshots por clon) ──────────────────────────────────────────────────────
TUniquePtr<FPTOctreeNode> FPTVoxelOctree::CloneNode(const FPTOctreeNode* N)
{
    if (!N) return nullptr;
    TUniquePtr<FPTOctreeNode> C = MakeUnique<FPTOctreeNode>();
    C->bLeaf = N->bLeaf;
    for (int32 i = 0; i < 8; ++i) { C->Corner[i] = N->Corner[i]; C->Col[i] = N->Col[i]; }
    if (!N->bLeaf)
        for (int32 i = 0; i < 8; ++i) C->Children[i] = CloneNode(N->Children[i].Get());
    return C;
}

void FPTVoxelOctree::PushUndoSnapshot()
{
    if (!Root.IsValid()) return;
    UndoStack.Add(CloneNode(Root.Get()));
    while (UndoStack.Num() > FMath::Max(1, MaxUndo)) UndoStack.RemoveAt(0); // tope: descarta el más viejo
}

bool FPTVoxelOctree::Undo()
{
    if (UndoStack.Num() == 0) return false;
    Root = MoveTemp(UndoStack.Last());
    UndoStack.Pop();
    PendingBalanceBounds = FBox(Origin, Origin + FVector(RootSize));
    return true;
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
bool FPTVoxelOctree::LeafVertex(const FPTOctreeNode& Leaf, const FVector& Min, float Size, FVector& OutLocal, FColor& OutColor)
{
    FVector Sum(0.f);
    float RSum = 0, GSum = 0, BSum = 0; // promedio de color en bytes crudos (sin gamma → color fiel)
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
        // Color = el de la esquina de ADENTRO (material). NO promediar con la de afuera (aire=blanco),
        // que aclaraba el color. Así el color queda fiel al elegido.
        const FColor& Ci = Leaf.Col[Inside(va) ? a : b];
        RSum += (float)Ci.R;
        GSum += (float)Ci.G;
        BSum += (float)Ci.B;
        ++Count;
    }
    if (Count == 0) return false;
    OutLocal = Sum / (float)Count;
    const float Inv = 1.f / (float)Count;
    OutColor = FColor((uint8)FMath::RoundToInt(RSum * Inv), (uint8)FMath::RoundToInt(GSum * Inv),
                      (uint8)FMath::RoundToInt(BSum * Inv), 255);
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

// ── Balance 2:1 ─────────────────────────────────────────────────────────────────────
void FPTVoxelOctree::CollectBalanceCandidates(const FPTOctreeNode* N, const FVector& NodeMin,
                                             float NodeSize, const FBox& ChangedBounds,
                                             TArray<FLeafRef>& Out) const
{
    if (!N) return;
    // A leaf's face probes extend half a minimum cell beyond its box. A parent box contains
    // all descendant probes after this expansion, so disjoint subtrees are safe to skip.
    const float Probe = MinCellSize() * 0.5f + KINDA_SMALL_NUMBER;
    if (!FBox(NodeMin, NodeMin + FVector(NodeSize)).ExpandBy(Probe).Intersect(ChangedBounds)) return;
    if (N->IsLeaf()) { Out.Add({N, NodeMin, NodeSize}); return; }
    const float Half = NodeSize * 0.5f;
    for (int32 i = 0; i < 8; ++i)
        CollectBalanceCandidates(N->Children[i].Get(), NodeMin + OctantOffset(i) * Half, Half, ChangedBounds, Out);
}

void FPTVoxelOctree::Balance(TArray<FBox>* OutRefinedBounds, bool bForceFull)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SVO_Balance);
    LastBalanceLeafChecks = 0;
    if (!Root.IsValid()) return;
    FBox ChangedBounds = bForceFull ? FBox(Origin, Origin + FVector(RootSize)) : PendingBalanceBounds;
    PendingBalanceBounds = FBox(ForceInit);
    if (!ChangedBounds.IsValid) return;
    const float MinCell = MinCellSize();
    // Direcciones de las 6 caras (normal saliente).
    static const FVector FaceN[6] = {
        FVector(1,0,0), FVector(-1,0,0), FVector(0,1,0), FVector(0,-1,0), FVector(0,0,1), FVector(0,0,-1) };

    // Solo importan para las costuras las hojas de SUPERFICIE (cambio de signo). Las macizas del
    // interior y las de aire no generan triángulos → no hace falta balancearlas (gran ahorro).
    auto HasSurface = [](const FPTOctreeNode* Nd)
    {
        bool bIn = false, bOut = false;
        for (int32 k = 0; k < 8; ++k) { if (Nd->Corner[k] > 0.f) bIn = true; else bOut = true; }
        return bIn && bOut;
    };

    // ¿La hoja toca la pared del box de recorte? Ahí forzamos 1:1 (en vez de 2:1) para que la tapa
    // contra la pared no tenga T-junctions (grietas en el borde con saltos de resolución).
    auto TouchesClamp = [&](const FVector& Min, float Size) -> bool
    {
        if (!bClamp) return false;
        const float t = MinCell * 0.5f;
        const FVector Max = Min + FVector(Size);
        return (Min.X <= ClampBox.Min.X + t || Min.Y <= ClampBox.Min.Y + t || Min.Z <= ClampBox.Min.Z + t ||
                Max.X >= ClampBox.Max.X - t || Max.Y >= ClampBox.Max.Y - t || Max.Z >= ClampBox.Max.Z - t);
    };

    for (int32 iter = 0; iter <= MaxDepth; ++iter)
    {
        TArray<FLeafRef> Leaves;
        if (bForceFull) CollectLeaves(Root.Get(), Origin, RootSize, Leaves);
        else CollectBalanceCandidates(Root.Get(), Origin, RootSize, ChangedBounds, Leaves);

        TArray<FPTOctreeNode*> ToRefine;
        FBox NextBounds(ForceInit);
        for (const FLeafRef& L : Leaves)
        {
            ++LastBalanceLeafChecks;
            if (L.Size <= MinCell + KINDA_SMALL_NUMBER) continue; // ya en el máximo detalle
            if (!HasSurface(L.Node)) continue;                    // no es superficie → no afecta costuras
            const float probe = MinCell * 0.5f;
            // Interior: 2:1 (vecino ≥2 niveles más fino). Contra la pared: 1:1 (vecino ≥1 nivel más fino)
            // → tapa uniforme, sin grietas en el borde.
            const float FinerFactor = TouchesClamp(L.Min, L.Size) ? 0.5f : 0.25f;
            bool bNeeds = false;
            for (int32 f = 0; f < 6 && !bNeeds; ++f)
            {
                // Grilla 3x3 sobre la cara empujada apenas hacia afuera (densa: no se escapa una
                // sub-zona fina en las esquinas de la cara).
                const int32 ax = (f / 2); // 0=X,1=Y,2=Z
                const int32 u = (ax + 1) % 3, v = (ax + 2) % 3;
                for (int32 jv = 0; jv < 3 && !bNeeds; ++jv)
                for (int32 ju = 0; ju < 3 && !bNeeds; ++ju)
                {
                    FVector P = L.Min;
                    P[ax] += (FaceN[f][ax] > 0 ? L.Size + probe : -probe);
                    P[u]  += (ju + 0.5f) / 3.f * L.Size;
                    P[v]  += (jv + 0.5f) / 3.f * L.Size;
                    const FLeafInfo N = FindLeaf(P);
                    if (N.Node && N.Size <= L.Size * FinerFactor + KINDA_SMALL_NUMBER) bNeeds = true; // vecino más fino
                }
            }
            if (bNeeds)
            {
                ToRefine.Add(const_cast<FPTOctreeNode*>(L.Node));
                const FBox Bounds(L.Min, L.Min + FVector(L.Size));
                NextBounds += Bounds;
                if (OutRefinedBounds) OutRefinedBounds->Add(Bounds);
            }
        }

        if (ToRefine.Num() == 0) break;
        for (FPTOctreeNode* N : ToRefine) RefineLeaf(*N);
        ChangedBounds = NextBounds;
    }
}

// ── Mallado: Dual Contouring por aristas mínimas ────────────────────────────────────
void FPTVoxelOctree::BuildMesh(TArray<FVector>& OutVerts, TArray<int32>& OutTris, TArray<FVector>& OutNormals,
                               TArray<FColor>& OutColors) const
{
    OutVerts.Reset(); OutTris.Reset(); OutNormals.Reset(); OutColors.Reset();
    if (!Root.IsValid()) return;

    // 1) Un vértice por hoja con cambio de signo.
    TArray<FLeafRef> Leaves;
    CollectLeaves(Root.Get(), Origin, RootSize, Leaves);

    TMap<const FPTOctreeNode*, int32> VertOf;
    VertOf.Reserve(Leaves.Num());
    for (const FLeafRef& L : Leaves)
    {
        FVector VLocal; FColor VCol;
        if (!LeafVertex(*L.Node, L.Min, L.Size, VLocal, VCol)) continue;
        const int32 Idx = OutVerts.Add(VLocal);
        OutNormals.Add(FieldNormal(VLocal));
        OutColors.Add(VCol);
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
                // Sondeo con radio CHICO fijo (no proporcional a s): así cae siempre en la celda
                // inmediatamente adyacente a la arista, aun con saltos de nivel extremos.
                const float eps  = FMath::Min(0.25f * s, MinCellSize() * 0.25f);

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

                // Hojas DISTINTAS alrededor del anillo (una hoja grande puede ocupar 2 cuadrantes
                // consecutivos → se dedupea). Orden CCW preservado.
                const FPTOctreeNode* RingU[4]; int32 NR = 0;
                for (int32 q = 0; q < 4; ++q)
                {
                    const FPTOctreeNode* Nd = Ring[q];
                    if (NR > 0 && Nd == RingU[NR - 1]) continue;
                    RingU[NR++] = Nd;
                }
                if (NR > 1 && RingU[NR - 1] == RingU[0]) --NR; // cierre del anillo

                // Mapear a vértices. Si a una hoja le FALTA vértice (p.ej. quedó sólida por un salto
                // grande de nivel), se la salta y se cierra la arista con los que sí existen (triángulo)
                // en vez de dejar un HUECO. Así no hay grietas en las transiciones grande↔chico.
                int32 Ord[4]; int32 NV = 0;
                for (int32 i = 0; i < NR; ++i)
                {
                    const int32* Found = VertOf.Find(RingU[i]);
                    if (Found) Ord[NV++] = *Found;
                }
                if (NV < 3) continue;

                // Orientación: si el material está del lado -axis (f0 dentro, f1 fuera) la normal va +axis
                // y el anillo CCW ya es correcto; si no, invertimos.
                const bool bFlip = (Inside(f0) && !Inside(f1));

                auto EmitTri = [&](int32 i0, int32 i1, int32 i2)
                {
                    if (bFlip) { OutTris.Add(i0); OutTris.Add(i2); OutTris.Add(i1); }
                    else       { OutTris.Add(i0); OutTris.Add(i1); OutTris.Add(i2); }
                };

                EmitTri(Ord[0], Ord[1], Ord[2]);
                if (NV == 4) EmitTri(Ord[0], Ord[2], Ord[3]);
            }
        }
    }
}

// ── Recolección filtrada (para mallado por chunks) ──────────────────────────────────
void FPTVoxelOctree::CollectLeavesFiltered(const FPTOctreeNode* N, const FVector& NodeMin, float NodeSize,
                                           const FBox& Region, float MinSize, TArray<FLeafRef>& Out) const
{
    if (!N) return;
    const FBox NodeBox(NodeMin, NodeMin + FVector(NodeSize));
    if (!NodeBox.Intersect(Region)) return;
    if (N->IsLeaf()) { Out.Add({ N, NodeMin, NodeSize }); return; }
    if (NodeSize < MinSize) return; // descendientes aún más chicos → ninguno califica (poda del pase grueso)
    const float Half = NodeSize * 0.5f;
    for (int32 i = 0; i < 8; ++i)
        if (N->Children[i].IsValid())
            CollectLeavesFiltered(N->Children[i].Get(), NodeMin + OctantOffset(i) * Half, Half, Region, MinSize, Out);
}

// ── Mallado FILTRADO por región + rango de tamaño (re-mallado incremental) ───────────
void FPTVoxelOctree::BuildMeshFiltered(const FBox& OwnerRegion, float MinLeafSize, float MaxLeafSize,
                                       TArray<FVector>& OutVerts, TArray<int32>& OutTris,
                                       TArray<FVector>& OutNormals, TArray<FColor>& OutColors) const
{
    TRACE_CPUPROFILER_EVENT_SCOPE(SVO_MeshChunk);
    OutVerts.Reset(); OutTris.Reset(); OutNormals.Reset(); OutColors.Reset();
    if (!Root.IsValid()) return;

    TArray<FLeafRef> Owners;
    CollectLeavesFiltered(Root.Get(), Origin, RootSize, OwnerRegion, MinLeafSize, Owners);
    if (Owners.Num() == 0) return;

    // Vértices ON-DEMAND, locales a esta sección (se duplican los de hojas vecinas → autocontenida).
    TMap<const FPTOctreeNode*, int32> VertOf;
    auto GetVert = [&](const FPTOctreeNode* Nd, const FVector& Min, float Size) -> int32
    {
        if (const int32* F = VertOf.Find(Nd)) return *F;
        FVector VLocal; FColor VCol;
        int32 Idx = -1;
        if (LeafVertex(*Nd, Min, Size, VLocal, VCol))
        {
            Idx = OutVerts.Add(VLocal);
            OutNormals.Add(FieldNormal(VLocal));
            OutColors.Add(VCol);
        }
        VertOf.Add(Nd, Idx);
        return Idx;
    };

    auto InHalfOpen = [](const FBox& R, const FVector& P) -> bool
    {
        return P.X >= R.Min.X && P.Y >= R.Min.Y && P.Z >= R.Min.Z &&
               P.X <  R.Max.X && P.Y <  R.Max.Y && P.Z <  R.Max.Z;
    };

    for (const FLeafRef& L : Owners)
    {
        if (L.Size < MinLeafSize - KINDA_SMALL_NUMBER || L.Size >= MaxLeafSize - KINDA_SMALL_NUMBER) continue;
        if (!InHalfOpen(OwnerRegion, L.Min)) continue;   // 1 sección por hoja (partición por esquina Min)
        if (GetVert(L.Node, L.Min, L.Size) < 0) continue; // sin superficie

        const float s = L.Size;
        for (int32 axis = 0; axis < 3; ++axis)
        {
            const int32 u = (axis + 1) % 3, v = (axis + 2) % 3;
            for (int32 cv = 0; cv < 2; ++cv)
            for (int32 cu = 0; cu < 2; ++cu)
            {
                FVector A0 = L.Min, A1 = L.Min;
                A0[u] = A1[u] = L.Min[u] + cu * s;
                A0[v] = A1[v] = L.Min[v] + cv * s;
                A0[axis] = L.Min[axis];
                A1[axis] = L.Min[axis] + s;

                const float f0 = Sample(A0);
                const float f1 = Sample(A1);
                if (Inside(f0) == Inside(f1)) continue;

                static const int32 SU[4] = { -1, +1, +1, -1 };
                static const int32 SV[4] = { -1, -1, +1, +1 };
                const float aMid = L.Min[axis] + 0.5f * s;
                const float eps  = FMath::Min(0.25f * s, MinCellSize() * 0.25f);

                const FPTOctreeNode* Ring[4] = { nullptr, nullptr, nullptr, nullptr };
                FVector RingMin[4]; float RingSize[4] = { 0,0,0,0 };
                bool bValid = true; float finest = s;
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
                if (finest < s - KINDA_SMALL_NUMBER) continue;

                const FPTOctreeNode* Owner = nullptr; FVector OwnerMin(FLT_MAX);
                for (int32 q = 0; q < 4; ++q)
                {
                    if (FMath::Abs(RingSize[q] - s) > KINDA_SMALL_NUMBER) continue;
                    const FVector& M = RingMin[q];
                    if (M.X < OwnerMin.X - KINDA_SMALL_NUMBER ||
                        (FMath::IsNearlyEqual(M.X, OwnerMin.X) && (M.Y < OwnerMin.Y - KINDA_SMALL_NUMBER ||
                        (FMath::IsNearlyEqual(M.Y, OwnerMin.Y) && M.Z < OwnerMin.Z - KINDA_SMALL_NUMBER))))
                    { OwnerMin = M; Owner = Ring[q]; }
                }
                if (Owner != L.Node) continue;

                const FPTOctreeNode* RingU[4]; FVector RingUMin[4]; float RingUSize[4]; int32 NR = 0;
                for (int32 q = 0; q < 4; ++q)
                {
                    if (NR > 0 && Ring[q] == RingU[NR - 1]) continue;
                    RingU[NR] = Ring[q]; RingUMin[NR] = RingMin[q]; RingUSize[NR] = RingSize[q]; ++NR;
                }
                if (NR > 1 && RingU[NR - 1] == RingU[0]) --NR;

                int32 Ord[4]; int32 NV = 0;
                for (int32 i = 0; i < NR; ++i)
                {
                    const int32 Vi = GetVert(RingU[i], RingUMin[i], RingUSize[i]);
                    if (Vi >= 0) Ord[NV++] = Vi;
                }
                if (NV < 3) continue;

                const bool bFlip = (Inside(f0) && !Inside(f1));
                auto EmitTri = [&](int32 i0, int32 i1, int32 i2)
                {
                    if (bFlip) { OutTris.Add(i0); OutTris.Add(i2); OutTris.Add(i1); }
                    else       { OutTris.Add(i0); OutTris.Add(i1); OutTris.Add(i2); }
                };
                EmitTri(Ord[0], Ord[1], Ord[2]);
                if (NV == 4) EmitTri(Ord[0], Ord[2], Ord[3]);
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
