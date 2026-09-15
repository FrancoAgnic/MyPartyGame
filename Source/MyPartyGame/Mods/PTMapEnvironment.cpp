// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMapEnvironment.h"
#include "../Sculpt/PTSculptVolume.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"

APTMapEnvironment::APTMapEnvironment()
{
    PrimaryActorTick.bCanEverTick = false;
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

// ── Juntar la geometría del box (base + SVO chunks + capas de detalle) en una sola malla ──
static void PT_GatherVolumeGeometry(APTSculptVolume* Volume, FPTPropGeometry& Out)
{
    auto AddSection = [&Out](UProceduralMeshComponent* Src)
    {
        if (!Src) return;
        const FTransform X = Src->GetComponentTransform();
        const int32 N = Src->GetNumSections();
        for (int32 s = 0; s < N; ++s)
        {
            const FProcMeshSection* Sec = Src->GetProcMeshSection(s);
            if (!Sec || Sec->ProcVertexBuffer.Num() == 0) continue;
            const int32 Base = Out.Verts.Num();
            for (const FProcMeshVertex& V : Sec->ProcVertexBuffer)
            {
                // A espacio del ACTOR (world del componente); luego se recentra en el bbox.
                Out.Verts.Add((FVector3f)X.TransformPosition(FVector(V.Position)));
                Out.Normals.Add((FVector3f)X.TransformVectorNoScale(FVector(V.Normal)));
                Out.Colors.Add(V.Color);
            }
            for (uint32 Idx : Sec->ProcIndexBuffer) Out.Tris.Add(Base + (int32)Idx);
        }
    };

    AddSection(Volume->GetMeshComponent());
    for (const auto& Pair : Volume->GetSVOChunkMeshes()) AddSection(Pair.Value);
    for (UProceduralMeshComponent* DM : Volume->GetDetailMeshes()) AddSection(DM);

    // Recentrar en el centro del bounding box → el prop queda centrado en su origen (colocar/rotar/escalar
    // intuitivo). Guardamos la geometría ya centrada.
    if (Out.Verts.Num() > 0)
    {
        FBox3f Box(ForceInit);
        for (const FVector3f& V : Out.Verts) Box += V;
        const FVector3f C = Box.GetCenter();
        for (FVector3f& V : Out.Verts) V -= C;
    }
}

int32 APTMapEnvironment::BakeAssetFromVolume(APTSculptVolume* Volume)
{
    if (!Volume) return INDEX_NONE;
    FPTPropGeometry Geo;
    PT_GatherVolumeGeometry(Volume, Geo);
    if (!Geo.IsValid()) return INDEX_NONE; // box vacío
    return AddAsset(Geo);
}

UStaticMesh* APTMapEnvironment::BuildStaticMesh(const FPTPropGeometry& Geo) const
{
    if (!Geo.IsValid()) return nullptr;

    FMeshDescription MeshDesc;
    FStaticMeshAttributes Attrs(MeshDesc);
    Attrs.Register();

    FMeshDescriptionBuilder Builder;
    Builder.SetMeshDescription(&MeshDesc);
    Builder.EnablePolyGroups();
    Builder.SetNumUVLayers(1);

    TArray<FVertexID> VertexIDs; VertexIDs.SetNum(Geo.Verts.Num());
    for (int32 i = 0; i < Geo.Verts.Num(); ++i)
        VertexIDs[i] = Builder.AppendVertex(FVector(Geo.Verts[i]));

    const FPolygonGroupID PG = Builder.AppendPolygonGroup();

    for (int32 t = 0; t + 2 < Geo.Tris.Num(); t += 3)
    {
        FVertexInstanceID VI[3];
        bool bOk = true;
        for (int32 c = 0; c < 3; ++c)
        {
            const int32 vi = Geo.Tris[t + c];
            if (!Geo.Verts.IsValidIndex(vi)) { bOk = false; break; }
            const FVertexInstanceID Id = Builder.AppendInstance(VertexIDs[vi]);
            Builder.SetInstanceNormal(Id, Geo.Normals.IsValidIndex(vi) ? FVector(Geo.Normals[vi]) : FVector::UpVector);
            Builder.SetInstanceColor(Id, FVector4f(FLinearColor(Geo.Colors.IsValidIndex(vi) ? Geo.Colors[vi] : FColor::White)));
            Builder.SetInstanceUV(Id, FVector2D::ZeroVector, 0);
            VI[c] = Id;
        }
        if (bOk) Builder.AppendTriangle(VI[0], VI[1], VI[2], PG);
    }

    UStaticMesh* Mesh = NewObject<UStaticMesh>(GetTransientPackage());
    Mesh->GetStaticMaterials().Add(FStaticMaterial(PropMaterial));
    Mesh->bAllowCPUAccess = true;

    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = true;
    Params.bFastBuild = true;
    Mesh->BuildFromMeshDescriptions({ &MeshDesc }, Params);
    return Mesh;
}

int32 APTMapEnvironment::AddAsset(const FPTPropGeometry& Geo)
{
    UStaticMesh* Mesh = BuildStaticMesh(Geo);
    if (!Mesh) return INDEX_NONE;

    UHierarchicalInstancedStaticMeshComponent* HISM =
        NewObject<UHierarchicalInstancedStaticMeshComponent>(this);
    HISM->SetupAttachment(GetRootComponent());
    HISM->RegisterComponent();
    HISM->SetStaticMesh(Mesh);
    if (PropMaterial) HISM->SetMaterial(0, PropMaterial);
    HISM->SetMobility(EComponentMobility::Movable);
    HISM->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    HISM->SetCollisionObjectType(ECC_WorldStatic);

    FPTPropAsset A;
    A.Geo = Geo;
    A.Mesh = Mesh;
    A.HISM = HISM;
    return Assets.Add(MoveTemp(A));
}

UStaticMesh* APTMapEnvironment::GetAssetMesh(int32 AssetIdx) const
{
    return Assets.IsValidIndex(AssetIdx) ? Assets[AssetIdx].Mesh : nullptr;
}
const FPTPropGeometry* APTMapEnvironment::GetAssetGeometry(int32 AssetIdx) const
{
    return Assets.IsValidIndex(AssetIdx) ? &Assets[AssetIdx].Geo : nullptr;
}

void APTMapEnvironment::PlaceInstance(int32 AssetIdx, const FTransform& WorldXf)
{
    if (!Assets.IsValidIndex(AssetIdx) || !Assets[AssetIdx].HISM) return;
    Assets[AssetIdx].HISM->AddInstance(WorldXf, /*bWorldSpace=*/true);
}

bool APTMapEnvironment::RemoveInstanceNear(const FVector& WorldPos, float Radius)
{
    float BestD2 = Radius * Radius;
    int32 BestAsset = INDEX_NONE, BestInst = INDEX_NONE;
    for (int32 a = 0; a < Assets.Num(); ++a)
    {
        UHierarchicalInstancedStaticMeshComponent* H = Assets[a].HISM;
        if (!H) continue;
        const int32 Count = H->GetInstanceCount();
        for (int32 i = 0; i < Count; ++i)
        {
            FTransform Xf;
            if (!H->GetInstanceTransform(i, Xf, /*bWorldSpace=*/true)) continue;
            const float D2 = FVector::DistSquared(Xf.GetLocation(), WorldPos);
            if (D2 < BestD2) { BestD2 = D2; BestAsset = a; BestInst = i; }
        }
    }
    if (BestAsset != INDEX_NONE)
    {
        Assets[BestAsset].HISM->RemoveInstance(BestInst);
        return true;
    }
    return false;
}
