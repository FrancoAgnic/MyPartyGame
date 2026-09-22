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
#include "DrawDebugHelpers.h" // debug del LOD (cilindro + marcadores por etapa)
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
static void PT_GatherVolumeGeometry(APTSculptVolume* Volume, FPTPropGeometry& Out, FPTPropGeometry& OutEyes,
                                    bool bUsePivot = false, const FVector& PivotWorld = FVector::ZeroVector)
{
    // Junta las secciones de un componente en Dst. bClay: guarda además la UV de LOOKUP del atlas de pintura
    // (posición VOLUMEN-LOCAL = V.Position, empaquetada en UV0.xy + UV1.x) para que el material del asset
    // pintado samplee el atlas exactamente como la arcilla en vivo → pintura NÍTIDA, sin tocar la geometría.
    auto AddInto = [](FPTPropGeometry& Dst, UProceduralMeshComponent* Src, bool bClay)
    {
        if (!Src) return;
        const FTransform X = Src->GetComponentTransform();
        const int32 NSec = Src->GetNumSections();
        for (int32 s = 0; s < NSec; ++s)
        {
            const FProcMeshSection* Sec = Src->GetProcMeshSection(s);
            if (!Sec || Sec->ProcVertexBuffer.Num() == 0) continue;
            const int32 Base = Dst.Verts.Num();
            for (const FProcMeshVertex& V : Sec->ProcVertexBuffer)
            {
                Dst.Verts.Add((FVector3f)X.TransformPosition(FVector(V.Position)));
                Dst.Normals.Add((FVector3f)X.TransformVectorNoScale(FVector(V.Normal)));
                Dst.Colors.Add(V.Color); // color BASE (Add); la pintura la da el atlas por UV
                if (bClay)
                {
                    // Posición volumen-local (raw), para el lookup del atlas.
                    Dst.UV0.Add(FVector2f((float)V.Position.X, (float)V.Position.Y));
                    Dst.UV1.Add(FVector2f((float)V.Position.Z, 0.f));
                }
            }
            for (uint32 Idx : Sec->ProcIndexBuffer) Dst.Tris.Add(Base + (int32)Idx);
        }
    };

    // ARCILLA (base + SVO + capas de detalle) → Out (con UV de atlas). OJOS → OutEyes (material aparte).
    AddInto(Out, Volume->GetMeshComponent(), /*bClay=*/true);
    for (const auto& Pair : Volume->GetSVOChunkMeshes()) AddInto(Out, Pair.Value, /*bClay=*/true);
    for (UProceduralMeshComponent* DM : Volume->GetDetailMeshes()) AddInto(Out, DM, /*bClay=*/true);
    AddInto(OutEyes, Volume->GetEyesMesh(), /*bClay=*/false);

    // Recentrar arcilla Y ojos por el MISMO origen (pivot elegido, o centro del bbox de la arcilla) para que
    // los ojos queden alineados con la arcilla al colocar la instancia.
    FVector3f Origin(0.f);
    if (bUsePivot) Origin = (FVector3f)PivotWorld;
    else if (Out.Verts.Num() > 0)
    {
        FBox3f Box(ForceInit);
        for (const FVector3f& V : Out.Verts) Box += V;
        Origin = Box.GetCenter();
    }
    for (FVector3f& V : Out.Verts)     V -= Origin;
    for (FVector3f& V : OutEyes.Verts) V -= Origin;
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
    FPTPropGeometry Geo, EyesGeo;
    PT_GatherVolumeGeometry(Volume, Geo, EyesGeo, bUsePivot, PivotWorld);
    if (!Geo.IsValid()) return INDEX_NONE; // box vacío
    FPTPaintAtlas Atlas;
    Volume->GetPaintAtlasSnapshot(Atlas); // pintura nítida (si hay); vacío = solo vertex color
    return AddAsset(Geo, EyesGeo, Atlas);
}

