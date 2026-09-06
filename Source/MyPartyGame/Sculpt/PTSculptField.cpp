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
    // Undo: guardar cómo estaba este brick ANTES de que el trazo lo toque (solo la 1ra vez).
    BackupBrick(Key);

    TSharedPtr<FPTBrick>& Slot = Bricks.FindOrAdd(Key);
    if (!Slot.IsValid()) Slot = MakeShared<FPTBrick>();
    return *Slot;
}

// ─── Undo por trazo ──────────────────────────────────────────────────────────

void FPTSculptField::BackupBrick(const FPTBrickKey& Key)
{
    if (!bRecording) return;
    if (CurrentStroke.Bricks.Contains(Key)) return; // ya respaldado en este trazo

    if (const TSharedPtr<FPTBrick>* Existing = Bricks.Find(Key))
        CurrentStroke.Bricks.Add(Key, MakeShared<FPTBrick>(**Existing)); // copia del contenido
    else
        CurrentStroke.Bricks.Add(Key, nullptr); // no existía → al deshacer se borra
}

void FPTSculptField::BeginStroke()
{
    CurrentStroke.Bricks.Reset();
    bRecording = true;
}

void FPTSculptField::PushStroke()
{
    bRecording = false;
    // Se apila SIEMPRE (aunque el trazo no haya tocado geometría, ej: solo pintura u ojos): el
    // volumen lleva su propia pila en paralelo y las dos tienen que quedar 1:1 para deshacer juntas.
    UndoStack.Add(MoveTemp(CurrentStroke));
    CurrentStroke.Bricks.Reset();
    // Tope de niveles: tirar el más viejo (la memoria es ∝ a lo que tocó cada trazo).
    while (UndoStack.Num() > MaxUndoSteps) UndoStack.RemoveAt(0);
}

bool FPTSculptField::UndoStroke()
{
    if (UndoStack.Num() == 0) return false;

    FStrokeBackup S = MoveTemp(UndoStack.Last());
    UndoStack.Pop();

    for (const auto& It : S.Bricks)
    {
        if (It.Value.IsValid()) Bricks.Add(It.Key, It.Value); // restaurar contenido previo
        else                    Bricks.Remove(It.Key);        // no existía antes → sacarlo

        // Marcar el brick y sus vecinos: los bordes comparten muestras, si no quedan costuras.
        for (int32 dz = -1; dz <= 1; ++dz)
        for (int32 dy = -1; dy <= 1; ++dy)
        for (int32 dx = -1; dx <= 1; ++dx)
            MarkDirty(FPTBrickKey(It.Key.X + dx, It.Key.Y + dy, It.Key.Z + dz));
    }
    return true;
}

void FPTSculptField::ClearUndo()
{
    UndoStack.Reset();
    CurrentStroke.Bricks.Reset();
    bRecording = false;
}

