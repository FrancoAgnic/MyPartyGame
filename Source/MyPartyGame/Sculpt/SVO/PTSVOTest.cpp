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

static FAutoConsoleCommand GCmdSVOUndo(
    TEXT("PTSVO.Undo"), TEXT("Deshace el último PTSVO.Add."),
    FConsoleCommandDelegate::CreateLambda([] { if (APTSVOTest* A = APTSVOTest::Instance.Get()) A->Undo(); }));

static FAutoConsoleCommand GCmdSVOColor(
    TEXT("PTSVO.Color"), TEXT("PTSVO.Color <R> <G> <B> (0-255) — color de la arcilla para lo que agregues."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        APTSVOTest* A = APTSVOTest::Instance.Get();
        if (!A || Args.Num() < 3) return;
        A->PaintColor = FColor(
            (uint8)FMath::Clamp(FCString::Atoi(*Args[0]), 0, 255),
            (uint8)FMath::Clamp(FCString::Atoi(*Args[1]), 0, 255),
            (uint8)FMath::Clamp(FCString::Atoi(*Args[2]), 0, 255));
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
    // Idempotente: sólo crea el octree si aún no existe (así los Add ACUMULAN, no se pisan).
    if (!Octree.IsInit())
        Octree.Init(FVector(-RootSize * 0.5f), RootSize, MaxDepth); // origin = -RootSize/2 → centro en (0,0,0)
}

void APTSVOTest::ClearAll()
{
    Octree.Init(FVector(-RootSize * 0.5f), RootSize, MaxDepth); // fuerza árbol vacío
    if (Mesh) Mesh->ClearAllMeshSections();
    UE_LOG(LogTemp, Log, TEXT("[SVO] Clear."));
}

void APTSVOTest::RunDemo()
{
    Octree.Init(FVector(-RootSize * 0.5f), RootSize, MaxDepth); // demo siempre desde cero
    // Esfera GRANDE (se mallar grueso → pocos tris), en gris arcilla.
    const float BigR = RootSize * 0.16f;
    const float SmallR = RootSize * 0.02f;
    Octree.EditSphere(FVector(0, 0, 0), BigR, /*bAdd=*/true, FColor(180, 170, 160));
    // Varios detalles CHICOS de colores encima (se subdividen fino → detalle + color por voxel).
    static const FColor Palette[8] = {
        FColor::Red, FColor::Green, FColor::Blue, FColor::Yellow,
        FColor::Cyan, FColor::Magenta, FColor::Orange, FColor::Purple };
    for (int32 i = 0; i < 8; ++i)
    {
        const float a = (float)i / 8.f * 2.f * PI;
        const FVector P(FMath::Cos(a) * BigR * 0.9f, FMath::Sin(a) * BigR * 0.9f, BigR * 0.4f);
        Octree.EditSphere(P, SmallR, /*bAdd=*/true, Palette[i]);
    }
    Rebuild();
}

void APTSVOTest::AddSphere(float Radius)
{
    EnsureInit();
    Octree.PushUndoSnapshot(); // cada Add = un trazo deshacible
    Octree.EditSphere(FVector(0, 0, 0), Radius, /*bAdd=*/true, PaintColor);
    Rebuild();
}

void APTSVOTest::Undo()
{
    if (Octree.Undo()) { Rebuild(); UE_LOG(LogTemp, Log, TEXT("[SVO] Undo (quedan %d)."), Octree.UndoDepth()); }
    else               UE_LOG(LogTemp, Log, TEXT("[SVO] Nada para deshacer."));
}

void APTSVOTest::Rebuild()
{
    if (!Mesh) return;
    TArray<FVector> V, N;
    TArray<int32>   T;
    TArray<FColor>  C;
    Octree.BuildMesh(V, T, N, C);

    TArray<FVector2D> UV;
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
