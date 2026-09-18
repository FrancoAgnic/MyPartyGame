// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMapEnvironment.h"
#include "../Sculpt/PTSculptVolume.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshAttributes.h"
#include "MeshDescription.h"
#include "MeshDescriptionBuilder.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/SceneCapture2D.h"
#include "Engine/DirectionalLight.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Engine/SkyLight.h"
#include "Components/SkyLightComponent.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/Actor.h"

APTMapEnvironment::APTMapEnvironment()
{
    PrimaryActorTick.bCanEverTick = false;
    USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);
}

// ── Juntar la geometría del box (base + SVO chunks + capas de detalle) en una sola malla ──
static void PT_GatherVolumeGeometry(APTSculptVolume* Volume, FPTPropGeometry& Out,
                                    bool bUsePivot = false, const FVector& PivotWorld = FVector::ZeroVector)
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
    AddSection(Volume->GetEyesMesh()); // ojos colocados → se hornean junto a la arcilla

    // Recentrar: el ORIGEN del asset queda donde el pivot elegido (si bUsePivot) o en el centro del bbox
    // (por defecto). Así al colocar la instancia el pivot cae exacto en el punto de colocación.
    if (Out.Verts.Num() > 0)
    {
        FVector3f Origin;
        if (bUsePivot)
        {
            Origin = (FVector3f)PivotWorld; // los verts ya están en mundo → restar el pivot en mundo
        }
        else
        {
            FBox3f Box(ForceInit);
            for (const FVector3f& V : Out.Verts) Box += V;
            Origin = Box.GetCenter();
        }
        for (FVector3f& V : Out.Verts) V -= Origin;
    }
}

void APTMapEnvironment::SetSkySettings(const FPTSkySettings& In)
{
    SkySettings = In;
    ApplySkySettings();
}

void APTMapEnvironment::ApplySkySettings()
{
    UWorld* W = GetWorld();
    if (!W) return;
    const FPTSkySettings& S = SkySettings;

    // ── Sol (primer DirectionalLight del nivel) ── TimeOfDay 0→amanecer(0°) .5→mediodía(-90°) 1→atardecer(-180°)
    const float Pitch = FMath::Lerp(0.f, -180.f, FMath::Clamp(S.TimeOfDay, 0.f, 1.f));
    const FRotator SunRot(Pitch, S.SunYaw, 0.f);
    if (ADirectionalLight* Sun = Cast<ADirectionalLight>(UGameplayStatics::GetActorOfClass(W, ADirectionalLight::StaticClass())))
    {
        Sun->SetActorRotation(SunRot);
        if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Sun->GetLightComponent()))
        {
            LC->SetLightColor(S.SunColor);
            LC->SetIntensity(FMath::Max(0.f, S.SunIntensity));
        }
    }
    // Dirección HACIA el sol (para el disco del sol del material del cielo) = opuesto al "forward" de la luz.
    const FVector SunDir = -SunRot.Vector();

    // ── Luz ambiental (SkyLight) ──
    if (ASkyLight* Sky = Cast<ASkyLight>(UGameplayStatics::GetActorOfClass(W, ASkyLight::StaticClass())))
        if (USkyLightComponent* SLC = Sky->GetLightComponent())
        {
            SLC->SetLightColor(S.AmbientColor);
            SLC->SetIntensity(FMath::Max(0.f, S.AmbientIntensity));
        }

    // ── Niebla (ExponentialHeightFog) ──
    if (AExponentialHeightFog* Fog = Cast<AExponentialHeightFog>(UGameplayStatics::GetActorOfClass(W, AExponentialHeightFog::StaticClass())))
        if (UExponentialHeightFogComponent* FC = Fog->GetComponent())
        {
            FC->SetFogInscatteringColor(S.FogColor);
            FC->SetFogDensity(FMath::Max(0.f, S.FogDensity));
        }

    // ── Sky Sphere (actor con tag "MapSky") → material con parámetros SkyTop/SkyHorizon/SunColor ──
    TArray<AActor*> SkyActors;
    UGameplayStatics::GetAllActorsWithTag(W, FName("MapSky"), SkyActors);
    for (AActor* A : SkyActors)
    {
        if (!A) continue;
        UStaticMeshComponent* SM = A->FindComponentByClass<UStaticMeshComponent>();
        if (!SM || SM->GetNumMaterials() == 0) continue;
        UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(SM->GetMaterial(0));
        if (!MID) MID = SM->CreateDynamicMaterialInstance(0);
        if (!MID) continue;
        MID->SetVectorParameterValue(TEXT("SkyTop"),      S.SkyTopColor);
        MID->SetVectorParameterValue(TEXT("SkyHorizon"),  S.SkyHorizonColor);
        MID->SetVectorParameterValue(TEXT("SunColor"),    S.SunColor);
        // El parámetro del sol en M_CartoonSky se llama "SunDir". Seteamos ambos nombres por robustez
        // (el que no exista es no-op) para que el disco del sol SIGA al sol real al cambiar hora/yaw.
        const FLinearColor SunDirCol(SunDir.X, SunDir.Y, SunDir.Z, 0.f);
        MID->SetVectorParameterValue(TEXT("SunDir"),       SunDirCol);
        MID->SetVectorParameterValue(TEXT("SunDirection"), SunDirCol);
        MID->SetScalarParameterValue(TEXT("Bands"),      S.Bands);
        MID->SetScalarParameterValue(TEXT("HorizonExp"), S.HorizonExp);
        MID->SetScalarParameterValue(TEXT("SunSize"),    S.SunSize);
        MID->SetScalarParameterValue(TEXT("SunGlow"),    S.SunGlow);
    }

    // ── Assets: mismo sol que el cielo (por si el material los usa); el MID es compartido ──
    ApplyAssetSunParams();
}

