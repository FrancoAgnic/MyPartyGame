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
// Copia el brick con SNMargin celdas de borde extra en el lado - para poder
// mallar a paso grueso (hasta MaxStep) y las costuras con vecinos. Rango de
// esquinas global: [b*BS - SNMargin .. b*BS + BS].
//
// SNMargin = MaxStep para que el muestreo grueso tenga su celda fantasma.

static constexpr int32 SNMaxStep = 2;          // paso de mallado más grueso (zonas lisas)
static constexpr int32 SNMargin  = SNMaxStep;  // borde fantasma en celdas finas
static constexpr float SNFlatThreshold = 0.985f;

static float BrickFlatness(const FPTSculptField::FBrickSnapshot& Snap, int32 SS);

void FPTSculptField::SnapshotBrick(const FPTBrickKey& Key, FBrickSnapshot& Out)
{
    const int32 BS = FPTBrick::BrickSize;
    const int32 SS = BS + SNMargin + 1; // muestras por eje (índices 0..BS+SNMargin)

    Out.Key       = Key;
    Out.VoxelSize = VoxelSize;
    Out.SDF.SetNumUninitialized(SS * SS * SS);
    Out.Color.SetNumUninitialized(SS * SS * SS);

    const int32 baseX = Key.X * BS - SNMargin;
    const int32 baseY = Key.Y * BS - SNMargin;
    const int32 baseZ = Key.Z * BS - SNMargin;

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

    // Cache de flatness para decidir el paso (vacío = liso, no fuerza vecinos a fino).
    Flatness.Add(Key, Out.bEmpty ? 1.f : BrickFlatness(Out, SS));
}