void APTMapEnvironment::FillProcSection(UProceduralMeshComponent* PMC, int32 Section,
                                        const FPTPropGeometry& Geo, const FTransform& Xf, bool bCollision)
{
    if (!PMC || !Geo.IsValid()) return;
    const int32 NV = Geo.Verts.Num();
    TArray<FVector> Verts;   Verts.SetNumUninitialized(NV);
    TArray<FVector> Normals; Normals.SetNumUninitialized(NV);
    TArray<FColor>  Colors;  Colors.SetNumUninitialized(NV);
    TArray<FVector2D> UV0, UV1;
    const bool bHasUV = (Geo.UV0.Num() == NV && Geo.UV1.Num() == NV); // UV de lookup del atlas de pintura
    if (bHasUV) { UV0.SetNumUninitialized(NV); UV1.SetNumUninitialized(NV); }
    for (int32 i = 0; i < NV; ++i)
    {
        Verts[i]   = Xf.TransformPosition(FVector(Geo.Verts[i]));
        Normals[i] = Xf.TransformVectorNoScale(FVector(Geo.Normals.IsValidIndex(i) ? Geo.Normals[i] : FVector3f::ZAxisVector));
        Colors[i]  = Geo.Colors.IsValidIndex(i) ? Geo.Colors[i] : FColor::White;
        if (bHasUV) { UV0[i] = FVector2D(Geo.UV0[i]); UV1[i] = FVector2D(Geo.UV1[i]); }
    }
    const TArray<FVector2D> NoUV;
    const TArray<FProcMeshTangent> NoTan;
    // ProceduralMeshComponent SÍ renderiza los vertex colors en build cocinada (a diferencia de un
    // UStaticMesh construido en runtime), por eso los props horneados usan esto. UV0/UV1 = lookup del atlas.
    PMC->CreateMeshSection(Section, Verts, Geo.Tris, Normals, UV0, UV1, NoUV, NoUV, Colors, NoTan, bCollision);
}

UMaterialInstanceDynamic* APTMapEnvironment::MakePaintMID(const FPTPaintAtlas& A)
{
    if (!A.bValid || !PaintClayMaterial) return nullptr;
    const int32 PGW = A.BrickDim.X;
    const int32 PGH = A.BrickDim.Y * A.BrickDim.Z;
    if (PGW <= 0 || PGH <= 0 || A.AtlasW <= 0 || A.AtlasH <= 0) return nullptr;
    if (A.PageBuf.Num() < PGW * PGH || A.AtlasBuf.Num() < A.AtlasW * A.AtlasH) return nullptr;

    // Recrear la page table (R32F) y el atlas (BGRA8) desde el snapshot.
    UTexture2D* PageTex = UTexture2D::CreateTransient(PGW, PGH, PF_R32_FLOAT);
    if (!PageTex) return nullptr;
    PageTex->SRGB = false; PageTex->Filter = TF_Nearest; PageTex->AddressX = TA_Clamp; PageTex->AddressY = TA_Clamp;
    {
        FTexture2DMipMap& Mip = PageTex->GetPlatformData()->Mips[0];
        void* D = Mip.BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(D, A.PageBuf.GetData(), FMath::Min<int64>((int64)A.PageBuf.Num() * sizeof(float), Mip.BulkData.GetBulkDataSize()));
        Mip.BulkData.Unlock();
    }
    PageTex->UpdateResource();

    UTexture2D* AtlasTex = UTexture2D::CreateTransient(A.AtlasW, A.AtlasH, PF_B8G8R8A8);
    if (!AtlasTex) return nullptr;
    AtlasTex->SRGB = true; AtlasTex->Filter = TF_Bilinear; AtlasTex->AddressX = TA_Clamp; AtlasTex->AddressY = TA_Clamp;
    {
        FTexture2DMipMap& Mip = AtlasTex->GetPlatformData()->Mips[0];
        void* D = Mip.BulkData.Lock(LOCK_READ_WRITE);
        FMemory::Memcpy(D, A.AtlasBuf.GetData(), FMath::Min<int64>((int64)A.AtlasBuf.Num() * sizeof(FColor), Mip.BulkData.GetBulkDataSize()));
        Mip.BulkData.Unlock();
    }
    AtlasTex->UpdateResource();

    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(PaintClayMaterial, this);
    if (!MID) return nullptr;
    MID->SetTextureParameterValue(TEXT("PageTable"), PageTex);
    MID->SetTextureParameterValue(TEXT("Atlas"),     AtlasTex);
    MID->SetVectorParameterValue(TEXT("CanvasMin"),  A.CanvasMin);
    MID->SetScalarParameterValue(TEXT("ColorVoxel"), A.ColorVoxel);
    MID->SetVectorParameterValue(TEXT("VoxDim"),     FVector(A.VoxDim));
    MID->SetVectorParameterValue(TEXT("BrickDim"),   FVector(A.BrickDim));
    MID->SetScalarParameterValue(TEXT("PageW"),      A.BrickDim.X);
    MID->SetScalarParameterValue(TEXT("PageH"),      A.BrickDim.Y * A.BrickDim.Z);
    MID->SetScalarParameterValue(TEXT("TilesPerRow"), A.TilesPerRow);
    MID->SetScalarParameterValue(TEXT("AtlasW"),     A.AtlasW);
    MID->SetScalarParameterValue(TEXT("AtlasH"),     A.AtlasH);
    MID->SetScalarParameterValue(TEXT("CB"),         A.CB);
    MID->SetScalarParameterValue(TEXT("NewClayGlowBrightness"), 0.f); // sin brillo de arcilla nueva
    // Sol del ambiente (por si el material cel-shadea): mismo parámetro que el resto.
    const float Pitch = FMath::Lerp(0.f, -180.f, FMath::Clamp(SkySettings.TimeOfDay, 0.f, 1.f));
    const FVector SunDir = -FRotator(Pitch, SkySettings.SunYaw, 0.f).Vector();
    MID->SetVectorParameterValue(TEXT("SunDir"), FLinearColor(SunDir.X, SunDir.Y, SunDir.Z, 0.f));

    AssetMIDs.Add(MID); // anti-GC
    return MID;
}

