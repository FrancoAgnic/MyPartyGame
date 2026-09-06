// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSVOTest.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"

TWeakObjectPtr<APTSVOTest> APTSVOTest::Instance;

// ── Comandos de consola (operan sobre la instancia activa) ───────────────────────
static FAutoConsoleCommand GCmdSVODemo(
    TEXT("PTSVO.Demo"), TEXT("Re-esculpe el demo del octree adaptativo."),
    FConsoleCommandDelegate::CreateLambda([] { if (APTSVOTest* A = APTSVOTest::Instance.Get()) A->RunDemo(); }));

static FAutoConsoleCommand GCmdSVOClear(
    TEXT("PTSVO.Clear"), TEXT("Limpia el octree de prueba."),
    FConsoleCommandDelegate::CreateLambda([] { if (APTSVOTest* A = APTSVOTest::Instance.Get()) A->ClearAll(); }));

static FAutoConsoleCommand GCmdSVOStats(
    TEXT("PTSVO.Stats"), TEXT("Imprime hojas/nodos/verts/tris del octree de prueba."),
    FConsoleCommandDelegate::CreateLambda([] { if (APTSVOTest* A = APTSVOTest::Instance.Get()) A->Rebuild(); }));

static FAutoConsoleCommand GCmdSVOAdd(
    TEXT("PTSVO.Add"), TEXT("PTSVO.Add <radio> — agrega una esfera de ese radio en el centro."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        APTSVOTest* A = APTSVOTest::Instance.Get();
        if (!A) return;
        const float R = (Args.Num() > 0) ? FCString::Atof(*Args[0]) : 60.f;
        A->AddSphere(R);
    }));

// ── Actor ────────────────────────────────────────────────────────────────────────
APTSVOTest::APTSVOTest()
{
    PrimaryActorTick.bCanEverTick = false;
    Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SVOMesh"));
    RootComponent = Mesh;
    Mesh->bUseAsyncCooking = true;
}

void APTSVOTest::BeginPlay()
{
    Super::BeginPlay();
    Instance = this;
    EnsureInit();
    RunDemo();
}

void APTSVOTest::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Instance.Get() == this) Instance = nullptr;
    Super::EndPlay(Reason);
}

void APTSVOTest::EnsureInit()
{
    // Octree centrado en el actor: origin = -RootSize/2 → el centro (0,0,0) local cae en el medio.
    Octree.Init(FVector(-RootSize * 0.5f), RootSize, MaxDepth);
}

void APTSVOTest::ClearAll()
{
    EnsureInit();
    if (Mesh) Mesh->ClearAllMeshSections();
    UE_LOG(LogTemp, Log, TEXT("[SVO] Clear."));
}

void APTSVOTest::RunDemo()
{
    EnsureInit();
    // Esfera GRANDE (se mallar grueso → pocos tris).
    Octree.EditSphere(FVector(0, 0, 0), RootSize * 0.16f, /*bAdd=*/true);
    // Varios detalles CHICOS encima (se subdividen fino → mucha malla local). Muestra lo adaptativo.
    const float BigR = RootSize * 0.16f;
    const float SmallR = RootSize * 0.02f;
    for (int32 i = 0; i < 8; ++i)
    {
        const float a = (float)i / 8.f * 2.f * PI;
        const FVector P(FMath::Cos(a) * BigR * 0.9f, FMath::Sin(a) * BigR * 0.9f, BigR * 0.4f);
        Octree.EditSphere(P, SmallR, /*bAdd=*/true);
    }
    Rebuild();
}

void APTSVOTest::AddSphere(float Radius)
{
    EnsureInit(); // no re-init si ya está; EnsureInit re-crea → para "Add" acumulativo mejor no re-init.
    Octree.EditSphere(FVector(0, 0, 0), Radius, /*bAdd=*/true);
    Rebuild();
}

void APTSVOTest::Rebuild()
{
    if (!Mesh) return;
    TArray<FVector> V, N;
    TArray<int32>   T;
    Octree.BuildMesh(V, T, N);

    TArray<FVector2D> UV;
    TArray<FColor>    C;
    TArray<FProcMeshTangent> Tan;
    Mesh->CreateMeshSection(0, V, T, N, UV, C, Tan, /*bCreateCollision=*/false);
    if (Material) Mesh->SetMaterial(0, Material);

    const int32 Tris = T.Num() / 3;
    UE_LOG(LogTemp, Log, TEXT("[SVO] hojas=%d nodos=%d verts=%d tris=%d"),
           Octree.CountLeaves(), Octree.CountNodes(), V.Num(), Tris);
    if (GEngine)
        GEngine->AddOnScreenDebugMessage(-1, 6.f, FColor::Cyan,
            FString::Printf(TEXT("[SVO] hojas=%d  verts=%d  tris=%d"), Octree.CountLeaves(), V.Num(), Tris));
}