int32 FPTSculptField::DecideStep(const FPTBrickKey& Key) const
{
    if (SNMaxStep < 2) return 1;
    auto F = [&](const FPTBrickKey& K) -> float {
        const float* p = Flatness.Find(K);
        return p ? *p : 1.f; // desconocido/vacío = liso
    };
    if (F(Key) <= SNFlatThreshold) return 1;
    static const FIntVector N6[6] = {
        {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
    for (const FIntVector& d : N6)
        if (F(Key + d) <= SNFlatThreshold) return 1;
    return 2;
}

// ─── Surface Nets ─────────────────────────────────────────────────────────────
// Un vértice por celda que cruza la superficie (promedio de cruces de arista).
// Solo se emiten los quads del lado - (min): como los bricks comparten muestras
// globales, el vecino emite los del lado + con vértices idénticos → sin costuras
// ni duplicados.

// Tablas de esquinas/aristas del cubo unitario (índice de esquina = x + 2y + 4z).
static const int32 GCornerX[8] = {0,1,0,1,0,1,0,1};
static const int32 GCornerY[8] = {0,0,1,1,0,0,1,1};
static const int32 GCornerZ[8] = {0,0,0,0,1,1,1,1};
static const int32 GEdgeA[12]  = {0,1,2,3, 0,1,4,5, 0,2,4,6};
static const int32 GEdgeB[12]  = {1,3,3,2, 4,5,5,7, 2,6,6,7};

// Coherencia de normales del brick a resolución fina: 1 = superficie muy lisa,
// →0 = superficie con mucho detalle/curvatura. Decide el paso de mallado.
static float BrickFlatness(const FPTSculptField::FBrickSnapshot& Snap, int32 SS)
{
    const int32 BS = FPTBrick::BrickSize;
    auto S = [&](int32 i, int32 j, int32 k){ return Snap.SDF[i + j*SS + k*SS*SS]; };

    FVector sum = FVector::ZeroVector;
    int32 count = 0;
    // Celdas finas propias del brick: min-corner en [SNMargin, SNMargin+BS).
    for (int32 cz = SNMargin; cz < SNMargin + BS; ++cz)
    for (int32 cy = SNMargin; cy < SNMargin + BS; ++cy)
    for (int32 cx = SNMargin; cx < SNMargin + BS; ++cx)
    {
        float cv[8]; int32 mask = 0;
        for (int32 c = 0; c < 8; ++c)
        {
            cv[c] = S(cx + GCornerX[c], cy + GCornerY[c], cz + GCornerZ[c]);
            if (cv[c] > 0.f) mask |= (1 << c);
        }
        if (mask == 0 || mask == 0xFF) continue;
        const FVector grad(
            (cv[1]+cv[3]+cv[5]+cv[7]) - (cv[0]+cv[2]+cv[4]+cv[6]),
            (cv[2]+cv[3]+cv[6]+cv[7]) - (cv[0]+cv[1]+cv[4]+cv[5]),
            (cv[4]+cv[5]+cv[6]+cv[7]) - (cv[0]+cv[1]+cv[2]+cv[3]));
        sum += (-grad).GetSafeNormal();
        ++count;
    }
    if (count == 0) return 0.f;
    return sum.Size() / count; // |Σn|/N ∈ [0,1]
}

void FPTSculptField::MeshBrick(const FBrickSnapshot& Snap, FPTBrickMesh& Out)
{
    Out.Section = Snap.Section;
    Out.Verts.Reset(); Out.Normals.Reset(); Out.Tris.Reset(); Out.Colors.Reset();
    if (Snap.bEmpty) return;

    const int32 BS = FPTBrick::BrickSize;
    const int32 SS = BS + SNMargin + 1; // stride de muestras finas
    const float Vx = Snap.VoxelSize;

    // Paso decidido en el GameThread con info de vecinos (evita costuras LOD).
    int32 step = FMath::Clamp(Snap.Step, 1, SNMaxStep);
    if (BS % step != 0) step = 1;

    const int32 CPA = BS / step;      // celdas gruesas por eje
    const int32 CN  = CPA + 1;        // + celda fantasma en el lado -
    // origen global de la esquina fina (0,0,0) del snapshot:
    const int32 gBaseX = Snap.Key.X * BS - SNMargin;
    const int32 gBaseY = Snap.Key.Y * BS - SNMargin;
    const int32 gBaseZ = Snap.Key.Z * BS - SNMargin;

    // Muestra en coordenada de esquina GRUESA (kx en [0,CPA]).
    // Esquina gruesa kx → esquina fina = (SNMargin - step) + kx*step.
    auto FineIdx = [&](int32 k){ return (SNMargin - step) + k * step; };
    auto SDFAt = [&](int32 kx, int32 ky, int32 kz) -> float {
        const int32 i = FineIdx(kx), j = FineIdx(ky), k = FineIdx(kz);
        return Snap.SDF[i + j * SS + k * SS * SS];
    };
    auto ColAt = [&](int32 kx, int32 ky, int32 kz) -> FColor {
        const int32 i = FineIdx(kx), j = FineIdx(ky), k = FineIdx(kz);
        return Snap.Color[i + j * SS + k * SS * SS];
    };
    // Coord fina (float) de una esquina gruesa, para posicionar vértices.
    auto FineCoordX = [&](float k){ return gBaseX + FineIdx(0) + k * step; };
    auto FineCoordY = [&](float k){ return gBaseY + FineIdx(0) + k * step; };
    auto FineCoordZ = [&](float k){ return gBaseZ + FineIdx(0) + k * step; };

    TArray<int32> VertIdx;
    VertIdx.Init(-1, CN * CN * CN);
    auto CellSlot = [&](int32 cx, int32 cy, int32 cz) -> int32& {
        return VertIdx[cx + cy * CN + cz * CN * CN];
    };

    // 1) Vértice por celda gruesa (incluye celda fantasma cc=0).
    for (int32 cz = 0; cz < CN; ++cz)
    for (int32 cy = 0; cy < CN; ++cy)
    for (int32 cx = 0; cx < CN; ++cx)
    {
        float cv[8]; FColor cc[8];
        int32 mask = 0;
        for (int32 c = 0; c < 8; ++c)
        {
            cv[c] = SDFAt(cx + GCornerX[c], cy + GCornerY[c], cz + GCornerZ[c]);
            cc[c] = ColAt(cx + GCornerX[c], cy + GCornerY[c], cz + GCornerZ[c]);
            if (cv[c] > 0.f) mask |= (1 << c);
        }
        if (mask == 0 || mask == 0xFF) continue;

        FVector4 accumCol(0,0,0,0);
        FVector  accumOff = FVector::ZeroVector; // offset dentro de la celda [0,1]
        int32 crossings = 0;
        for (int32 e = 0; e < 12; ++e)
        {
            const int32 a = GEdgeA[e], b = GEdgeB[e];
            if ((cv[a] > 0.f) == (cv[b] > 0.f)) continue;
            const float t = cv[a] / (cv[a] - cv[b]);
            const FVector pa(GCornerX[a], GCornerY[a], GCornerZ[a]);
            const FVector pb(GCornerX[b], GCornerY[b], GCornerZ[b]);
            accumOff += pa + (pb - pa) * t;
            const FColor ca = cc[a], cb = cc[b];
            accumCol += FVector4(
                FMath::Lerp((float)ca.R, (float)cb.R, t),
                FMath::Lerp((float)ca.G, (float)cb.G, t),
                FMath::Lerp((float)ca.B, (float)cb.B, t),
                FMath::Lerp((float)ca.A, (float)cb.A, t));
            ++crossings;
        }
        if (crossings == 0) continue;
        accumOff /= crossings;
        accumCol /= crossings;

        // Posición local: coord fina de la celda + offset*step, × VoxelSize.
        const FVector localPos(
            FineCoordX(cx + accumOff.X) * Vx,
            FineCoordY(cy + accumOff.Y) * Vx,
            FineCoordZ(cz + accumOff.Z) * Vx);

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

    // 2) Quads: solo celdas reales cc en [1,CPA]. Lado - (min) para dedupe con vecinos.
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

    for (int32 cz = 1; cz <= CPA; ++cz)
    for (int32 cy = 1; cy <= CPA; ++cy)
    for (int32 cx = 1; cx <= CPA; ++cx)
    {
        const bool in000 = SDFAt(cx, cy, cz) > 0.f;
        if (in000 != (SDFAt(cx + 1, cy, cz) > 0.f))
            EmitQuad(CellSlot(cx, cy,   cz), CellSlot(cx, cy-1, cz),
                     CellSlot(cx, cy-1, cz-1), CellSlot(cx, cy, cz-1), in000);
        if (in000 != (SDFAt(cx, cy + 1, cz) > 0.f))
            EmitQuad(CellSlot(cx, cy, cz), CellSlot(cx, cy, cz-1),
                     CellSlot(cx-1, cy, cz-1), CellSlot(cx-1, cy, cz), in000);
        if (in000 != (SDFAt(cx, cy, cz + 1) > 0.f))
            EmitQuad(CellSlot(cx, cy, cz), CellSlot(cx-1, cy, cz),
                     CellSlot(cx-1, cy-1, cz), CellSlot(cx, cy-1, cz), in000);
    }

    // 3) Skirts: cortina hacia adentro en cada arista de borde ABIERTA (una que
    //    pertenece a un solo triángulo = perímetro del parche del brick). Tapa
    //    las costuras entre bricks de distinto paso. En zonas uniformes queda
    //    detrás de la superficie coincidente del vecino → invisible.
    {
        TMap<uint64, int32> EdgeCount;
        EdgeCount.Reserve(Out.Tris.Num());
        auto AddEdge = [&](int32 a, int32 b) {
            const uint32 lo = (uint32)FMath::Min(a, b);
            const uint32 hi = (uint32)FMath::Max(a, b);
            EdgeCount.FindOrAdd(((uint64)lo << 32) | hi)++;
        };
        for (int32 t = 0; t + 2 < Out.Tris.Num(); t += 3)
        {
            AddEdge(Out.Tris[t],   Out.Tris[t+1]);
            AddEdge(Out.Tris[t+1], Out.Tris[t+2]);
            AddEdge(Out.Tris[t+2], Out.Tris[t]);
        }

        const float depth = Vx * step; // ~1 voxel del paso actual
        TArray<TPair<int32,int32>> Border;
        for (const TPair<uint64,int32>& KV : EdgeCount)
            if (KV.Value == 1)
                Border.Emplace((int32)(KV.Key >> 32), (int32)(KV.Key & 0xffffffffu));

        for (const TPair<int32,int32>& E : Border)
        {
            const int32 a = E.Key, b = E.Value;
            // Copiar a locales ANTES de Add (Add puede reubicar el array y
            // pasar una referencia a un elemento propio dispara el check de aliasing).
            const FVector pa = Out.Verts[a],   pb = Out.Verts[b];
            const FVector na = Out.Normals[a], nb = Out.Normals[b];
            const FColor  ca = Out.Colors[a],  cb = Out.Colors[b];

            const int32 a2 = Out.Verts.Num();
            Out.Verts.Add(pa - na * depth);
            Out.Normals.Add(na); Out.Colors.Add(ca);
            const int32 b2 = Out.Verts.Num();
            Out.Verts.Add(pb - nb * depth);
            Out.Normals.Add(nb); Out.Colors.Add(cb);

            // Quad (a,b,b2,a2) a doble cara para no depender del winding.
            Out.Tris.Add(a); Out.Tris.Add(b);  Out.Tris.Add(b2);
            Out.Tris.Add(a); Out.Tris.Add(b2); Out.Tris.Add(a2);
            Out.Tris.Add(a); Out.Tris.Add(b2); Out.Tris.Add(b);
            Out.Tris.Add(a); Out.Tris.Add(a2); Out.Tris.Add(b2);
        }
    }
}