// ── (8b) Decimado por agrupación en grilla (vertex clustering): junta los vértices que caen en la misma
// celda de tamaño Cell en un representante (posición/normal/color promediados) y remapea los triángulos,
// descartando los degenerados. Barato, robusto (no deja huecos) y buena calidad para formas de arcilla vistas
// de lejos. NO genera UVs (por eso solo se usa en assets sin pintura). Cell<=0 → devuelve la malla igual. ──
static FPTPropGeometry PT_DecimateGeo(const FPTPropGeometry& G, float Cell)
{
    if (!G.IsValid() || Cell <= KINDA_SMALL_NUMBER) return G;

    const int32 NV = G.Verts.Num();
    TMap<FIntVector, int32> CellToNew;
    CellToNew.Reserve(NV);
    TArray<FVector3f> AccPos, AccNrm;
    TArray<FVector4f> AccCol;
    TArray<int32>     AccCnt;
    TArray<int32>     Remap; Remap.SetNumUninitialized(NV);
    const float Inv = 1.f / Cell;

    for (int32 i = 0; i < NV; ++i)
    {
        const FVector3f& V = G.Verts[i];
        const FIntVector C(FMath::FloorToInt(V.X * Inv), FMath::FloorToInt(V.Y * Inv), FMath::FloorToInt(V.Z * Inv));
        int32 Idx;
        if (int32* Found = CellToNew.Find(C)) { Idx = *Found; }
        else
        {
            Idx = AccPos.Num();
            CellToNew.Add(C, Idx);
            AccPos.Add(FVector3f::ZeroVector); AccNrm.Add(FVector3f::ZeroVector);
            AccCol.Add(FVector4f(0.f, 0.f, 0.f, 0.f)); AccCnt.Add(0);
        }
        AccPos[Idx] += V;
        AccNrm[Idx] += (G.Normals.IsValidIndex(i) ? G.Normals[i] : FVector3f::ZAxisVector);
        const FColor Cc = G.Colors.IsValidIndex(i) ? G.Colors[i] : FColor::White;
        AccCol[Idx] += FVector4f(Cc.R, Cc.G, Cc.B, Cc.A);
        AccCnt[Idx] += 1;
        Remap[i] = Idx;
    }

    FPTPropGeometry Out;
    const int32 NN = AccPos.Num();
    Out.Verts.SetNumUninitialized(NN);
    Out.Normals.SetNumUninitialized(NN);
    Out.Colors.SetNumUninitialized(NN);
    for (int32 k = 0; k < NN; ++k)
    {
        const float N = (float)FMath::Max(1, AccCnt[k]);
        Out.Verts[k]   = AccPos[k] / N;
        Out.Normals[k] = (AccNrm[k] / N).GetSafeNormal();
        const FVector4f Col = AccCol[k] / N;
        Out.Colors[k]  = FColor((uint8)FMath::Clamp(Col.X, 0.f, 255.f), (uint8)FMath::Clamp(Col.Y, 0.f, 255.f),
                                (uint8)FMath::Clamp(Col.Z, 0.f, 255.f), (uint8)FMath::Clamp(Col.W, 0.f, 255.f));
    }
    Out.Tris.Reserve(G.Tris.Num());
    for (int32 t = 0; t + 2 < G.Tris.Num(); t += 3)
    {
        const int32 A = Remap[G.Tris[t]], B = Remap[G.Tris[t + 1]], C = Remap[G.Tris[t + 2]];
        if (A != B && B != C && A != C) { Out.Tris.Add(A); Out.Tris.Add(B); Out.Tris.Add(C); }
    }
    // Si el decimado no redujo nada (asset muy chico), devolver la original para no gastar una copia inútil.
    return Out.IsValid() ? Out : G;
}