UMaterialInstanceDynamic* APTMapEnvironment::GetOrCreatePropMID()
{
    if (PropMID) return PropMID;
    if (!PropMaterial) return nullptr;
    PropMID = UMaterialInstanceDynamic::Create(PropMaterial, this);
    ApplyAssetSunParams();
    return PropMID;
}

UMaterialInterface* APTMapEnvironment::GetPropMaterialForPreview()
{
    UMaterialInterface* M = GetOrCreatePropMID();
    return M ? M : PropMaterial;
}

void APTMapEnvironment::ApplyAssetSunParams()
{
    if (!PropMID) return;
    const float Pitch = FMath::Lerp(0.f, -180.f, FMath::Clamp(SkySettings.TimeOfDay, 0.f, 1.f));
    const FVector SunDir = -FRotator(Pitch, SkySettings.SunYaw, 0.f).Vector(); // dirección HACIA el sol
    const FLinearColor SunDirCol(SunDir.X, SunDir.Y, SunDir.Z, 0.f);
    // Mismo nombre de parámetro que el cielo ("SunDir"); seteamos ambos por robustez (no-op el que falte).
    PropMID->SetVectorParameterValue(TEXT("SunDir"),       SunDirCol);
    PropMID->SetVectorParameterValue(TEXT("SunDirection"), SunDirCol);
    PropMID->SetVectorParameterValue(TEXT("SunColor"),     SkySettings.SunColor);
}

int32 APTMapEnvironment::BakeAssetFromVolume(APTSculptVolume* Volume, bool bUsePivot, const FVector& PivotWorld)
{
    if (!Volume) return INDEX_NONE;
    FPTPropGeometry Geo;
    PT_GatherVolumeGeometry(Volume, Geo, bUsePivot, PivotWorld);
    if (!Geo.IsValid()) return INDEX_NONE; // box vacío
    return AddAsset(Geo);
}

void APTMapEnvironment::FillProcSection(UProceduralMeshComponent* PMC, int32 Section,
                                        const FPTPropGeometry& Geo, const FTransform& Xf, bool bCollision)
{
    if (!PMC || !Geo.IsValid()) return;
    const int32 NV = Geo.Verts.Num();
    TArray<FVector> Verts;   Verts.SetNumUninitialized(NV);
    TArray<FVector> Normals; Normals.SetNumUninitialized(NV);
    TArray<FColor>  Colors;  Colors.SetNumUninitialized(NV);
    for (int32 i = 0; i < NV; ++i)
    {
        Verts[i]   = Xf.TransformPosition(FVector(Geo.Verts[i]));
        Normals[i] = Xf.TransformVectorNoScale(FVector(Geo.Normals.IsValidIndex(i) ? Geo.Normals[i] : FVector3f::ZAxisVector));
        Colors[i]  = Geo.Colors.IsValidIndex(i) ? Geo.Colors[i] : FColor::White;
    }
    const TArray<FVector2D> NoUV;
    const TArray<FProcMeshTangent> NoTan;
    // ProceduralMeshComponent SÍ renderiza los vertex colors en build cocinada (a diferencia de un
    // UStaticMesh construido en runtime), por eso los props horneados usan esto.
    PMC->CreateMeshSection(Section, Verts, Geo.Tris, Normals, NoUV, Colors, NoTan, bCollision);
}

