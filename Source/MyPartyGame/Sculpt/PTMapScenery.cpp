// Copyright Epic Games, Inc. All Rights Reserved.
#include "PTMapScenery.h"
#include "Components/SphereComponent.h"
#include "Async/Async.h"
#include "Misc/Compression.h"
#include "Misc/FileHelper.h"

APTMapScenery::APTMapScenery()
{
    PrimaryActorTick.bCanEverTick = false;
    // La geometría se transmite por el transporte de red (Fase 4), no por replicación de propiedades.
    bReplicates = false;

    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->bUseAsyncCooking = true;                    // cocinar colisión fuera del hilo del juego
    Mesh->SetCastShadow(true);
    Mesh->bCastDynamicShadow = true;
    // Escenario SÓLIDO: colisión de mundo que bloquea a los pawns (se posan/chocan al volar).
    Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Mesh->SetCollisionObjectType(ECC_WorldStatic);
    Mesh->SetCollisionResponseToAllChannels(ECR_Block);

    // Barrera invisible: esfera sólida que SOLO bloquea pawns → los contiene aunque el domo tenga
    // aberturas. (Precedente en Lvl-01: los BlockingVolume que delimitan el área ~±2500 UU.)
    Barrier = CreateDefaultSubobject<USphereComponent>(TEXT("Barrier"));
    Barrier->SetupAttachment(Mesh);
    Barrier->SetSphereRadius(PlayableRadius);
    Barrier->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    Barrier->SetCollisionObjectType(ECC_WorldStatic);
    Barrier->SetCollisionResponseToAllChannels(ECR_Ignore);
    Barrier->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    Barrier->SetHiddenInGame(true);
    Barrier->SetCastShadow(false);
}

void APTMapScenery::BeginPlay()
{
    Super::BeginPlay();
    // Iteración rápida en dev: cargar un archivo local si se configuró en el actor.
    if (!DevMapFilePath.IsEmpty())
        LoadFromFile(DevMapFilePath);
}

void APTMapScenery::SetPlayableRadius(float InRadius)
{
    PlayableRadius = FMath::Max(InRadius, 1.f);
    if (Barrier) Barrier->SetSphereRadius(PlayableRadius);
}

// ─── Contenedor del blob del mapa: [int32 rawSize][ zlib(bytes crudos) ] ─────────
bool APTMapScenery::EncodeMapBlob(const TArray<uint8>& RawOctreeBytes, TArray<uint8>& OutBlob)
{
    OutBlob.Reset();
    const int32 RawSize = RawOctreeBytes.Num();
    if (RawSize <= 0) return false;

    int32 CompBound = FCompression::CompressMemoryBound(NAME_Zlib, RawSize);
    TArray<uint8> Comp; Comp.SetNumUninitialized(CompBound);
    int32 CompSize = CompBound;
    if (!FCompression::CompressMemory(NAME_Zlib, Comp.GetData(), CompSize, RawOctreeBytes.GetData(), RawSize))
        return false;
    Comp.SetNum(CompSize);

    OutBlob.Append((const uint8*)&RawSize, sizeof(int32));
    OutBlob.Append(Comp);
    return true;
}

bool APTMapScenery::DecodeMapBlob(const TArray<uint8>& Blob, TArray<uint8>& OutRawOctreeBytes)
{
    OutRawOctreeBytes.Reset();
    if (Blob.Num() <= (int32)sizeof(int32)) return false;

    int32 RawSize = 0;
    FMemory::Memcpy(&RawSize, Blob.GetData(), sizeof(int32));
    if (RawSize <= 0) return false;

    OutRawOctreeBytes.SetNumUninitialized(RawSize);
    if (!FCompression::UncompressMemory(NAME_Zlib, OutRawOctreeBytes.GetData(), RawSize,
            Blob.GetData() + sizeof(int32), Blob.Num() - sizeof(int32)))
    {
        OutRawOctreeBytes.Reset();
        return false;
    }
    return true;
}

// ─── Carga ───────────────────────────────────────────────────────────────────
bool APTMapScenery::LoadFromMapBlob(const TArray<uint8>& Blob)
{
    TArray<uint8> Raw;
    if (!DecodeMapBlob(Blob, Raw)) return false;
    return LoadFromOctreeBytes(Raw);
}

bool APTMapScenery::LoadFromOctreeBytes(const TArray<uint8>& RawOctreeBytes)
{
    if (!SVOField.LoadFromBytes(RawOctreeBytes)) return false;
    bReady = false;
    RebuildMesh();
    return true;
}

bool APTMapScenery::LoadFromFile(const FString& AbsPath)
{
    TArray<uint8> Blob;
    if (!FFileHelper::LoadFileToArray(Blob, *AbsPath)) return false;
    return LoadFromMapBlob(Blob);
}

// ─── Mallado (async, colisión cocinada una sola vez) ────────────────────────────
void APTMapScenery::RebuildMesh()
{
    if (!Mesh || !SVOField.IsInit()) return;
    if (bMeshing) return; // ya hay uno en vuelo; el resultado válido lo decide MeshGen

    SVOField.Balance(); // 2:1 antes de mallar (como el volumen de gameplay)

    // Clon completo para mallar en el hilo de fondo sin tocar el octree vivo (mismo patrón que
    // APTSculptVolume::RebuildSVOMesh, sin glow ni capas de detalle).
    const FBox Whole(SVOField.GetOrigin() - FVector(1.f),
                     SVOField.GetOrigin() + FVector(SVOField.GetRootSize() + 1.f));
    TSharedPtr<FPTVoxelOctree> Clone = SVOField.CloneRegion(Whole);
    if (!Clone.IsValid()) return;

    bMeshing = true;
    const uint32 Gen = ++MeshGen;
    TWeakObjectPtr<APTMapScenery> WeakThis(this);

    Async(EAsyncExecution::ThreadPool, [WeakThis, Clone, Gen]()
    {
        struct FRes { TArray<FVector> V, N; TArray<int32> T; TArray<FColor> C; };
        TSharedPtr<FRes, ESPMode::ThreadSafe> R = MakeShared<FRes, ESPMode::ThreadSafe>();
        Clone->BuildMeshMC(R->V, R->T, R->N, R->C); // Marching Cubes uniforme = watertight

        AsyncTask(ENamedThreads::GameThread, [WeakThis, R, Gen]()
        {
            APTMapScenery* Self = WeakThis.Get();
            if (!Self) return;
            Self->bMeshing = false;
            if (Gen != Self->MeshGen || !Self->Mesh) return; // resultado viejo → descartar

            if (R->V.Num() == 0)
            {
                Self->Mesh->ClearMeshSection(0);
            }
            else
            {
                const TArray<FVector2D> NoUV;
                const TArray<FProcMeshTangent> NoTan;
                // collision=true → cocina la malla de colisión (una sola vez; el escenario es estático).
                Self->Mesh->CreateMeshSection(0, R->V, R->T, R->N, NoUV, R->C, NoTan, /*collision=*/true);
                if (Self->SceneryMaterial) Self->Mesh->SetMaterial(0, Self->SceneryMaterial);
            }

            // NOTA (loading gate, Fase 4): con bUseAsyncCooking la malla de colisión termina de
            // cocinarse uno o dos frames después; bReady/broadcast marcan "malla lista". Como el gate
            // espera a TODOS (segundos), la colisión ya está lista para el primer turno. Si se quisiera
            // que bReady implique colisión 100% cocinada, poner bUseAsyncCooking=false (a costa de un hitch).
            Self->bReady = true;
            Self->OnSceneryReady.Broadcast(Self);
        });
    });
}