// Deciman hasta dejar ~KeepRatio de los triángulos originales (KeepRatio = 1 - %reducción). Como el conteo de
// triángulos DECRECE monótonamente con el tamaño de celda, se hace una búsqueda binaria del cell que alcanza el
// objetivo. Se corre una sola vez por asset al hornear/cargar (no por frame). Radius = radio bbox del asset.
static FPTPropGeometry PT_DecimateToTriRatio(const FPTPropGeometry& G, float KeepRatio, float Radius)
{
    if (!G.IsValid()) return FPTPropGeometry();
    KeepRatio = FMath::Clamp(KeepRatio, 0.05f, 0.99f);
    const int32 SrcTris    = G.Tris.Num() / 3;
    const int32 TargetTris = FMath::Max(2, FMath::RoundToInt(SrcTris * KeepRatio));
    if (TargetTris >= SrcTris) return FPTPropGeometry(); // no hay nada que reducir

    float Lo = FMath::Max(1.f, Radius * 0.01f); // celda chica → casi full
    float Hi = FMath::Max(Lo * 2.f, Radius * 0.80f); // celda grande → muy reducido
    FPTPropGeometry Best = PT_DecimateGeo(G, Hi);     // fallback: el más reducido
    for (int32 It = 0; It < 7; ++It)
    {
        const float Mid = 0.5f * (Lo + Hi);
        FPTPropGeometry Cand = PT_DecimateGeo(G, Mid);
        const int32 CandTris = Cand.Tris.Num() / 3;
        if (CandTris > TargetTris) { Lo = Mid; }              // todavía tiene de más → celda más grande
        else                       { Hi = Mid; Best = MoveTemp(Cand); } // ya bajó del objetivo → guardar y afinar
    }
    return Best.IsValid() ? Best : FPTPropGeometry();
}

int32 APTMapEnvironment::AddAsset(const FPTPropGeometry& Geo, const FPTPropGeometry& EyesGeo, const FPTPaintAtlas& PaintAtlas)
{
    if (!Geo.IsValid()) return INDEX_NONE;

    UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
    PMC->SetupAttachment(GetRootComponent());
    PMC->RegisterComponent();
    PMC->SetMobility(EComponentMobility::Movable);
    PMC->bUseComplexAsSimpleCollision = true;
    // Cocinar la colisión en un hilo aparte (no bloquear el game thread al cargar mapas densos: si no, la
    // carga tarda segundos y el cliente se desconecta por ConnectionTimeout).
    PMC->bUseAsyncCooking = true;
    PMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    PMC->SetCollisionObjectType(ECC_WorldStatic);

    FPTPropAsset A;
    A.Geo = Geo;
    A.EyesGeo = EyesGeo;
    A.PMC = PMC;
    A.PaintAtlas = PaintAtlas;
    A.PaintMID = PaintAtlas.bValid ? MakePaintMID(PaintAtlas) : nullptr; // pintura nítida (atlas) si hay
    // (8b) 3 mallas decimadas para el LOD por distancia. Solo assets SIN pintura (no re-generamos UVs del atlas).
    if (!A.PaintMID)
    {
        FBox3f Box(ForceInit);
        for (const FVector3f& V : Geo.Verts) Box += V;
        const float Radius = FMath::Max(1.f, Box.GetExtent().Size());
        const float Reduce[3] = { LODStage1Reduce, LODStage2Reduce, LODStage3Reduce };
        for (int32 s = 0; s < 3; ++s)
            A.LODGeo[s] = PT_DecimateToTriRatio(Geo, 1.f - Reduce[s], Radius); // vacía si no hubo ahorro
    }
    return Assets.Add(MoveTemp(A));
}

UMaterialInterface* APTMapEnvironment::GetAssetPreviewMaterial(int32 AssetIdx)
{
    if (Assets.IsValidIndex(AssetIdx) && Assets[AssetIdx].PaintMID) return Assets[AssetIdx].PaintMID;
    return GetPropMaterialForPreview();
}

const FPTPropGeometry* APTMapEnvironment::GetAssetGeometry(int32 AssetIdx) const
{
    return Assets.IsValidIndex(AssetIdx) ? &Assets[AssetIdx].Geo : nullptr;
}

