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

static FAutoConsoleCommand GCmdSVOBake(
    TEXT("PTSVO.Bake"), TEXT("Serializa el octree a un blob de bytes (persistencia)."),
    FConsoleCommandDelegate::CreateLambda([] { if (APTSVOTest* A = APTSVOTest::Instance.Get()) A->Bake(); }));

static FAutoConsoleCommand GCmdSVORestore(
    TEXT("PTSVO.Restore"), TEXT("Recarga el octree desde el ultimo PTSVO.Bake (verifica round-trip)."),
    FConsoleCommandDelegate::CreateLambda([] { if (APTSVOTest* A = APTSVOTest::Instance.Get()) A->Restore(); }));

static FAutoConsoleCommand GCmdSVOShape(
    TEXT("PTSVO.Shape"),
    TEXT("PTSVO.Shape <box|cyl|torus|cone|ellip> <hx> <hy> <hz> [yawGrados] — shape con escala no-uniforme y rotacion."),
    FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
    {
        APTSVOTest* A = APTSVOTest::Instance.Get();
        if (!A || Args.Num() < 4) return;
        EPTSVOShape S = EPTSVOShape::Box;
        const FString Name = Args[0].ToLower();
        if      (Name.StartsWith(TEXT("cyl")))   S = EPTSVOShape::Cylinder;
        else if (Name.StartsWith(TEXT("tor")))   S = EPTSVOShape::Torus;
        else if (Name.StartsWith(TEXT("con")))   S = EPTSVOShape::Cone;
        else if (Name.StartsWith(TEXT("ell")) || Name.StartsWith(TEXT("sph"))) S = EPTSVOShape::Sphere;
        const FVector H(FCString::Atof(*Args[1]), FCString::Atof(*Args[2]), FCString::Atof(*Args[3]));
        const float Yaw = (Args.Num() > 4) ? FCString::Atof(*Args[4]) : 0.f;
        A->AddShape(S, H, Yaw);
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

void APTSVOTest::AddShape(EPTSVOShape Shape, const FVector& HalfExtent, float YawDeg)
{
    EnsureInit();
    Octree.PushUndoSnapshot();
    const FTransform Xf(FRotator(0.f, YawDeg, 0.f), FVector::ZeroVector);
    Octree.EditShape(Xf, Shape, HalfExtent, /*bAdd=*/true, PaintColor);
    Rebuild();
}

void APTSVOTest::Undo()
{
    if (Octree.Undo()) { Rebuild(); UE_LOG(LogTemp, Log, TEXT("[SVO] Undo (quedan %d)."), Octree.UndoDepth()); }
    else               UE_LOG(LogTemp, Log, TEXT("[SVO] Nada para deshacer."));
}

void APTSVOTest::Bake()
{
    Octree.Serialize(BakedBlob);
    UE_LOG(LogTemp, Log, TEXT("[SVO] Bake: %d bytes."), BakedBlob.Num());
    if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Green,
        FString::Printf(TEXT("[SVO] Bake: %d bytes"), BakedBlob.Num()));
}

void APTSVOTest::Restore()
{
    if (BakedBlob.Num() == 0) { UE_LOG(LogTemp, Warning, TEXT("[SVO] No hay blob bakeado.")); return; }
    if (Octree.LoadFromBytes(BakedBlob)) { Rebuild(); UE_LOG(LogTemp, Log, TEXT("[SVO] Restore OK.")); }
    else UE_LOG(LogTemp, Warning, TEXT("[SVO] Restore fallo."));
}

void APTSVOTest::Rebuild()
{
    if (!Mesh) return;
    Octree.Balance(); // 2:1 antes de mallar (sin artefactos en saltos grandes)
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
