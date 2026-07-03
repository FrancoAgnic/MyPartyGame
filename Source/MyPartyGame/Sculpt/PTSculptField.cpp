#include "PTSculptField.h"

// ─── Coord helpers ────────────────────────────────────────────────────────────

static FORCEINLINE int32 FloorDiv(int32 a, int32 b)
{
    // División hacia -infinito (para coords de celda negativas).
    int32 q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) --q;
    return q;
}

void FPTSculptField::CellToBrick(int32 X, int32 Y, int32 Z, FPTBrickKey& OutKey, int32& lx, int32& ly, int32& lz)
{
    const int32 BS = FPTBrick::BrickSize;
    OutKey = FPTBrickKey(FloorDiv(X, BS), FloorDiv(Y, BS), FloorDiv(Z, BS));
    lx = X - OutKey.X * BS;
    ly = Y - OutKey.Y * BS;
    lz = Z - OutKey.Z * BS;
}

const FPTBrick* FPTSculptField::FindBrick(const FPTBrickKey& Key) const
{
    const TSharedPtr<FPTBrick>* Found = Bricks.Find(Key);
    return (Found && Found->IsValid()) ? Found->Get() : nullptr;
}

FPTBrick& FPTSculptField::FindOrAddBrick(const FPTBrickKey& Key)
{
    TSharedPtr<FPTBrick>& Slot = Bricks.FindOrAdd(Key);
    if (!Slot.IsValid()) Slot = MakeShared<FPTBrick>();
    return *Slot;
}

int32 FPTSculptField::SectionIndex(const FPTBrickKey& Key)
{
    if (int32* Found = SectionOf.Find(Key)) return *Found;
    const int32 S = NextSection++;
    SectionOf.Add(Key, S);
    return S;
}

// ─── Muestras: un brick almacena BrickSize³ celdas propias ────────────────────
//
// NOTA: FPTBrick tiene Stride = BrickSize+1, pero en Etapa 1 usamos solo el
// sub-rango [0,BrickSize) de cada brick como celdas propias; el meshing lee un
// borde extra vía GetSDF global (que busca en el brick vecino). Así no hace falta
// mantener muestras duplicadas en el borde.

float FPTSculptField::GetSDF(int32 X, int32 Y, int32 Z) const
{
    FPTBrickKey Key; int32 lx, ly, lz;
    CellToBrick(X, Y, Z, Key, lx, ly, lz);
    if (const FPTBrick* B = FindBrick(Key))
        return B->SDF[FPTBrick::LocalIdx(lx, ly, lz)];
    return -1.f; // vacío
}

FColor FPTSculptField::GetColor(int32 X, int32 Y, int32 Z) const
{
    FPTBrickKey Key; int32 lx, ly, lz;
    CellToBrick(X, Y, Z, Key, lx, ly, lz);
    if (const FPTBrick* B = FindBrick(Key))
        return B->Color[FPTBrick::LocalIdx(lx, ly, lz)];
    return FColor::White;
}

void FPTSculptField::SetSDF(int32 X, int32 Y, int32 Z, float V)
{
    FPTBrickKey Key; int32 lx, ly, lz;
    CellToBrick(X, Y, Z, Key, lx, ly, lz);
    FPTBrick& B = FindOrAddBrick(Key);
    B.SDF[FPTBrick::LocalIdx(lx, ly, lz)] = FMath::Clamp(V, -1.f, 1.f);
}

void FPTSculptField::SetColor(int32 X, int32 Y, int32 Z, FColor C)
{
    FPTBrickKey Key; int32 lx, ly, lz;
    CellToBrick(X, Y, Z, Key, lx, ly, lz);
    FPTBrick& B = FindOrAddBrick(Key);
    B.Color[FPTBrick::LocalIdx(lx, ly, lz)] = C;
}

float FPTSculptField::SampleSDF(float X, float Y, float Z) const
{
    const int32 x0 = FMath::FloorToInt(X), y0 = FMath::FloorToInt(Y), z0 = FMath::FloorToInt(Z);
    const float fx = X - x0, fy = Y - y0, fz = Z - z0;
    auto V = [&](int32 xi, int32 yi, int32 zi) { return GetSDF(xi, yi, zi); };
    return FMath::Lerp(
        FMath::Lerp(FMath::Lerp(V(x0,y0,z0),   V(x0+1,y0,z0),   fx), FMath::Lerp(V(x0,y0+1,z0),   V(x0+1,y0+1,z0),   fx), fy),
        FMath::Lerp(FMath::Lerp(V(x0,y0,z0+1), V(x0+1,y0,z0+1), fx), FMath::Lerp(V(x0,y0+1,z0+1), V(x0+1,y0+1,z0+1), fx), fy),
        fz);
}