const FPTPropGeometry* APTMapEnvironment::GetAssetEyesGeometry(int32 AssetIdx) const
{
    if (!Assets.IsValidIndex(AssetIdx)) return nullptr;
    const FPTPropGeometry& E = Assets[AssetIdx].EyesGeo;
    return E.IsValid() ? &E : nullptr;
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
    if (UMaterialInterface* Mat = GetAssetPreviewMaterial(AssetIdx)) MC->SetMaterial(0, Mat);
    // Ojos en la miniatura (sección 1 con su material), si el asset tiene.
    if (const FPTPropGeometry* EG = GetAssetEyesGeometry(AssetIdx))
    {
        FillProcSection(MC, 1, *EG, FTransform::Identity, /*bCollision=*/false);
        if (EyeMaterial) MC->SetMaterial(1, EyeMaterial);
    }
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

void APTMapEnvironment::BuildMergedSection(FPTPropAsset& A, const FPTPropGeometry& G, int32 Section, UMaterialInterface* Mat,
                                           const TArray<FTransform>& Xfs, bool bCollision)
{
    if (!A.PMC) return;
    if (!G.IsValid() || Xfs.Num() == 0) { A.PMC->ClearMeshSection(Section); return; }

    const int32 NVper = G.Verts.Num();
    const int32 NTper = G.Tris.Num();
    const int32 NInst = Xfs.Num();
    if (NInst == 0) { A.PMC->ClearMeshSection(Section); return; }
    const bool  bHasUV = (G.UV0.Num() == NVper && G.UV1.Num() == NVper); // UV de lookup del atlas de pintura
    TArray<FVector> Verts;   Verts.Reserve(NVper * NInst);
    TArray<FVector> Normals; Normals.Reserve(NVper * NInst);
    TArray<FColor>  Colors;  Colors.Reserve(NVper * NInst);
    TArray<int32>   Tris;    Tris.Reserve(NTper * NInst);
    TArray<FVector2D> UV0, UV1;
    if (bHasUV) { UV0.Reserve(NVper * NInst); UV1.Reserve(NVper * NInst); }

    for (const FTransform& Xf : Xfs)
    {
        const int32 Base = Verts.Num();
        for (int32 i = 0; i < NVper; ++i)
        {
            Verts.Add(Xf.TransformPosition(FVector(G.Verts[i])));
            Normals.Add(Xf.TransformVectorNoScale(FVector(G.Normals.IsValidIndex(i) ? G.Normals[i] : FVector3f::ZAxisVector)));
            Colors.Add(G.Colors.IsValidIndex(i) ? G.Colors[i] : FColor::White);
            // La UV del atlas es la MISMA para todas las instancias (posición volumen-local del asset).
            if (bHasUV) { UV0.Add(FVector2D(G.UV0[i])); UV1.Add(FVector2D(G.UV1[i])); }
        }
        for (int32 t = 0; t < NTper; ++t) Tris.Add(Base + G.Tris[t]);
    }

    const TArray<FVector2D> NoUV;
    const TArray<FProcMeshTangent> NoTan;
    A.PMC->ClearMeshSection(Section);
    A.PMC->CreateMeshSection(Section, Verts, Tris, Normals, UV0, UV1, NoUV, NoUV, Colors, NoTan, bCollision);
    if (Mat) A.PMC->SetMaterial(Section, Mat);
}

AActor* APTMapEnvironment::GetPlayAreaActor()
{
    if (PlayAreaActor.IsValid()) return PlayAreaActor.Get();
    UWorld* W = GetWorld();
    if (!W || PlayAreaTag.IsNone()) return nullptr;
    TArray<AActor*> Found;
    UGameplayStatics::GetAllActorsWithTag(W, PlayAreaTag, Found);
    if (Found.Num() > 0) { PlayAreaActor = Found[0]; return Found[0]; }
    return nullptr;
}

bool APTMapEnvironment::GetPlayAreaCylinder(FVector& OutCenter, float& OutRadius, float& OutHalfHeight)
{
    AActor* A = GetPlayAreaActor();
    if (!A) return false;
    FVector Origin, Extent;
    A->GetActorBounds(/*bOnlyCollidingComponents=*/false, Origin, Extent); // bounds del actor (incluye no-colisionables)
    OutCenter     = Origin;
    OutRadius     = FMath::Max(Extent.X, Extent.Y); // cilindro que abarca el bounds en XY
    OutHalfHeight = Extent.Z;
    return OutRadius > KINDA_SMALL_NUMBER && OutHalfHeight > KINDA_SMALL_NUMBER;
}

bool APTMapEnvironment::IsInPlayArea(const FVector& WorldLoc)
{
    FVector C; float R, H;
    if (!GetPlayAreaCylinder(C, R, H)) return true; // sin área definida → todo cuenta como "dentro" (colisión normal)
    if (FMath::Abs(WorldLoc.Z - C.Z) > H) return false;             // fuera en altura
    const float dx = WorldLoc.X - C.X, dy = WorldLoc.Y - C.Y;
    return (dx * dx + dy * dy) <= (R * R);                          // dentro del radio (XY)
}

FVector APTMapEnvironment::GetLODOrigin()
{
    // Origen = centro del cilindro del área jugable si existe; si no, la ubicación del actor (origen del mapa).
    FVector C; float R, H;
    if (GetPlayAreaCylinder(C, R, H)) return C;
    return GetActorLocation();
}

void APTMapEnvironment::RebuildAssetMesh(FPTPropAsset& A)
{
    if (!A.PMC) return;
    if (A.InstXf.Num() == 0)
    {
        for (int32 s = 0; s <= 5; ++s) A.PMC->ClearMeshSection(s);
        return;
    }

    // ¿Aplicar LOD? Solo en partida (bLODEnabled) y si el asset tiene mallas decimadas (assets sin pintura).
    // El nivel de cada instancia es FIJO: se decide una vez por su distancia al ORIGEN (no cambia con el player).
    const bool bUseLOD = bLODEnabled && (A.LODGeo[0].IsValid() || A.LODGeo[1].IsValid() || A.LODGeo[2].IsValid());
    const FVector Origin = GetLODOrigin();

    // Repartos de instancias por etapa:
    //   Etapa 0 (cerca)  → malla FULL. Se sub-parte por área para la colisión (8a): dentro = con colisión.
    //   Etapas 1/2/3     → malla decimada (más reducida cuanto más lejos), sin colisión.
    TArray<FTransform> Stage0In, Stage0Out, Stage1, Stage2, Stage3;
    for (const FTransform& Xf : A.InstXf)
    {
        const FVector Loc = Xf.GetLocation();
        int32 Stage = 0;
        if (bUseLOD)
        {
            const float D = FVector::Dist(Loc, Origin);
            if      (D >= LODStage3Dist) Stage = 3;
            else if (D >= LODStage2Dist) Stage = 2;
            else if (D >= LODStage1Dist) Stage = 1;
        }
        switch (Stage)
        {
            case 1:  Stage1.Add(Xf); break;
            case 2:  Stage2.Add(Xf); break;
            case 3:  Stage3.Add(Xf); break;
            default: (IsInPlayArea(Loc) ? Stage0In : Stage0Out).Add(Xf); break;
        }
    }

    // Arcilla: material con atlas (nítido) si el asset tiene pintura; si no, el normal.
    UMaterialInterface* ClayMat = A.PaintMID ? (UMaterialInterface*)A.PaintMID : GetOrCreatePropMID();
    if (!ClayMat) ClayMat = PropMaterial;

    // Etapa 0: sección 0 = dentro del área (full, con colisión), sección 2 = fuera (full, sin colisión).
    BuildMergedSection(A, A.Geo, 0, ClayMat, Stage0In,  /*bCollision=*/true);
    BuildMergedSection(A, A.Geo, 2, ClayMat, Stage0Out, /*bCollision=*/false);
    // Etapas 1/2/3: secciones 3/4/5 con la malla decimada correspondiente (o la full si esa etapa no ahorró).
    BuildMergedSection(A, A.LODGeo[0].IsValid() ? A.LODGeo[0] : A.Geo, 3, ClayMat, Stage1, /*bCollision=*/false);
    BuildMergedSection(A, A.LODGeo[1].IsValid() ? A.LODGeo[1] : A.Geo, 4, ClayMat, Stage2, /*bCollision=*/false);
    BuildMergedSection(A, A.LODGeo[2].IsValid() ? A.LODGeo[2] : A.Geo, 5, ClayMat, Stage3, /*bCollision=*/false);

    // Sección 1 = OJOS (todas las instancias, sin colisión). Vacía si el asset no tiene ojos.
    if (A.EyesGeo.IsValid()) BuildMergedSection(A, A.EyesGeo, 1, EyeMaterial, A.InstXf, /*bCollision=*/false);
    else                     A.PMC->ClearMeshSection(1);
}

void APTMapEnvironment::SetLODEnabled(bool bOn)
{
    if (bLODEnabled == bOn) return;
    bLODEnabled = bOn;
    // El LOD es estático (se decide al reconstruir): basta reconstruir todos los assets una vez al cambiar.
    for (FPTPropAsset& A : Assets) RebuildAssetMesh(A);
    if (bLODDebug) DrawLODDebug(); // refrescar el overlay
}

void APTMapEnvironment::RebuildAllLODMeshes()
{
    const float Reduce[3] = { LODStage1Reduce, LODStage2Reduce, LODStage3Reduce };
    for (FPTPropAsset& A : Assets)
    {
        if (A.PaintMID) continue; // assets con pintura: quedan full (no se decima para no romper el atlas)
        FBox3f Box(ForceInit);
        for (const FVector3f& V : A.Geo.Verts) Box += V;
        const float Radius = FMath::Max(1.f, Box.GetExtent().Size());
        for (int32 s = 0; s < 3; ++s)
            A.LODGeo[s] = PT_DecimateToTriRatio(A.Geo, 1.f - Reduce[s], Radius);
    }
    for (FPTPropAsset& A : Assets) RebuildAssetMesh(A);
}

void APTMapEnvironment::SetLODDebug(bool bOn)
{
    if (bLODDebug == bOn) return;
    bLODDebug = bOn;
    UWorld* W = GetWorld();
    if (!W) return;
    if (bOn)
    {
        W->GetTimerManager().SetTimer(LODDebugTimer, this, &APTMapEnvironment::DrawLODDebug, 0.25f, true, 0.f);
    }
    else
    {
        W->GetTimerManager().ClearTimer(LODDebugTimer);
        if (GEngine) GEngine->RemoveOnScreenDebugMessage((uint64)9231);
    }
}

void APTMapEnvironment::DrawLODDebug()
{
    UWorld* W = GetWorld();
    if (!W) return;
    const float Life = 0.30f; // un poco más que el intervalo del timer (0.25) para que no parpadee
    const FVector Origin = GetLODOrigin();

    // Cilindro del área jugable (cian).
    FVector C; float R, H;
    if (GetPlayAreaCylinder(C, R, H))
        DrawDebugCylinder(W, C - FVector(0, 0, H), C + FVector(0, 0, H), R, 24, FColor::Cyan, false, Life, 0, 2.f);

    static const FColor StageCol[4] = { FColor::Green, FColor::Yellow, FColor(255, 140, 0), FColor::Red };
    int32 InstByStage[4] = { 0, 0, 0, 0 };
    int64 TrisByStage[4] = { 0, 0, 0, 0 };
    int32 TotalInst = 0;

    for (const FPTPropAsset& A : Assets)
    {
        const bool bHasLOD = bLODEnabled && (A.LODGeo[0].IsValid() || A.LODGeo[1].IsValid() || A.LODGeo[2].IsValid());
        for (const FTransform& Xf : A.InstXf)
        {
            const FVector L = Xf.GetLocation();
            int32 St = 0;
            if (bHasLOD)
            {
                const float D = FVector::Dist(L, Origin);
                if      (D >= LODStage3Dist) St = 3;
                else if (D >= LODStage2Dist) St = 2;
                else if (D >= LODStage1Dist) St = 1;
            }
            ++InstByStage[St]; ++TotalInst;
            // Poste vertical + esfera arriba, en foreground (SDPG_Foreground=1) para verlos a través de la
            // geometría y desde lejos. Verde=full, amarillo/naranja/rojo = etapas 1/2/3.
            DrawDebugLine(W, L, L + FVector(0, 0, 400.f), StageCol[St], false, Life, /*DepthPrio=*/1, 6.f);
            DrawDebugSphere(W, L + FVector(0, 0, 400.f), 45.f, 8, StageCol[St], false, Life, /*DepthPrio=*/1, 4.f);
        }
        // Triángulos realmente dibujados por sección: 0+2 = etapa0 (full), 3/4/5 = etapas 1/2/3 (decimadas).
        if (A.PMC)
        {
            auto SecTris = [&](int32 s) -> int64
            {
                const FProcMeshSection* Se = A.PMC->GetProcMeshSection(s);
                return Se ? (int64)(Se->ProcIndexBuffer.Num() / 3) : 0;
            };
            TrisByStage[0] += SecTris(0) + SecTris(2);
            TrisByStage[1] += SecTris(3);
            TrisByStage[2] += SecTris(4);
            TrisByStage[3] += SecTris(5);
        }
    }

    const int64 TotalTris = TrisByStage[0] + TrisByStage[1] + TrisByStage[2] + TrisByStage[3];
    if (GEngine)
    {
        const FString Msg = FString::Printf(
            TEXT("[LOD %s]  Instancias: %d   Triangulos: %lld\n")
            TEXT("  E0 full : %d inst / %lld tris\n")
            TEXT("  E1 -%d%% : %d inst / %lld tris\n")
            TEXT("  E2 -%d%% : %d inst / %lld tris\n")
            TEXT("  E3 -%d%% : %d inst / %lld tris"),
            bLODEnabled ? TEXT("ON") : TEXT("OFF (autoria)"),
            TotalInst, TotalTris,
            InstByStage[0], TrisByStage[0],
            FMath::RoundToInt(LODStage1Reduce * 100.f), InstByStage[1], TrisByStage[1],
            FMath::RoundToInt(LODStage2Reduce * 100.f), InstByStage[2], TrisByStage[2],
            FMath::RoundToInt(LODStage3Reduce * 100.f), InstByStage[3], TrisByStage[3]);
        GEngine->AddOnScreenDebugMessage((uint64)9231, 0.35f, FColor::White, Msg);
    }
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
    PropMID = nullptr;   // se recrea en el próximo mapa/ambiente
    AssetMIDs.Reset();   // libera los MID de pintura por asset
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

// v5: snapshot del atlas de pintura por asset (page table + atlas + parámetros).
static void PT_SerializePaintAtlas(FArchive& Ar, FPTPaintAtlas& P)
{
    Ar << P.bValid;
    if (!P.bValid) return;
    Ar << P.PageBuf; Ar << P.AtlasBuf;
    Ar << P.CanvasMin; Ar << P.ColorVoxel;
    Ar << P.VoxDim; Ar << P.BrickDim;
    Ar << P.TilesPerRow; Ar << P.AtlasW; Ar << P.AtlasH; Ar << P.CB;
}

void APTMapEnvironment::SerializeEnvironment(TArray<uint8>& Out)
{
    Out.Reset();
    FMemoryWriter Ar(Out, /*bIsPersistent=*/true);
    int32 Version = 5; Ar << Version; // v5: + UV de atlas por vértice + snapshot del atlas de pintura por asset
    PT_SerializeSky(Ar, SkySettings, Version);
    int32 NumAssets = Assets.Num(); Ar << NumAssets;
    for (FPTPropAsset& A : Assets)
    {
        Ar << A.Geo.Verts;
        Ar << A.Geo.Normals;
        Ar << A.Geo.Colors;
        Ar << A.Geo.Tris;
        Ar << A.Geo.UV0;   // v5: UV de lookup del atlas de pintura
        Ar << A.Geo.UV1;
        // v4: malla de OJOS (sección aparte). Se guarda siempre (puede estar vacía).
        Ar << A.EyesGeo.Verts;
        Ar << A.EyesGeo.Normals;
        Ar << A.EyesGeo.Colors;
        Ar << A.EyesGeo.Tris;
        PT_SerializePaintAtlas(Ar, A.PaintAtlas); // v5: pintura nítida (atlas)
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
        FPTPropGeometry Geo, EyesGeo;
        FPTPaintAtlas   Atlas;
        Ar << Geo.Verts;
        Ar << Geo.Normals;
        Ar << Geo.Colors;
        Ar << Geo.Tris;
        if (Version >= 5) { Ar << Geo.UV0; Ar << Geo.UV1; } // v5: UV de lookup del atlas
        if (Version >= 4) // v4+: geometría de ojos (mapas viejos no la tienen → sin ojos, compatible)
        {
            Ar << EyesGeo.Verts;
            Ar << EyesGeo.Normals;
            Ar << EyesGeo.Colors;
            Ar << EyesGeo.Tris;
        }
        if (Version >= 5) PT_SerializePaintAtlas(Ar, Atlas); // v5: snapshot del atlas de pintura (nítida)
        const int32 Idx = AddAsset(Geo, EyesGeo, Atlas);
        int32 NInst = 0; Ar << NInst;
        // Carga en BULK: acumular TODAS las instancias y reconstruir la malla combinada UNA sola vez por
        // asset. (Antes llamaba PlaceInstance por instancia → reconstruía la malla completa N veces = O(N²)
        // + colisión N veces → bloqueaba el game thread varios segundos y el cliente se desconectaba por
        // ConnectionTimeout al cargar mapas densos.)
        for (int32 i = 0; i < NInst; ++i)
        {
            FTransform Xf; Ar << Xf;
            if (Idx != INDEX_NONE)
            {
                Assets[Idx].InstXf.Add(Xf);
                PlaceOrder.Add(Idx);
            }
        }
        if (Idx != INDEX_NONE) RebuildAssetMesh(Assets[Idx]); // una sola reconstrucción (O(N))
    }
    ApplySkySettings(); // aplicar el ambiente cargado (sol/cielo/niebla) en esta máquina
}