int32 APTMapEnvironment::AddAsset(const FPTPropGeometry& Geo)
{
    if (!Geo.IsValid()) return INDEX_NONE;

    UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
    PMC->SetupAttachment(GetRootComponent());
    PMC->RegisterComponent();
    PMC->SetMobility(EComponentMobility::Movable);
    PMC->bUseComplexAsSimpleCollision = true;
    PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PMC->SetCollisionObjectType(ECC_WorldStatic);

    FPTPropAsset A;
    A.Geo = Geo;
    A.PMC = PMC;
    return Assets.Add(MoveTemp(A));
}

const FPTPropGeometry* APTMapEnvironment::GetAssetGeometry(int32 AssetIdx) const
{
    return Assets.IsValidIndex(AssetIdx) ? &Assets[AssetIdx].Geo : nullptr;
}

float APTMapEnvironment::GetAssetRadius(int32 AssetIdx) const
{
    if (!Assets.IsValidIndex(AssetIdx)) return 0.f;
    const FPTPropGeometry& G = Assets[AssetIdx].Geo;
    if (G.Verts.Num() == 0) return 0.f;
    FBox3f Box(ForceInit);
    for (const FVector3f& V : G.Verts) Box += V;
    return Box.GetExtent().Size();
}

UTextureRenderTarget2D* APTMapEnvironment::GetAssetThumbnail(int32 AssetIdx, int32 Size)
{
    if (!Assets.IsValidIndex(AssetIdx)) return nullptr;
    if (Thumbnails.IsValidIndex(AssetIdx) && Thumbnails[AssetIdx]) return Thumbnails[AssetIdx];

    UWorld* W = GetWorld();
    const FPTPropGeometry& Geo = Assets[AssetIdx].Geo;
    if (!W || !Geo.IsValid()) return nullptr;
    Size = FMath::Clamp(Size, 64, 512);

    UTextureRenderTarget2D* RT = UKismetRenderingLibrary::CreateRenderTarget2D(this, Size, Size, RTF_RGBA8);
    if (!RT) return nullptr;

    // Malla temporal (ProceduralMesh, para que se vean los vertex colors) LEJOS del mapa, aislada.
    const FVector Far(0.f, 0.f, 200000.f);
    AActor* MA = W->SpawnActor<AActor>(AActor::StaticClass(), FTransform(Far));
    if (!MA) return nullptr;
    UProceduralMeshComponent* MC = NewObject<UProceduralMeshComponent>(MA);
    MA->SetRootComponent(MC);
    MC->RegisterComponent();
    MC->SetMobility(EComponentMobility::Movable);
    FillProcSection(MC, 0, Geo, FTransform::Identity, /*bCollision=*/false);
    if (UMaterialInterface* Mat = GetPropMaterialForPreview()) MC->SetMaterial(0, Mat);
    MC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MA->SetActorLocation(Far);

    // Encuadre 3/4 según el radio (bbox de la geometría, centrada en ~0).
    const float    Radius = FMath::Max(1.f, GetAssetRadius(AssetIdx));
    const FVector  Center = Far;
    const float    FOV    = 45.f;
    const float    Dist   = Radius / FMath::Tan(FMath::DegreesToRadians(FOV * 0.5f)) * 1.35f;
    const FVector  Dir    = FVector(1.f, 0.55f, -0.5f).GetSafeNormal(); // cámara → objeto
    const FVector  Loc    = Center - Dir * Dist;

    ASceneCapture2D* Cap = W->SpawnActor<ASceneCapture2D>();
    if (!Cap) { MA->Destroy(); return nullptr; }
    USceneCaptureComponent2D* C = Cap->GetCaptureComponent2D();
    C->TextureTarget       = RT;
    C->CaptureSource       = ESceneCaptureSource::SCS_FinalColorLDR;
    C->bCaptureEveryFrame  = false;
    C->bCaptureOnMovement  = false;
    C->FOVAngle            = FOV;
    C->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    C->ShowOnlyActors.Add(MA);
    C->ShowFlags.SetDynamicShadows(false);
    C->ShowFlags.SetAtmosphere(false);
    C->ShowFlags.SetFog(false);
    Cap->SetActorLocation(Loc);
    Cap->SetActorRotation(Dir.Rotation());

    // Luz limpia frontal (sin sombras) para que la miniatura no salga negra.
    ADirectionalLight* Light = W->SpawnActor<ADirectionalLight>();
    if (Light)
    {
        if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Light->GetLightComponent()))
        {
            LC->SetMobility(EComponentMobility::Movable);
            LC->SetIntensity(3.5f);
            LC->SetCastShadows(false);
            LC->SetLightColor(FLinearColor::White);
        }
        Light->SetActorRotation(FVector(1.f, 0.3f, -0.8f).GetSafeNormal().Rotation());
    }

    C->CaptureScene();

    Cap->Destroy();
    MA->Destroy();
    if (Light) Light->Destroy();

    if (Thumbnails.Num() < Assets.Num()) Thumbnails.SetNumZeroed(Assets.Num());
    Thumbnails[AssetIdx] = RT;
    return RT;
}

