#include "PTVoxelOctree.h"
#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "PTSculptVolume.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/ScopeExit.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPTSVOIncrementalBalanceTest,
    "MyPartyGame.SVO.IncrementalBalanceMatchesFull",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPTSVOIncrementalBalanceTest::RunTest(const FString& Parameters)
{
    FPTVoxelOctree Incremental, Reference;
    Incremental.Init(FVector(-256), 512, 8);
    Reference.Init(FVector(-256), 512, 8);
    Incremental.SetClampBox(FBox(FVector(-220), FVector(220)));
    Reference.SetClampBox(FBox(FVector(-220), FVector(220)));
    FRandomStream Random(7391);
    for (int32 Step = 0; Step < 90; ++Step)
    {
        const FVector P(Random.FRandRange(-210, 210), Random.FRandRange(-210, 210), Random.FRandRange(-210, 210));
        const float R = Step % 3 == 0 ? 75.f : (Step % 3 == 1 ? 6.f : 22.f);
        const FTransform Xf(FRotator(17.f * Step, 29.f * Step, 11.f).Quaternion(), P);
        const FVector H(R, R * .7f, R * 1.3f);
        const EPTSVOShape Shape = static_cast<EPTSVOShape>(Step % 5);
        const FColor Color(Step * 2, 110, 200);
        if (Step == 30) { Incremental.PushUndoSnapshot(); Reference.PushUndoSnapshot(); }
        Incremental.EditShape(Xf, Shape, H, Step % 4 != 3, Color);
        Reference.EditShape(Xf, Shape, H, Step % 4 != 3, Color);
        Incremental.Balance(); Reference.Balance(nullptr, true);
        if (Step == 45) { Incremental.Undo(); Reference.Undo(); Incremental.Balance(); Reference.Balance(nullptr, true); }
        TArray<uint8> A, B;
        Incremental.Serialize(A); Reference.Serialize(B);
        if (!TestTrue(FString::Printf(TEXT("Identical tree after edit %d"), Step), A == B)) return false;
        if (Step == 60) { Incremental.LoadFromBytes(A); Reference.LoadFromBytes(B); }
    }
    TArray<FVector> AV, AN, BV, BN; TArray<int32> AT, BT; TArray<FColor> AC, BC;
    Incremental.BuildMesh(AV, AT, AN, AC); Reference.BuildMesh(BV, BT, BN, BC);
    TestTrue(TEXT("Same vertices, indices, normals and colors"), AV == BV && AT == BT && AN == BN && AC == BC);
    Incremental.PaintShape(FTransform(FQuat::Identity, FVector::ZeroVector), EPTSVOShape::Sphere, FVector(200), FColor::Red);
    Incremental.Balance();
    TestEqual(TEXT("Paint does not rebalance geometry"), Incremental.GetLastBalanceLeafChecks(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPTSVOScaleTest,
    "MyPartyGame.SVO.LocalEditScaling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPTSVOScaleTest::RunTest(const FString& Parameters)
{
    FPTVoxelOctree Local, Full;
    Local.Init(FVector(-512), 1024, 9);
    // Many distant fine strokes: reproduce accumulated detail, not just a fresh sphere.
    for (int32 z = 0; z < 5; ++z)
    for (int32 y = 0; y < 7; ++y)
    for (int32 x = 0; x < 7; ++x)
        Local.EditSphere(FVector(-400 + 120*x, -400 + 120*y, -400 + 160*z), 9.f, true);
    Local.Balance();
    TArray<uint8> Initial; Local.Serialize(Initial); Full.LoadFromBytes(Initial); Full.Balance();
    double LocalSeconds = 0, FullSeconds = 0; int64 LocalChecks = 0, FullChecks = 0;
    for (int32 i = 0; i < 30; ++i)
    {
        const FVector P(-398.f + i*.2f, -398.f, -397.f);
        Local.EditSphere(P, 4.f, i % 3 != 2); Full.EditSphere(P, 4.f, i % 3 != 2);
        double Start = FPlatformTime::Seconds(); Local.Balance(); LocalSeconds += FPlatformTime::Seconds() - Start;
        Start = FPlatformTime::Seconds(); Full.Balance(nullptr, true); FullSeconds += FPlatformTime::Seconds() - Start;
        LocalChecks += Local.GetLastBalanceLeafChecks(); FullChecks += Full.GetLastBalanceLeafChecks();
    }
    TArray<uint8> A,B; Local.Serialize(A); Full.Serialize(B);
    TestTrue(TEXT("Preserves all accumulated detail"), A == B);
    TestTrue(TEXT("Local balance checks less than 20 percent of full traversal"), LocalChecks * 5 < FullChecks);
    AddInfo(FString::Printf(TEXT("30 fine edits: local balance %.3f ms / full %.3f ms; leaf checks %lld / %lld; leaves %d"),
        LocalSeconds*1000, FullSeconds*1000, LocalChecks, FullChecks, Local.CountLeaves()));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPTSVOChunkMeshTest,
    "MyPartyGame.SVO.ChunkPartitionMatchesWhole",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPTSVOChunkMeshTest::RunTest(const FString& Parameters)
{
    FPTVoxelOctree F; F.Init(FVector(-128), 256, 7);
    F.EditSphere(FVector::ZeroVector, 90, true, FColor::Green);
    for (int32 i = 0; i < 18; ++i)
        F.EditSphere(FVector(63, -45 + i*5, 46), 7, i % 3 != 0, FColor::Blue);
    F.Balance();
    TArray<FVector> V,N; TArray<int32> T; TArray<FColor> C;
    auto AddTriangles = [&](TArray<FString>& Out)
    {
        TestEqual(TEXT("Normal count"), N.Num(), V.Num());
        TestEqual(TEXT("Color count"), C.Num(), V.Num());
        for (int32 i = 0; i < T.Num(); i += 3)
        {
            FString Key;
            for (int32 j = 0; j < 3; ++j)
            {
                if (!V.IsValidIndex(T[i+j])) { AddError(TEXT("Invalid mesh index")); return; }
                const int32 Idx = T[i+j];
                Key += V[Idx].ToString() + N[Idx].ToString() + C[Idx].ToString();
            }
            Out.Add(Key);
        }
    };
    TArray<FString> Whole, Chunked;
    F.BuildMesh(V,T,N,C); AddTriangles(Whole);
    constexpr int32 D = 12; const float CS = F.GetRootSize()/D;
    F.BuildMeshFiltered(FBox(FVector(-129), FVector(129)), CS, FLT_MAX, V,T,N,C); AddTriangles(Chunked);
    for (int32 z=0; z<D; ++z) for (int32 y=0; y<D; ++y) for (int32 x=0; x<D; ++x)
    {
        const FVector Min = F.GetOrigin() + FVector(x,y,z)*CS;
        F.BuildMeshFiltered(FBox(Min, Min + FVector(CS)), 0, CS, V,T,N,C); AddTriangles(Chunked);
    }
    Whole.Sort(); Chunked.Sort();
    TestTrue(TEXT("Chunk partition has exactly the same oriented triangles and shading as whole mesh"), Whole == Chunked);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPTSVOVolumeLifecycleTest,
    "MyPartyGame.SVO.VolumeIncrementalLifecycle",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPTSVOVolumeLifecycleTest::RunTest(const FString& Parameters)
{
    UWorld* World = nullptr;
    for (const FWorldContext& Context : GEngine->GetWorldContexts())
        if (Context.WorldType == EWorldType::Editor) { World = Context.World(); break; }
    if (!TestNotNull(TEXT("Editor world"), World)) return false;
    FActorSpawnParameters Spawn; Spawn.ObjectFlags |= RF_Transient;
    APTSculptVolume* Volume = World->SpawnActor<APTSculptVolume>(FVector::ZeroVector, FRotator::ZeroRotator, Spawn);
    if (!TestNotNull(TEXT("Test sculpt volume"), Volume)) return false;
    ON_SCOPE_EXIT { World->DestroyActor(Volume); };
    Volume->bUseSVO = true;
    auto Rebuild = [&] { static_cast<AActor*>(Volume)->Tick(.06f); };
    auto Stamp = [&](FVector P, float Size, EPTEditMode Mode, bool Detail = false)
    {
        Volume->Multicast_ApplyStamp_Implementation(P, EPTStampShape::Cube, Size, Mode,
            FLinearColor::Green, FRotator(20,45,10), Detail, FVector(1, .7, 1.3));
        Rebuild();
    };
    auto Snapshot = [&]
    {
        TArray<FString> Rows;
        auto Collect = [&](UProceduralMeshComponent* M)
        {
            if (!M) return;
            for (int32 s = 0; s < M->GetNumSections(); ++s)
            {
                const FProcMeshSection* Sec = M->GetProcMeshSection(s);
                if (!Sec) continue;
                for (int32 i = 0; i < Sec->ProcIndexBuffer.Num(); i += 3)
                {
                    FString Row;
                    for (int32 j = 0; j < 3; ++j)
                    {
                        const FProcMeshVertex& V = Sec->ProcVertexBuffer[Sec->ProcIndexBuffer[i+j]];
                        Row += V.Position.ToString() + V.Normal.ToString() + V.Color.ToString();
                    }
                    Rows.Add(Row);
                }
            }
        };
        Collect(Volume->GetMeshComponent());
        for (const auto& Pair : Volume->GetSVOChunkMeshes()) Collect(Pair.Value);
        for (UProceduralMeshComponent* M : Volume->GetDetailMeshes()) Collect(M);
        Rows.Sort(); return Rows;
    };
    Stamp(FVector(-320,-320,-320), 55, EPTEditMode::Add);
    Stamp(FVector(300,300,300), 60, EPTEditMode::Add);
    TestTrue(TEXT("Fine meshes exist independently of root"), Volume->GetSVOChunkMeshes().Num() > 1);
    UProceduralMeshComponent* Far = nullptr;
    for (const auto& Pair : Volume->GetSVOChunkMeshes())
        if (Pair.Value->Bounds.Origin.X > 200) { Far = Pair.Value; break; }
    if (!TestNotNull(TEXT("Distant chunk"), Far)) return false;
    const FProcMeshVertex* FarBuffer = Far->GetProcMeshSection(0)->ProcVertexBuffer.GetData();
    Volume->Multicast_BeginStroke_Implementation();
    const auto BeforeStroke = Snapshot();
    for (int32 i=0; i<8; ++i) Stamp(FVector(-304+i*2,-306,-302), 12, EPTEditMode::Add);
    Volume->Multicast_EndStroke_Implementation();
    TestTrue(TEXT("Distant chunk keeps its mesh buffer"), FarBuffer == Far->GetProcMeshSection(0)->ProcVertexBuffer.GetData());
    Volume->Multicast_Undo_Implementation(); Rebuild();
    TestTrue(TEXT("Undo restores mesh"), BeforeStroke == Snapshot());
    Volume->Multicast_BeginDetailLayer_Implementation();
    Stamp(FVector(-305,-305,-270), 24, EPTEditMode::Add, true);
    Stamp(FVector(-315,-315,-300), 20, EPTEditMode::Paint);
    Stamp(FVector(-308,-308,-286), 16, EPTEditMode::Erase);
    const auto Incremental = Snapshot(); TArray<uint8> Saved;
    Volume->SaveFieldState(Saved);
    TestTrue(TEXT("Load succeeds"), Volume->LoadFieldState(Saved)); Rebuild();
    TestTrue(TEXT("Incremental mesh equals full rebuild after rotated edits, paint and erase"), Incremental == Snapshot());
    Volume->ClearAll(); Rebuild();
    TestEqual(TEXT("Clear destroys chunk components"), Volume->GetSVOChunkMeshes().Num(), 0);
    TestEqual(TEXT("Clear removes all triangles"), Snapshot().Num(), 0);
    return true;
}
#endif