// ─── Snapshot ─────────────────────────────────────────────────────────────────
// Copia el brick con 1 celda de borde extra en el lado -/+ para poder mallar
// costuras con los vecinos. Rango de esquinas global: [b*BS-1 .. b*BS+BS].

void FPTSculptField::SnapshotBrick(const FPTBrickKey& Key, FBrickSnapshot& Out) const
{
    const int32 BS = FPTBrick::BrickSize;
    const int32 SS = BS + 2; // 18 muestras por eje (índices 0..17 → global b*BS-1 .. b*BS+16)

    Out.Key       = Key;
    Out.VoxelSize = VoxelSize;
    Out.SDF.SetNumUninitialized(SS * SS * SS);
    Out.Color.SetNumUninitialized(SS * SS * SS);

    const int32 baseX = Key.X * BS - 1;
    const int32 baseY = Key.Y * BS - 1;
    const int32 baseZ = Key.Z * BS - 1;

    bool bHasPos = false, bHasNeg = false;
    for (int32 k = 0; k < SS; ++k)
    for (int32 j = 0; j < SS; ++j)
    for (int32 i = 0; i < SS; ++i)
    {
        const float v = GetSDF(baseX + i, baseY + j, baseZ + k);
        const int32 idx = i + j * SS + k * SS * SS;
        Out.SDF[idx]   = v;
        Out.Color[idx] = GetColor(baseX + i, baseY + j, baseZ + k);
        if (v > 0.f) bHasPos = true; else bHasNeg = true;
    }
    Out.bEmpty = !(bHasPos && bHasNeg);
}

// ─── Surface Nets ─────────────────────────────────────────────────────────────
// Un vértice por celda que cruza la superficie (promedio de cruces de arista).
// Solo se emiten los quads del lado - (min): como los bricks comparten muestras
// globales, el vecino emite los del lado + con vértices idénticos → sin costuras
// ni duplicados.