void APTMapEnvironment::RebuildAssetMesh(FPTPropAsset& A)
{
    if (!A.PMC) return;
    if (A.InstXf.Num() == 0) { A.PMC->ClearMeshSection(0); return; }

    const FPTPropGeometry& G = A.Geo;
    const int32 NVper = G.Verts.Num();
    const int32 NTper = G.Tris.Num();
    TArray<FVector> Verts;   Verts.Reserve(NVper * A.InstXf.Num());
    TArray<FVector> Normals; Normals.Reserve(NVper * A.InstXf.Num());
    TArray<FColor>  Colors;  Colors.Reserve(NVper * A.InstXf.Num());
    TArray<int32>   Tris;    Tris.Reserve(NTper * A.InstXf.Num());

    for (const FTransform& Xf : A.InstXf)
    {
        const int32 Base = Verts.Num();
        for (int32 i = 0; i < NVper; ++i)
        {
            Verts.Add(Xf.TransformPosition(FVector(G.Verts[i])));
            Normals.Add(Xf.TransformVectorNoScale(FVector(G.Normals.IsValidIndex(i) ? G.Normals[i] : FVector3f::ZAxisVector)));
            Colors.Add(G.Colors.IsValidIndex(i) ? G.Colors[i] : FColor::White);
        }
        for (int32 t = 0; t < NTper; ++t) Tris.Add(Base + G.Tris[t]);
    }

    const TArray<FVector2D> NoUV;
    const TArray<FProcMeshTangent> NoTan;
    A.PMC->ClearMeshSection(0);
    A.PMC->CreateMeshSection(0, Verts, Tris, Normals, NoUV, Colors, NoTan, /*bCollision=*/true);
    UMaterialInterface* Mat = GetOrCreatePropMID(); if (!Mat) Mat = PropMaterial;
    if (Mat) A.PMC->SetMaterial(0, Mat);
}

void APTMapEnvironment::PlaceInstance(int32 AssetIdx, const FTransform& WorldXf)
{
    if (!Assets.IsValidIndex(AssetIdx)) return;
    FPTPropAsset& A = Assets[AssetIdx];
    A.InstXf.Add(WorldXf);
    RebuildAssetMesh(A);
    PlaceOrder.Add(AssetIdx); // para el undo LIFO
}

bool APTMapEnvironment::GetNearestInstance(const FVector& WorldPos, float Radius, int32& OutAsset, FTransform& OutXf) const
{
    float BestD2 = Radius * Radius; OutAsset = INDEX_NONE;
    for (int32 a = 0; a < Assets.Num(); ++a)
        for (const FTransform& Xf : Assets[a].InstXf)
        {
            const float D2 = FVector::DistSquared(Xf.GetLocation(), WorldPos);
            if (D2 < BestD2) { BestD2 = D2; OutAsset = a; OutXf = Xf; }
        }
    return OutAsset != INDEX_NONE;
}

