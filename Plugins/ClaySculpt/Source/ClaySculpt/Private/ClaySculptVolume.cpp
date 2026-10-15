#include "ClaySculptVolume.h"

// ─── Constructor ─────────────────────────────────────────────────────────────
AClaySculptVolume::AClaySculptVolume()
{
	PrimaryActorTick.bCanEverTick = true;

	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetMobility(EComponentMobility::Movable);
	Mesh->SetCastShadow(true);
	Mesh->bUseAsyncCooking = true;

	// Caja que define el lienzo de esculpido. Visible en editor, oculta en juego.
	BoundsBox = CreateDefaultSubobject<UBoxComponent>(TEXT("BoundsBox"));
	BoundsBox->SetupAttachment(Mesh);
	BoundsBox->SetRelativeLocation(FVector::ZeroVector);
	BoundsBox->SetBoxExtent(FVector(480.f)); // lienzo por defecto (960 UU/lado)
	BoundsBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BoundsBox->ShapeColor = FColor(0, 200, 255);
	BoundsBox->bDrawOnlyIfSelected = false;
	BoundsBox->SetHiddenInGame(true);
}

void AClaySculptVolume::EnsureFieldInit()
{
	if (bFieldInit) return;
	Field.VoxelSize        = VoxelSize;
	Field.DisplaySmoothing = DisplaySmoothing;
	bFieldInit = true;
}

void AClaySculptVolume::BeginPlay()
{
	Super::BeginPlay();
	EnsureFieldInit();
}

void AClaySculptVolume::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!Field.HasDirty()) return;
	TimeSinceRebuild += DeltaTime;
	if (TimeSinceRebuild >= RebuildInterval)
	{
		TimeSinceRebuild = 0.f;
		RebuildDirty();
	}
}

// ─── Coordenadas / lienzo ─────────────────────────────────────────────────────
FVector AClaySculptVolume::WorldToCell(FVector W) const
{
	return GetActorTransform().InverseTransformPosition(W) / VoxelSize;
}

void AClaySculptVolume::CellBounds(FIntVector& OutMin, FIntVector& OutMax) const
{
	const FVector Center = BoundsBox ? BoundsBox->GetRelativeLocation() : FVector::ZeroVector;
	const FVector Half   = BoundsBox ? BoundsBox->GetUnscaledBoxExtent() : FVector(480.f);
	const FVector LMin = (Center - Half) / VoxelSize;
	const FVector LMax = (Center + Half) / VoxelSize;
	OutMin = FIntVector(FMath::FloorToInt(LMin.X), FMath::FloorToInt(LMin.Y), FMath::FloorToInt(LMin.Z));
	OutMax = FIntVector(FMath::CeilToInt(LMax.X),  FMath::CeilToInt(LMax.Y),  FMath::CeilToInt(LMax.Z));
}

float AClaySculptVolume::SampleWorldDensity(FVector WorldPos) const
{
	const FVector C = WorldToCell(WorldPos);
	return Field.SampleSDF(C.X, C.Y, C.Z);
}

// ─── SDF de las formas ────────────────────────────────────────────────────────
float AClaySculptVolume::StampSDF(EClayStampShape Shape, FVector P, float HalfSize)
{
	switch (Shape)
	{
	case EClayStampShape::Sphere:
		return HalfSize - P.Size();

	case EClayStampShape::Cube:
		return FMath::Min3(HalfSize - FMath::Abs(P.X),
		                   HalfSize - FMath::Abs(P.Y),
		                   HalfSize - FMath::Abs(P.Z));

	case EClayStampShape::Cylinder: // eje Z
	{
		const float radial = HalfSize - FVector2D(P.X, P.Y).Size();
		const float axial  = HalfSize - FMath::Abs(P.Z);
		return FMath::Min(radial, axial);
	}

	case EClayStampShape::Cone: // eje Z, punta arriba (+Z), base en -Z
	{
		const float h = HalfSize;
		const float rAllow = HalfSize * FMath::Clamp((h - P.Z) / (2.f * h), 0.f, 1.f);
		const float radial = rAllow - FVector2D(P.X, P.Y).Size();
		const float axial  = FMath::Min(P.Z + h, h - P.Z);
		return FMath::Min(radial, axial);
	}

	default:
		return HalfSize - P.Size();
	}
}