void FPTSculptField::MeshBrick(const FBrickSnapshot& Snap, FPTBrickMesh& Out)
{
    Out.Section = Snap.Section;
    Out.Verts.Reset(); Out.Normals.Reset(); Out.Tris.Reset(); Out.Colors.Reset();
    if (Snap.bEmpty) return;

    const int32 BS = FPTBrick::BrickSize;
    const int32 SS = BS + 2;                 // stride de muestras (18)
    const int32 CN = BS + 1;                 // celdas por eje: lc 0..BS (17)
    const float Vx = Snap.VoxelSize;

    const int32 baseX = Snap.Key.X * BS - 1; // origen global de la muestra (0,0,0)
    const int32 baseY = Snap.Key.Y * BS - 1;
    const int32 baseZ = Snap.Key.Z * BS - 1;

    auto SDFAt = [&](int32 i, int32 j, int32 k) -> float {
        return Snap.SDF[i + j * SS + k * SS * SS];
    };
    auto ColAt = [&](int32 i, int32 j, int32 k) -> FColor {
        return Snap.Color[i + j * SS + k * SS * SS];
    };

    // Índice de vértice por celda (lc en [0,BS] cada eje). -1 = sin vértice.
    TArray<int32> VertIdx;
    VertIdx.Init(-1, CN * CN * CN);
    auto CellSlot = [&](int32 cx, int32 cy, int32 cz) -> int32& {
        return VertIdx[cx + cy * CN + cz * CN * CN];
    };

    // 12 aristas del cubo por índices de esquina 0..7 (x + 2y + 4z).
    static const int32 CornerX[8] = {0,1,0,1,0,1,0,1};
    static const int32 CornerY[8] = {0,0,1,1,0,0,1,1};
    static const int32 CornerZ[8] = {0,0,0,0,1,1,1,1};
    static const int32 EdgeA[12]  = {0,1,2,3, 0,1,4,5, 0,2,4,6};
    static const int32 EdgeB[12]  = {1,3,3,2, 4,5,5,7, 2,6,6,7};

    // 1) Generar vértices por celda.
    for (int32 cz = 0; cz < CN; ++cz)
    for (int32 cy = 0; cy < CN; ++cy)
    for (int32 cx = 0; cx < CN; ++cx)
    {
        float cv[8]; FColor cc[8];
        int32 mask = 0;
        for (int32 c = 0; c < 8; ++c)
        {
            cv[c] = SDFAt(cx + CornerX[c], cy + CornerY[c], cz + CornerZ[c]);
            cc[c] = ColAt(cx + CornerX[c], cy + CornerY[c], cz + CornerZ[c]);
            if (cv[c] > 0.f) mask |= (1 << c);
        }
        if (mask == 0 || mask == 0xFF) continue; // sin cruce

        FVector accumPos = FVector::ZeroVector;
        FVector4 accumCol(0,0,0,0);
        int32 crossings = 0;
        for (int32 e = 0; e < 12; ++e)
        {
            const int32 a = EdgeA[e], b = EdgeB[e];
            const bool sa = cv[a] > 0.f, sb = cv[b] > 0.f;
            if (sa == sb) continue;
            const float t = cv[a] / (cv[a] - cv[b]); // punto de cruce (SDF=0)
            const FVector pa(cx + CornerX[a], cy + CornerY[a], cz + CornerZ[a]);
            const FVector pb(cx + CornerX[b], cy + CornerY[b], cz + CornerZ[b]);
            accumPos += pa + (pb - pa) * t;
            const FColor ca = cc[a], cb = cc[b];
            accumCol += FVector4(
                FMath::Lerp((float)ca.R, (float)cb.R, t),
                FMath::Lerp((float)ca.G, (float)cb.G, t),
                FMath::Lerp((float)ca.B, (float)cb.B, t),
                FMath::Lerp((float)ca.A, (float)cb.A, t));
            ++crossings;
        }
        if (crossings == 0) continue;
        accumPos /= crossings;
        accumCol /= crossings;

        // Posición local (espacio del actor): (baseCell + offset) * VoxelSize.
        const FVector localPos((baseX + accumPos.X) * Vx,
                               (baseY + accumPos.Y) * Vx,
                               (baseZ + accumPos.Z) * Vx);

        // Normal = -gradiente del SDF (positivo=dentro → gradiente apunta hacia adentro).
        const FVector grad(
            (cv[1]+cv[3]+cv[5]+cv[7]) - (cv[0]+cv[2]+cv[4]+cv[6]),
            (cv[2]+cv[3]+cv[6]+cv[7]) - (cv[0]+cv[1]+cv[4]+cv[5]),
            (cv[4]+cv[5]+cv[6]+cv[7]) - (cv[0]+cv[1]+cv[2]+cv[3]));
        FVector N = (-grad).GetSafeNormal();
        if (N.IsNearlyZero()) N = FVector::UpVector;

        CellSlot(cx, cy, cz) = Out.Verts.Num();
        Out.Verts.Add(localPos);
        Out.Normals.Add(N);
        Out.Colors.Add(FColor(
            (uint8)FMath::Clamp(accumCol.X, 0.f, 255.f),
            (uint8)FMath::Clamp(accumCol.Y, 0.f, 255.f),
            (uint8)FMath::Clamp(accumCol.Z, 0.f, 255.f),
            (uint8)FMath::Clamp(accumCol.W, 0.f, 255.f)));
    }

    // 2) Emitir quads (solo celdas reales lc en [1,BS]). Para cada eje, si la
    //    arista en la esquina mínima cruza, conectar las 4 celdas que la comparten.
    auto EmitQuad = [&](int32 v0, int32 v1, int32 v2, int32 v3, bool flip)
    {
        if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) return;
        if (!flip)
        {
            Out.Tris.Add(v0); Out.Tris.Add(v1); Out.Tris.Add(v2);
            Out.Tris.Add(v0); Out.Tris.Add(v2); Out.Tris.Add(v3);
        }
        else
        {
            Out.Tris.Add(v0); Out.Tris.Add(v2); Out.Tris.Add(v1);
            Out.Tris.Add(v0); Out.Tris.Add(v3); Out.Tris.Add(v2);
        }
    };

    for (int32 cz = 1; cz <= BS; ++cz)
    for (int32 cy = 1; cy <= BS; ++cy)
    for (int32 cx = 1; cx <= BS; ++cx)
    {
        const float s000 = SDFAt(cx, cy, cz);
        const bool  in000 = s000 > 0.f;

        // Arista en X: esquina (cx,cy,cz) → (cx+1,cy,cz)
        {
            const bool in1 = SDFAt(cx + 1, cy, cz) > 0.f;
            if (in000 != in1)
                EmitQuad(CellSlot(cx, cy,   cz),
                         CellSlot(cx, cy-1, cz),
                         CellSlot(cx, cy-1, cz-1),
                         CellSlot(cx, cy,   cz-1), in000);
        }
        // Arista en Y: (cx,cy,cz) → (cx,cy+1,cz)
        {
            const bool in1 = SDFAt(cx, cy + 1, cz) > 0.f;
            if (in000 != in1)
                EmitQuad(CellSlot(cx,   cy, cz),
                         CellSlot(cx,   cy, cz-1),
                         CellSlot(cx-1, cy, cz-1),
                         CellSlot(cx-1, cy, cz), in000);
        }
        // Arista en Z: (cx,cy,cz) → (cx,cy,cz+1)
        {
            const bool in1 = SDFAt(cx, cy, cz + 1) > 0.f;
            if (in000 != in1)
                EmitQuad(CellSlot(cx,   cy,   cz),
                         CellSlot(cx-1, cy,   cz),
                         CellSlot(cx-1, cy-1, cz),
                         CellSlot(cx,   cy-1, cz), in000);
        }
    }
}