bool APTMapEnvironment::RemoveInstanceNear(const FVector& WorldPos, float Radius)
{
    float BestD2 = Radius * Radius;
    int32 BestAsset = INDEX_NONE, BestInst = INDEX_NONE;
    for (int32 a = 0; a < Assets.Num(); ++a)
        for (int32 i = 0; i < Assets[a].InstXf.Num(); ++i)
        {
            const float D2 = FVector::DistSquared(Assets[a].InstXf[i].GetLocation(), WorldPos);
            if (D2 < BestD2) { BestD2 = D2; BestAsset = a; BestInst = i; }
        }
    if (BestAsset != INDEX_NONE)
    {
        Assets[BestAsset].InstXf.RemoveAt(BestInst);
        RebuildAssetMesh(Assets[BestAsset]);
        // Mantener PlaceOrder consistente (sacar una ocurrencia de ese asset, la última).
        for (int32 k = PlaceOrder.Num() - 1; k >= 0; --k)
            if (PlaceOrder[k] == BestAsset) { PlaceOrder.RemoveAt(k); break; }
        return true;
    }
    return false;
}

bool APTMapEnvironment::RemoveLastInstance()
{
    if (PlaceOrder.Num() == 0) return false;
    const int32 A = PlaceOrder.Pop();
    if (!Assets.IsValidIndex(A) || Assets[A].InstXf.Num() == 0) return false;
    Assets[A].InstXf.Pop(); // la última instancia de ese asset = la más reciente
    RebuildAssetMesh(Assets[A]);
    return true;
}

void APTMapEnvironment::ClearAll()
{
    for (FPTPropAsset& A : Assets)
        if (A.PMC) A.PMC->DestroyComponent();
    Assets.Reset();
    PlaceOrder.Reset();
    Thumbnails.Reset();
    PropMID = nullptr; // se recrea en el próximo mapa/ambiente
}

// Serializa los ajustes de ambiente (mismo orden en lectura/escritura). Version-aware para compatibilidad.
static void PT_SerializeSky(FArchive& Ar, FPTSkySettings& S, int32 Version)
{
    Ar << S.TimeOfDay; Ar << S.SunYaw;
    Ar << S.SkyTopColor; Ar << S.SkyHorizonColor;
    Ar << S.SunColor; Ar << S.SunIntensity;
    Ar << S.FogColor; Ar << S.FogDensity;
    Ar << S.AmbientColor; Ar << S.AmbientIntensity;
    if (Version >= 3) { Ar << S.Bands; Ar << S.HorizonExp; Ar << S.SunSize; Ar << S.SunGlow; }
}

void APTMapEnvironment::SerializeEnvironment(TArray<uint8>& Out)
{
    Out.Reset();
    FMemoryWriter Ar(Out, /*bIsPersistent=*/true);
    int32 Version = 3; Ar << Version; // v3: + knobs cartoon (Bands/HorizonExp/SunSize/SunGlow)
    PT_SerializeSky(Ar, SkySettings, Version);
    int32 NumAssets = Assets.Num(); Ar << NumAssets;
    for (FPTPropAsset& A : Assets)
    {
        Ar << A.Geo.Verts;
        Ar << A.Geo.Normals;
        Ar << A.Geo.Colors;
        Ar << A.Geo.Tris;
        // Instancias de este asset (transforms en mundo).
        int32 NInst = A.InstXf.Num();
        Ar << NInst;
        for (int32 i = 0; i < NInst; ++i)
        {
            FTransform Xf = A.InstXf[i];
            Ar << Xf;
        }
    }
}

void APTMapEnvironment::DeserializeEnvironment(const TArray<uint8>& In)
{
    ClearAll();
    if (In.Num() == 0) return;
    FMemoryReader Ar(In, /*bIsPersistent=*/true);
    int32 Version = 0; Ar << Version;
    if (Version >= 2) PT_SerializeSky(Ar, SkySettings, Version); // ambiente guardado con el mapa
    int32 NumAssets = 0; Ar << NumAssets;
    for (int32 a = 0; a < NumAssets; ++a)
    {
        FPTPropGeometry Geo;
        Ar << Geo.Verts;
        Ar << Geo.Normals;
        Ar << Geo.Colors;
        Ar << Geo.Tris;
        const int32 Idx = AddAsset(Geo); // reconstruye StaticMesh + HISM
        int32 NInst = 0; Ar << NInst;
        for (int32 i = 0; i < NInst; ++i)
        {
            FTransform Xf; Ar << Xf;
            if (Idx != INDEX_NONE) PlaceInstance(Idx, Xf);
        }
    }
    ApplySkySettings(); // aplicar el ambiente cargado (sol/cielo/niebla) en esta máquina
}