// ─── Aplicar sello ────────────────────────────────────────────────────────────
void AClaySculptVolume::ApplyStamp(FVector WorldPos, EClayStampShape Shape, float Size,
                                   EClayEditMode Mode, FLinearColor PaintColor)
{
	EnsureFieldInit();

	const FVector GC = WorldToCell(WorldPos);
	const float HalfSize = (Size * 0.5f) / VoxelSize; // radio en celdas
	const int32 R = FMath::CeilToInt(HalfSize) + 1;

	FIntVector BMin, BMax;
	CellBounds(BMin, BMax);
	const int32 x0 = FMath::Max(BMin.X, FMath::FloorToInt(GC.X) - R);
	const int32 y0 = FMath::Max(BMin.Y, FMath::FloorToInt(GC.Y) - R);
	const int32 z0 = FMath::Max(BMin.Z, FMath::FloorToInt(GC.Z) - R);
	const int32 x1 = FMath::Min(BMax.X, FMath::CeilToInt(GC.X) + R);
	const int32 y1 = FMath::Min(BMax.Y, FMath::CeilToInt(GC.Y) + R);
	const int32 z1 = FMath::Min(BMax.Z, FMath::CeilToInt(GC.Z) + R);

	bool bAnyChange = false;

	if (Mode == EClayEditMode::Smooth)
	{
		const int32 nx = x1 - x0 + 1, ny = y1 - y0 + 1, nz = z1 - z0 + 1;
		if (nx <= 0 || ny <= 0 || nz <= 0) return;
		TArray<float> NewV; NewV.SetNumUninitialized(nx * ny * nz);
		auto WI = [&](int32 x, int32 y, int32 z){ return (x-x0) + (y-y0)*nx + (z-z0)*nx*ny; };

		for (int32 z = z0; z <= z1; ++z)
		for (int32 y = y0; y <= y1; ++y)
		for (int32 x = x0; x <= x1; ++x)
		{
			const float prev = Field.GetSDF(x, y, z);
			const FVector LP(x - GC.X, y - GC.Y, z - GC.Z);
			const float sdf  = StampSDF(Shape, LP, HalfSize);
			const float fall = FMath::Clamp(sdf / FMath::Max(HalfSize, 1.f), 0.f, 1.f);
			if (fall <= 0.f) { NewV[WI(x,y,z)] = prev; continue; }

			const float avg = (Field.GetSDF(x+1,y,z) + Field.GetSDF(x-1,y,z)
			                 + Field.GetSDF(x,y+1,z) + Field.GetSDF(x,y-1,z)
			                 + Field.GetSDF(x,y,z+1) + Field.GetSDF(x,y,z-1)) / 6.f;
			const float target = avg + SmoothBias;
			float nv = FMath::Lerp(prev, target, fall * SmoothStrength);
			nv = prev + FMath::Clamp(nv - prev, -SmoothMaxDelta, SmoothMaxDelta);
			NewV[WI(x,y,z)] = nv;
		}

		for (int32 z = z0; z <= z1; ++z)
		for (int32 y = y0; y <= y1; ++y)
		for (int32 x = x0; x <= x1; ++x)
		{
			const float nv = NewV[WI(x,y,z)];
			if (nv != Field.GetSDF(x, y, z)) { Field.SetSDF(x, y, z, nv); bAnyChange = true; }
		}

		if (bAnyChange) MarkStampDirty(x0, y0, z0, x1, y1, z1);
		return;
	}

	for (int32 z = z0; z <= z1; ++z)
	for (int32 y = y0; y <= y1; ++y)
	for (int32 x = x0; x <= x1; ++x)
	{
		const FVector LP(x - GC.X, y - GC.Y, z - GC.Z);
		const float sdf  = StampSDF(Shape, LP, HalfSize);
		const float prev = Field.GetSDF(x, y, z);

		if (Mode == EClayEditMode::Add)
		{
			const float next = FMath::Max(prev, sdf);
			if (next != prev) { Field.SetSDF(x, y, z, next); bAnyChange = true; }
			// Color pleno directo sobre la geometría nueva (vertex color).
			if (sdf > -1.f)
			{
				Field.SetColor(x, y, z, PaintColor.ToFColor(true));
				bAnyChange = true;
			}
		}
		else // Erase
		{
			const float next = FMath::Min(prev, -sdf);
			if (next != prev) { Field.SetSDF(x, y, z, next); bAnyChange = true; }
		}
	}

	if (bAnyChange) MarkStampDirty(x0, y0, z0, x1, y1, z1);
}

void AClaySculptVolume::MarkStampDirty(int32 x0, int32 y0, int32 z0, int32 x1, int32 y1, int32 z1)
{
	const int32 BS = FClayBrick::BrickSize;
	auto FloorDivBS = [BS](int32 v){ return (v >= 0) ? v / BS : -(( -v + BS - 1) / BS); };
	const int32 bx0 = FloorDivBS(x0 - 1), bx1 = FloorDivBS(x1 + 1);
	const int32 by0 = FloorDivBS(y0 - 1), by1 = FloorDivBS(y1 + 1);
	const int32 bz0 = FloorDivBS(z0 - 1), bz1 = FloorDivBS(z1 + 1);
	for (int32 bz = bz0; bz <= bz1; ++bz)
	for (int32 by = by0; by <= by1; ++by)
	for (int32 bx = bx0; bx <= bx1; ++bx)
		Field.MarkDirty(FClayBrickKey(bx, by, bz));
}

// ─── Rebuild (síncrono; suficiente para modelar en editor) ────────────────────
void AClaySculptVolume::RebuildDirty()
{
	if (!Mesh) return;
	TArray<FClayBrickKey> Keys;
	Field.TakeDirty(Keys);
	for (const FClayBrickKey& K : Keys)
	{
		FClaySculptField::FBrickSnapshot S;
		S.Section = Field.SectionIndex(K);
		Field.SnapshotBrick(K, S);
		S.Step = Field.DecideStep(K);

		FClayBrickMesh M;
		FClaySculptField::MeshBrick(S, M);
		Mesh->CreateMeshSection(M.Section, M.Verts, M.Tris, M.Normals, {}, M.Colors, {}, /*collision=*/true);
		if (ClayMaterial) Mesh->SetMaterial(M.Section, ClayMaterial);
	}
}

void AClaySculptVolume::RebuildNow()
{
	RebuildDirty();
}

void AClaySculptVolume::ClearAll()
{
	Field = FClaySculptField();
	bFieldInit = false;
	EnsureFieldInit();
	if (Mesh) Mesh->ClearAllMeshSections();
	TimeSinceRebuild = 0.f;
}