void FPTSculptField::SerializeState(FArchive& Ar)
{
    int32 Version = 1;
    Ar << Version;

    if (Ar.IsLoading())
    {
        // Estado limpio antes de cargar: sin bricks, sin secciones ni undo previos.
        Bricks.Reset();
        DirtyBricks.Reset();
        SectionOf.Reset();
        Flatness.Reset();
        NextSection = 0;
        ClearUndo();

        int32 Count = 0;
        Ar << Count;
        for (int32 i = 0; i < Count; ++i)
        {
            FPTBrickKey Key;
            Ar << Key;
            TSharedPtr<FPTBrick> B = MakeShared<FPTBrick>();
            Ar << B->SDF;    // TArray<float>
            Ar << B->Color;  // TArray<FColor>
            // AddTime queda en su init (-1e9 = "vieja", no brilla).
            Bricks.Add(Key, B);
            DirtyBricks.Add(Key); // marcar para re-mallar
        }
    }
    else
    {
        int32 Count = Bricks.Num();
        Ar << Count;
        for (TPair<FPTBrickKey, TSharedPtr<FPTBrick>>& It : Bricks)
        {
            FPTBrickKey Key = It.Key;
            Ar << Key;
            Ar << It.Value->SDF;
            Ar << It.Value->Color;
        }
    }
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

float FPTSculptField::GetAddTime(int32 X, int32 Y, int32 Z) const
{
    FPTBrickKey Key; int32 lx, ly, lz;
    CellToBrick(X, Y, Z, Key, lx, ly, lz);
    if (const FPTBrick* B = FindBrick(Key))
        return B->AddTime[FPTBrick::LocalIdx(lx, ly, lz)];
    return -1.e9f;
}

void FPTSculptField::SetAddTime(int32 X, int32 Y, int32 Z, float T)
{
    FPTBrickKey Key; int32 lx, ly, lz;
    CellToBrick(X, Y, Z, Key, lx, ly, lz);
    FPTBrick& B = FindOrAddBrick(Key);
    B.AddTime[FPTBrick::LocalIdx(lx, ly, lz)] = T;
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

static constexpr int32 SNMaxStep = 4;          // paso de mallado más grueso permitido (1=fino..4=muy grueso)
static constexpr int32 SNMargin  = SNMaxStep;  // borde fantasma en celdas finas (= MaxStep)
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
    Out.AddTime.SetNumUninitialized(SS * SS * SS);

    const int32 baseX = Key.X * BS - SNMargin;
    const int32 baseY = Key.Y * BS - SNMargin;
    const int32 baseZ = Key.Z * BS - SNMargin;

    bool bHasPos = false, bHasNeg = false;
    for (int32 k = 0; k < SS; ++k)
    for (int32 j = 0; j < SS; ++j)
    for (int32 i = 0; i < SS; ++i)
    {
        const int32 gx = baseX + i, gy = baseY + j, gz = baseZ + k;
        float v = GetSDF(gx, gy, gz);

        // Suavizado de display: mezcla con el promedio de vecinos (lectura GLOBAL
        // → consistente entre bricks, sin costuras). No modifica los datos guardados.
        if (DisplaySmoothing > 0.f)
        {
            const float avg = (GetSDF(gx+1,gy,gz) + GetSDF(gx-1,gy,gz)
                             + GetSDF(gx,gy+1,gz) + GetSDF(gx,gy-1,gz)
                             + GetSDF(gx,gy,gz+1) + GetSDF(gx,gy,gz-1)) / 6.f;
            v = FMath::Lerp(v, avg, DisplaySmoothing);
        }

        const int32 idx = i + j * SS + k * SS * SS;
        Out.SDF[idx]     = v;
        Out.Color[idx]   = GetColor(gx, gy, gz);
        Out.AddTime[idx] = GetAddTime(gx, gy, gz);
        if (v > 0.f) bHasPos = true; else bHasNeg = true;
    }
    Out.bEmpty = !(bHasPos && bHasNeg);

    // Cache de flatness para decidir el paso (vacío = liso, no fuerza vecinos a fino).
    Flatness.Add(Key, Out.bEmpty ? 1.f : BrickFlatness(Out, SS));
}

int32 FPTSculptField::DecideStep(const FPTBrickKey& Key) const
{
    if (SNMaxStep < 2) return 1;
    // Paso OBJETIVO de cada brick: brocha grande → BigBrushStep (configurable); liso → 2; detalle → 1.
    auto Target = [&](const FPTBrickKey& K) -> int32 {
        if (CoarseBricks.Contains(K)) return FMath::Clamp(BigBrushStep, 1, SNMaxStep); // brocha grande
        const float* p = Flatness.Find(K);
        return ((p ? *p : 1.f) > SNFlatThreshold) ? 2 : 1; // liso=2, detalle=1
    };
    int32 s = Target(Key);
    if (s <= 1) return 1;
    // Constraint de vecinos: el paso es el MÍNIMO del brick y sus 6 vecinos (así el borde con zonas
    // finas baja de a poco y se minimizan costuras). A pasos altos igual puede notarse un pelín el borde.
    static const FIntVector N6[6] = {
        {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1} };
    for (const FIntVector& d : N6)
        s = FMath::Min(s, Target(Key + d));
    s = FMath::Clamp(s, 1, SNMaxStep);
    // BrickSize=16 solo malla bien en pasos DIVISORES (1,2,4); 3 caería a fino. Redondear hacia abajo.
    if (s >= 4) return 4;
    if (s >= 2) return 2;
    return 1;
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
    Out.Verts.Reset(); Out.Normals.Reset(); Out.Tris.Reset(); Out.Colors.Reset(); Out.UV0.Reset();
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
    auto TimeAt = [&](int32 kx, int32 ky, int32 kz) -> float {
        const int32 i = FineIdx(kx), j = FineIdx(ky), k = FineIdx(kz);
        return Snap.AddTime[i + j * SS + k * SS * SS];
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
        float cv[8]; FColor cc[8]; float ct[8];
        int32 mask = 0;
        for (int32 c = 0; c < 8; ++c)
        {
            cv[c] = SDFAt(cx + GCornerX[c], cy + GCornerY[c], cz + GCornerZ[c]);
            cc[c] = ColAt(cx + GCornerX[c], cy + GCornerY[c], cz + GCornerZ[c]);
            ct[c] = TimeAt(cx + GCornerX[c], cy + GCornerY[c], cz + GCornerZ[c]);
            if (cv[c] > 0.f) mask |= (1 << c);
        }
        if (mask == 0 || mask == 0xFF) continue;

        FVector4 accumCol(0,0,0,0);
        FVector  accumOff = FVector::ZeroVector; // offset dentro de la celda [0,1]
        float    accumTime = -1.e9f;             // el MÁS reciente de los cruces (lo nuevo manda)
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
            // Frescura: se queda con el instante MÁS reciente de las esquinas del cruce, para que
            // el vértice brille si CUALQUIER parte de él es arcilla nueva.
            accumTime = FMath::Max3(accumTime, ct[a], ct[b]);
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
        // accumCol son bytes sRGB (el field guarda el color con ToFColor(true)). El material
        // lee el Vertex Color como LINEAL, así que se convierte sRGB→lineal; si no, el color
        // sale más claro que el elegido (el atlas de Paint sí se decodifica por ser textura SRGB).
        const FColor SRGBCol(
            (uint8)FMath::Clamp(accumCol.X, 0.f, 255.f),
            (uint8)FMath::Clamp(accumCol.Y, 0.f, 255.f),
            (uint8)FMath::Clamp(accumCol.Z, 0.f, 255.f),
            (uint8)FMath::Clamp(accumCol.W, 0.f, 255.f));
        Out.Colors.Add(FLinearColor(SRGBCol).ToFColor(false));
        // UV0.x = instante de agregado del vértice → el material lo desvanece (brillo de lo nuevo).
        Out.UV0.Add(FVector2D(accumTime, 0.f));
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
}
