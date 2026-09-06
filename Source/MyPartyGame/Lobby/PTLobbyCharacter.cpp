// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyCharacter.h"
#include "PTPlayerState.h"
#include "PTNameTagWidget.h"
#include "../PTTextTable.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/ArrowComponent.h"
#include "Animation/AnimInstance.h"
#include "ProceduralMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "../Sculpt/PTSculptVolume.h"
#include "PTPlayerState.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DirectionalLight.h"
#include "Components/DirectionalLightComponent.h"
#include "StaticMeshResources.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Compression.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Modules/ModuleManager.h"
#include "Engine/Engine.h" // GEngine->AddOnScreenDebugMessage (debug de tamaño del blob)
#include "PTLockerSubsystem.h"
#include "../Multiplayer/MultiplayerSessionsSubsystem.h" // nick local de Steam (fallback del nametag propio)
#include "../PTGameInstance.h" // modo captura dev (Player N / ocultar nombres)
#include "Engine/GameInstance.h"
#include "Engine/SceneCapture2D.h"
#include "Components/SceneCaptureComponent2D.h"
#include "TextureResource.h"

// Forward-decls de helpers estáticos (definidos más abajo en este archivo).
static bool PT_EncodePNG_BGRA(const TArray<FColor>& Px, int32 N, TArray<uint8>& Out);
static bool PT_DecodePNG_BGRA(const TArray<uint8>& In, TArray<FColor>& OutPx, int32& OutN);
static void PT_SerializeHeadBlob(const TArray<uint8>& Geo, const FVector& Center,
                                 const TArray<uint8>& HeadPNG, const TArray<uint8>& BodyPNG, TArray<uint8>& Out);
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/Texture2D.h"
#include "TextureResource.h"
#include "RenderUtils.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Engine/Canvas.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "GameFramework/PlayerController.h"

APTLobbyCharacter::APTLobbyCharacter()
{
    PrimaryActorTick.bCanEverTick = true; // vuelo aplica input vertical por tick

    // Cámara en primera persona: directo en el pawn, a la altura de los ojos.
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(RootComponent);
    Camera->SetRelativeLocation(FVector(0.f, 0.f, BaseEyeHeight));
    Camera->bUsePawnControlRotation = true;

    // El personaje gira hacia donde se mueve, no hacia donde apunta la cámara
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);

    // Vuelo con inercia sutil: frena rápido pero con un pequeño deslizamiento.
    GetCharacterMovement()->BrakingDecelerationFlying = 2500.f;
    GetCharacterMovement()->bUseSeparateBrakingFriction = true;
    GetCharacterMovement()->BrakingFriction = 2.f;
    DefaultMaxAccel = GetCharacterMovement()->MaxAcceleration; // para restaurar al caminar

    // ACharacter + CharacterMovementComponent replican movimiento y rotación automáticamente.
    SetReplicates(true);
    SetReplicateMovement(true);

    // Cartel del nombre: atachado al hueso de la cabeza (Bone_008) del Mesh, así sigue
    // el jiggle de la cabeza. Screen Space = siempre mira a la cámara.
    NameTag = CreateDefaultSubobject<UWidgetComponent>(TEXT("NameTag"));
    NameTag->SetupAttachment(GetMesh(), TEXT("Bone_008"));
    NameTag->SetRelativeLocation(FVector(0.f, 0.f, 20.f)); // apenas arriba del hueso; ajustar en el BP
    NameTag->SetWidgetSpace(EWidgetSpace::Screen);
    NameTag->SetDrawSize(FVector2D(200.f, 50.f));

    // Cabeza custom: malla procedural pegada al socket "HeadSocket" del mesh (baila con la cabeza).
    // Arranca vacía; se llena con la cabeza esculpida por el jugador (SetHeadMeshFrom).
    HeadMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("HeadMesh"));
    HeadMesh->SetupAttachment(GetMesh(), TEXT("HeadSocket"));
    HeadMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HeadMesh->bUseComplexAsSimpleCollision = false;

    // Flecha "hacia dónde mira": pegada al root, apunta al forward del actor (+X). Oculta por
    // defecto; sólo se muestra en modo esculpir-cabeza. Se puede reubicar/recolorear en el BP.
    FacingArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("FacingArrow"));
    FacingArrow->SetupAttachment(RootComponent);
    FacingArrow->SetRelativeLocation(FVector(0.f, 0.f, 90.f));
    FacingArrow->ArrowSize = 1.5f;
    FacingArrow->ArrowLength = 120.f;
    FacingArrow->SetArrowColor(FLinearColor(0.3f, 0.8f, 1.f));
    FacingArrow->bIsScreenSizeScaled = false;
    FacingArrow->SetHiddenInGame(true); // visible sólo en modo G (SetFacingArrowVisible)
}

static const TCHAR* PTHeadSaveSlot = TEXT("PTHeadCustom");

TArray<FPTHeadSection> APTLobbyCharacter::ExtractSections(UProceduralMeshComponent* Src, const APTSculptVolume* PaintSource) const
{
    TArray<FPTHeadSection> Out;
    if (!Src) return Out;
    const int32 N = Src->GetNumSections();
    for (int32 s = 0; s < N; ++s)
    {
        const FProcMeshSection* Sec = Src->GetProcMeshSection(s);
        if (!Sec || Sec->ProcVertexBuffer.Num() == 0) continue;
        FPTHeadSection H;
        H.Verts.Reserve(Sec->ProcVertexBuffer.Num());
        H.Normals.Reserve(Sec->ProcVertexBuffer.Num());
        H.UVs.Reserve(Sec->ProcVertexBuffer.Num());
        H.Colors.Reserve(Sec->ProcVertexBuffer.Num());
        const FTransform SrcXform = Src->GetComponentTransform();
        for (const FProcMeshVertex& V : Sec->ProcVertexBuffer)
        {
            H.Verts.Add(V.Position); H.Normals.Add(V.Normal);
            // UV0 en el clay guarda el "tiempo de agregado" para el brillo de la arcilla nueva.
            // En la cabeza HORNEADA no queremos ese brillo (el NowTime del material del head no se
            // actualiza y el glow quedaría clavado al máximo). Se hornea con tiempo "muy viejo" para
            // que el término del glow dé 0 → la cabeza sale con su color normal.
            H.UVs.Add(FVector2D(-1.e9f, 0.f));
            // Color base = vertex color del campo (lo que pintás con Add-con-color). Si se pasó el
            // volumen y ese vértice fue PINTADO (modo Paint → atlas), usar ese color en su lugar,
            // para que la pintura quede horneada en el vertex color del mesh de la cabeza.
            FColor Col = V.Color;
            if (PaintSource)
            {
                bool bPainted = false;
                const FLinearColor PC = PaintSource->SampleWorldPaintColor(SrcXform.TransformPosition(FVector(V.Position)), bPainted);
                // MISMA conversión que usa el mesher del clay para el vertex color (si no, la pintura
                // horneada sale más pálida que en vivo): tomar los bytes sRGB y reinterpretarlos como
                // lineales (ToFColor(false)). El color base (V.Color) ya viene así del mesher.
                if (bPainted) Col = FLinearColor(PC.ToFColor(true)).ToFColor(false);
            }
            H.Colors.Add(Col);
        }
        H.Tris.Reserve(Sec->ProcIndexBuffer.Num());
        for (uint32 Idx : Sec->ProcIndexBuffer) H.Tris.Add((int32)Idx);
        Out.Add(MoveTemp(H));
    }
    return Out;
}

void APTLobbyCharacter::ApplyHeadSections(const TArray<FPTHeadSection>& Secs)
{
    if (!HeadMesh) return;
    HeadMesh->ClearAllMeshSections();
    const TArray<FProcMeshTangent> NoTangents;
    for (int32 s = 0; s < Secs.Num(); ++s)
    {
        const FPTHeadSection& H = Secs[s];
        if (H.Verts.Num() == 0) continue;
        HeadMesh->CreateMeshSection(s, H.Verts, H.Tris, H.Normals, H.UVs, H.Colors, NoTangents, /*bCreateCollision=*/false);
        if (H.bEye && EyeMaterial) { HeadMesh->SetMaterial(s, EyeMaterial); continue; }
        // Arcilla: si tenemos el MID de color (muestrea el atlas 3D → idéntico al vivo), usarlo;
        // si no, el material plano que lee el vertex color (fallback / pawns remotos por ahora).
        if (HeadColorMID)      HeadMesh->SetMaterial(s, HeadColorMID);
        else if (HeadMaterial) HeadMesh->SetMaterial(s, HeadMaterial);
    }
}

void APTLobbyCharacter::BakeAndReplicateHead(UProceduralMeshComponent* ClaySrc, APTSculptVolume* PaintSource,
                                             const TArray<FVector4>& LocalEyes, TArray<uint8>& OutBlob)
{
    OutBlob.Reset();
    if (!HeadMesh || !ClaySrc) return;

    // Componer: arcilla base + CAPAS de detalle (lentes/bigote, mallas aparte) + sección de OJOS.
    TArray<FPTHeadSection> Secs = ExtractSections(ClaySrc, PaintSource);
    if (PaintSource)
        for (const auto& Pair : PaintSource->GetSVOChunkMeshes())
            if (Pair.Value) Secs.Append(ExtractSections(Pair.Value, PaintSource));
    if (PaintSource)
        for (UProceduralMeshComponent* DM : PaintSource->GetDetailMeshes())
            if (DM) Secs.Append(ExtractSections(DM, PaintSource));
    if (LocalEyes.Num() > 0)
    {
        FPTHeadSection Eyes = BuildEyesSection(LocalEyes, HeadEyeMesh, HeadEyeBaseSize);
        if (Eyes.Verts.Num() > 0) Secs.Add(MoveTemp(Eyes));
    }

    // La cabeza horneada usa el MISMO material/textura de pintura 2D que en vivo (idéntica a lo pintado).
    HeadColorMID = CreateHeadPaintMID();
    bLocallyBakedHead = true; // no pisar este resultado al re-aplicar desde el blob replicado

    ApplyHeadSections(Secs);
    UpdateHeadCollision();

    // Blob combinado (geometría + centro + PNG cabeza + PNG cuerpo). Guardar/equipar/subir lo hace el Locker.
    BuildHeadBlob(Secs, OutBlob);
}

bool APTLobbyCharacter::GetBodyPaintPNG(TArray<uint8>& OutPNG)
{
    return PaintPixels.Num() > 0 && PT_EncodePNG_BGRA(PaintPixels, PaintTexN, OutPNG);
}

bool APTLobbyCharacter::CaptureLookThumbnailPNG(TArray<uint8>& OutPNG, bool bHeadFocus, int32 Size)
{
    UWorld* W = GetWorld();
    if (!W || !GetMesh()) return false;
    Size = FMath::Clamp(Size, 64, 1024);

    UTextureRenderTarget2D* RT = UKismetRenderingLibrary::CreateRenderTarget2D(this, Size, Size, RTF_RGBA8);
    if (!RT) return false;

    ASceneCapture2D* Cap = W->SpawnActor<ASceneCapture2D>();
    if (!Cap) return false;
    USceneCaptureComponent2D* C = Cap->GetCaptureComponent2D();
    C->TextureTarget       = RT;
    C->CaptureSource       = ESceneCaptureSource::SCS_FinalColorLDR;
    C->bCaptureEveryFrame  = false;
    C->bCaptureOnMovement  = false;
    C->FOVAngle            = 45.f;
    C->PrimitiveRenderMode = ESceneCapturePrimitiveRenderMode::PRM_UseShowOnlyList;
    C->ShowOnlyActors.Add(this); // solo el personaje (+ lo que agreguemos abajo)
    C->ShowFlags.SetDynamicShadows(false); // miniatura LIMPIA: sin sombras proyectadas

    // Personaje QUIETO (pose recta) para que la miniatura no salga a mitad de una animación. Si ya
    // estaba en pose de esculpido (editando), no la tocamos; si no (bailando en el lobby), la congelamos.
    const bool bAlreadyPosed = GetMesh()->GetAnimationMode() == EAnimationMode::AnimationSingleNode;
    if (!bAlreadyPosed) SetSculptPose(true);
    else                GetMesh()->RefreshBoneTransforms();

    // Encuadre: cabeza (cerca de HeadMesh) o cuerpo entero. Centro con offset + cámara con pitch.
    FVector Center = bHeadFocus
        ? (HeadMesh ? HeadMesh->GetComponentLocation() : GetActorLocation() + FVector(0, 0, ThumbHeight)) + FVector(0, 0, ThumbHeadHeight)
        : (GetActorLocation() + FVector(0, 0, ThumbHeight));
    const float Dist  = bHeadFocus ? ThumbHeadDistance : ThumbDistance;
    const float Pitch = bHeadFocus ? ThumbHeadPitch    : ThumbBodyPitch;
    FVector Fwd = GetActorForwardVector().RotateAngleAxis(Pitch, GetActorRightVector());
    const FVector Loc = Center + Fwd * Dist;
    Cap->SetActorLocation(Loc);
    Cap->SetActorRotation((Center - Loc).Rotation());

    // Luz frontal LIMPIA sin sombras (para que no se vea oscuro ni con sombras del lobby).
    ADirectionalLight* Light = nullptr;
    if (ThumbLightIntensity > 0.f)
    {
        Light = W->SpawnActor<ADirectionalLight>();
        if (Light)
        {
            if (UDirectionalLightComponent* LC = Cast<UDirectionalLightComponent>(Light->GetLightComponent()))
            {
                LC->SetMobility(EComponentMobility::Movable);
                LC->SetIntensity(ThumbLightIntensity);
                LC->SetCastShadows(false);
                LC->SetLightColor(FLinearColor::White);
            }
            Light->SetActorRotation((Center - Loc).Rotation()); // apunta desde la cámara hacia el personaje
        }
    }

    // ── Fondo de color: plano detrás del personaje, de cara a la cámara ──
    AStaticMeshActor* Backdrop = nullptr;
    if (ThumbBackdropMaterial)
    {
        if (UStaticMesh* Plane = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")))
        {
            Backdrop = W->SpawnActor<AStaticMeshActor>();
            if (Backdrop)
            {
                UStaticMeshComponent* PM = Backdrop->GetStaticMeshComponent();
                PM->SetMobility(EComponentMobility::Movable);
                PM->SetStaticMesh(Plane);
                UMaterialInstanceDynamic* BgMID = UMaterialInstanceDynamic::Create(ThumbBackdropMaterial, this);
                if (BgMID) { BgMID->SetVectorParameterValue(TEXT("Color"), ThumbBgColor); PM->SetMaterial(0, BgMID); }
                const FVector BgFwd = (Center - Loc).GetSafeNormal();
                Backdrop->SetActorLocation(Center + BgFwd * 300.f);       // detrás del personaje
                Backdrop->SetActorRotation(FRotationMatrix::MakeFromZ(-BgFwd).Rotator()); // normal hacia la cámara
                Backdrop->SetActorScale3D(FVector(30.f));               // grande para llenar el cuadro
                C->ShowOnlyActors.Add(Backdrop);
            }
        }
    }

    // ── Aislar el foco: apagar/reemplazar la parte que no es el foco ──
    TArray<UMaterialInterface*> SavedBodyMats;
    UStaticMeshComponent* TempHead = nullptr;
    bool bHeadWasVisible = HeadMesh ? HeadMesh->IsVisible() : false;

    if (bHeadFocus)
    {
        // Cuerpo en material apagado/default (sin la pintura editada).
        if (ThumbDimMaterial)
        {
            const int32 NumMats = GetMesh()->GetNumMaterials();
            SavedBodyMats.SetNum(NumMats);
            for (int32 i = 0; i < NumMats; ++i)
            {
                SavedBodyMats[i] = GetMesh()->GetMaterial(i);
                GetMesh()->SetMaterial(i, ThumbDimMaterial);
            }
        }
    }
    else
    {
        // Ocultar la cabeza real y poner una ESFERA default apagada en su lugar.
        if (HeadMesh) HeadMesh->SetVisibility(false);
        if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
        {
            TempHead = NewObject<UStaticMeshComponent>(this);
            if (TempHead)
            {
                TempHead->SetupAttachment(GetRootComponent());
                TempHead->RegisterComponent();
                TempHead->SetStaticMesh(Sphere);
                TempHead->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                if (ThumbDimMaterial) TempHead->SetMaterial(0, ThumbDimMaterial);
                const FVector HeadLoc = HeadMesh ? HeadMesh->GetComponentLocation()
                                                 : GetActorLocation() + FVector(0, 0, ThumbHeight);
                const float R = FMath::Max(1.f, DefaultHeadRadius);
                TempHead->SetWorldLocation(HeadLoc);
                TempHead->SetWorldScale3D(FVector(R / 50.f)); // la esfera del engine mide 50 de radio
            }
        }
    }

    C->CaptureScene();

    FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource();
    TArray<FColor> Px;
    const bool bOk = Res && Res->ReadPixels(Px) && Px.Num() == Size * Size;

    // ── Restaurar todo ──
    if (bHeadFocus)
    {
        for (int32 i = 0; i < SavedBodyMats.Num(); ++i) GetMesh()->SetMaterial(i, SavedBodyMats[i]);
    }
    else
    {
        if (TempHead) TempHead->DestroyComponent();
        if (HeadMesh) HeadMesh->SetVisibility(bHeadWasVisible);
    }
    if (Backdrop) Backdrop->Destroy();
    if (Light)    Light->Destroy();
    Cap->Destroy();
    if (!bAlreadyPosed) SetSculptPose(false); // devolver la animación normal si la habíamos congelado

    if (!bOk) return false;
    for (FColor& P : Px) P.A = 255; // opaco (evita miniaturas "vacías" por alpha 0)
    return PT_EncodePNG_BGRA(Px, Size, OutPNG);
}

UTexture2D* APTLobbyCharacter::MakeTextureFromPNG(UObject* Outer, const TArray<uint8>& PNG)
{
    TArray<FColor> Px; int32 N = 0;
    if (PNG.Num() == 0 || !PT_DecodePNG_BGRA(PNG, Px, N) || N <= 0) return nullptr;
    UTexture2D* T = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8, NAME_None);
    if (!T) return nullptr;
    T->SRGB = true; T->Filter = TF_Bilinear;
    FTexture2DMipMap& Mip = T->GetPlatformData()->Mips[0];
    void* D = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(D, Px.GetData(), Px.Num() * sizeof(FColor));
    Mip.BulkData.Unlock();
    T->UpdateResource();
    return T;
}

void APTLobbyCharacter::AssembleReplicatedBlob(const TArray<uint8>& HeadBlob, const TArray<uint8>& BodyPNG, TArray<uint8>& Out)
{
    Out.Reset();
    TArray<FPTHeadSection> Secs; FVector Center; TArray<uint8> HeadPNG, OldBodyPNG;
    if (!ParseHeadBlob(HeadBlob, Secs, Center, HeadPNG, OldBodyPNG) || Secs.Num() == 0) { Out = HeadBlob; return; }

    TArray<uint8> Geo; SectionsToBlob(Secs, Geo);
    const TArray<uint8>& UseBody = (BodyPNG.Num() > 0) ? BodyPNG : OldBodyPNG; // vacío = conservar el que traía
    PT_SerializeHeadBlob(Geo, Center, HeadPNG, UseBody, Out);
}

void APTLobbyCharacter::ApplyReplicatedHead()
{
    // Si este pawn acaba de hornear su cabeza localmente, ya tiene el material/textura correctos;
    // no re-aplicar desde el blob para no rehacer trabajo (y evitar recrear texturas).
    if (bLocallyBakedHead) return;

    const APTPlayerState* PS = GetPlayerState<APTPlayerState>();
    if (!PS || PS->HeadBlob.Num() == 0) return;
    // Blob combinado: reconstruye geometría + texturas de pintura (cabeza + cuerpo) + materiales.
    ApplyHeadBlobLocal(PS->HeadBlob);
}

// ── Serialización + compresión (Zlib) de la malla de la cabeza ─────────────────
void APTLobbyCharacter::SectionsToBlob(const TArray<FPTHeadSection>& Secs, TArray<uint8>& OutBlob)
{
    // 1) Empaquetar crudo (verts/normales como float para achicar antes de comprimir).
    TArray<uint8> Raw;
    FMemoryWriter Ar(Raw);
    int32 N = Secs.Num(); Ar << N;
    for (const FPTHeadSection& S : Secs)
    {
        int32 NV = S.Verts.Num(); Ar << NV;
        for (int32 i = 0; i < NV; ++i)
        {
            FVector3f p = (FVector3f)S.Verts[i];
            FVector3f n = S.Normals.IsValidIndex(i) ? (FVector3f)S.Normals[i] : FVector3f::ZeroVector;
            FVector2f uv= S.UVs.IsValidIndex(i)     ? (FVector2f)S.UVs[i]     : FVector2f::ZeroVector;
            FColor    c = S.Colors.IsValidIndex(i)  ? S.Colors[i]             : FColor::White;
            Ar << p.X << p.Y << p.Z << n.X << n.Y << n.Z << uv.X << uv.Y << c;
        }
        int32 NT = S.Tris.Num(); Ar << NT;
        for (int32 t = 0; t < NT; ++t) { int32 v = S.Tris[t]; Ar << v; }
        uint8 eye = S.bEye ? 1 : 0; Ar << eye;
    }

    // 2) Comprimir (header = tamaño sin comprimir).
    const int32 Uncomp = Raw.Num();
    int32 CompSize = FCompression::CompressMemoryBound(NAME_Zlib, Uncomp);
    OutBlob.SetNumUninitialized(sizeof(int32) + CompSize);
    FMemory::Memcpy(OutBlob.GetData(), &Uncomp, sizeof(int32));
    if (FCompression::CompressMemory(NAME_Zlib, OutBlob.GetData() + sizeof(int32), CompSize, Raw.GetData(), Uncomp))
        OutBlob.SetNum(sizeof(int32) + CompSize, EAllowShrinking::No);
    else
        OutBlob.Reset();
}

bool APTLobbyCharacter::BlobToSections(const TArray<uint8>& Blob, TArray<FPTHeadSection>& OutSecs)
{
    if (Blob.Num() <= (int32)sizeof(int32)) return false;

    int32 Uncomp = 0;
    FMemory::Memcpy(&Uncomp, Blob.GetData(), sizeof(int32));
    if (Uncomp <= 0) return false;

    TArray<uint8> Raw; Raw.SetNumUninitialized(Uncomp);
    if (!FCompression::UncompressMemory(NAME_Zlib, Raw.GetData(), Uncomp,
                                        Blob.GetData() + sizeof(int32), Blob.Num() - sizeof(int32)))
        return false;

    FMemoryReader Ar(Raw);
    int32 N = 0; Ar << N;
    OutSecs.Reset();
    OutSecs.Reserve(N);
    for (int32 s = 0; s < N; ++s)
    {
        FPTHeadSection S;
        int32 NV = 0; Ar << NV;
        S.Verts.Reserve(NV); S.Normals.Reserve(NV); S.UVs.Reserve(NV); S.Colors.Reserve(NV);
        for (int32 i = 0; i < NV; ++i)
        {
            FVector3f p, n; FVector2f uv; FColor c;
            Ar << p.X << p.Y << p.Z << n.X << n.Y << n.Z << uv.X << uv.Y << c;
            S.Verts.Add(FVector(p)); S.Normals.Add(FVector(n)); S.UVs.Add(FVector2D(uv)); S.Colors.Add(c);
        }
        int32 NT = 0; Ar << NT;
        S.Tris.Reserve(NT);
        for (int32 t = 0; t < NT; ++t) { int32 v = 0; Ar << v; S.Tris.Add(v); }
        uint8 eye = 0; Ar << eye; S.bEye = (eye != 0);
        OutSecs.Add(MoveTemp(S));
    }
    return true;
}

FPTHeadSection APTLobbyCharacter::BuildEyesSection(const TArray<FVector4>& LocalEyes,
                                                   UStaticMesh* EyeMesh, float BaseSize, int32 Segments)
{
    FPTHeadSection S;
    S.bEye = true;
    BaseSize = FMath::Max(BaseSize, 1.f);

    // ── Mesh propio (si tiene datos CPU): copiar su geometría por cada ojo → queda todo un mesh ──
    const FStaticMeshLODResources* LOD = nullptr;
    if (EyeMesh && EyeMesh->bAllowCPUAccess && EyeMesh->GetRenderData()
        && EyeMesh->GetRenderData()->LODResources.Num() > 0)
        LOD = &EyeMesh->GetRenderData()->LODResources[0];

    if (LOD)
    {
        const FPositionVertexBuffer&   PB = LOD->VertexBuffers.PositionVertexBuffer;
        const FStaticMeshVertexBuffer& VB = LOD->VertexBuffers.StaticMeshVertexBuffer;
        const FIndexArrayView          Idx = LOD->IndexBuffer.GetArrayView();
        const int32 NV = (int32)PB.GetNumVertices();
        for (const FVector4& E : LocalEyes)
        {
            const FVector Center(E.X, E.Y, E.Z);
            const float   Scale = (float)E.W / BaseSize;
            const int32   Base  = S.Verts.Num();
            for (int32 i = 0; i < NV; ++i)
            {
                S.Verts.Add(Center + FVector(PB.VertexPosition(i)) * Scale);
                S.Normals.Add(FVector(VB.VertexTangentZ(i)));
                S.UVs.Add(FVector2D(VB.GetVertexUV(i, 0)));
                S.Colors.Add(FColor::White);
            }
            for (int32 t = 0; t + 2 < Idx.Num(); t += 3)
            {
                S.Tris.Add(Base + (int32)Idx[t]);
                S.Tris.Add(Base + (int32)Idx[t + 1]);
                S.Tris.Add(Base + (int32)Idx[t + 2]);
            }
        }
        return S;
    }

    // ── Fallback: esferas UV procedurales ──
    const int32 Rings   = FMath::Max(Segments / 2, 4); // polo a polo
    const int32 Sectors = FMath::Max(Segments, 6);     // alrededor
    for (const FVector4& E : LocalEyes)
    {
        const FVector Center(E.X, E.Y, E.Z);
        const float   R    = FMath::Max((float)E.W, 1.f);
        const int32   Base = S.Verts.Num();
        for (int32 r = 0; r <= Rings; ++r)
        {
            const float phi = PI * r / Rings;
            const float sp = FMath::Sin(phi), cp = FMath::Cos(phi);
            for (int32 c = 0; c <= Sectors; ++c)
            {
                const float th = 2.f * PI * c / Sectors;
                const FVector Nrm(sp * FMath::Cos(th), sp * FMath::Sin(th), cp);
                S.Verts.Add(Center + Nrm * R);
                S.Normals.Add(Nrm);
                S.UVs.Add(FVector2D((float)c / Sectors, (float)r / Rings));
                S.Colors.Add(FColor::White);
            }
        }
        const int32 Stride = Sectors + 1;
        for (int32 r = 0; r < Rings; ++r)
        for (int32 c = 0; c < Sectors; ++c)
        {
            const int32 i0 = Base + r * Stride + c;
            const int32 i1 = i0 + 1;
            const int32 i2 = i0 + Stride;
            const int32 i3 = i2 + 1;
            S.Tris.Add(i0); S.Tris.Add(i2); S.Tris.Add(i1);
            S.Tris.Add(i1); S.Tris.Add(i2); S.Tris.Add(i3);
        }
    }
    return S;
}

void APTLobbyCharacter::UpdateHeadCollision()
{
    USkeletalMeshComponent* M = GetMesh();
    if (!M || !HeadMesh || HeadPhysicsBone.IsNone()) return;

    // Radio de la cabeza esculpida (bounds del HeadMesh, ya con su escala en el socket).
    HeadMesh->UpdateBounds();
    const float R = HeadMesh->Bounds.SphereRadius;
    if (R <= KINDA_SMALL_NUMBER) return;

    const float Scale = FMath::Clamp(R / FMath::Max(HeadCollisionRefSize, 1.f), 0.1f, 8.f);
    if (FBodyInstance* BI = M->GetBodyInstance(HeadPhysicsBone))
        BI->UpdateBodyScale(FVector(Scale)); // escala la cápsula/esfera del hueso de la cabeza
}

void APTLobbyCharacter::ClearHeadMesh()
{
    if (HeadMesh) HeadMesh->ClearAllMeshSections();
}

void APTLobbyCharacter::ApplyDefaultSphereHead()
{
    if (!HeadMesh) return;
    HeadMesh->ClearAllMeshSections();
    // Una sola "esfera" (reusa el generador de ojos: esfera UV blanca) centrada en el socket.
    TArray<FVector4> One;
    One.Add(FVector4(0.f, 0.f, 0.f, FMath::Max(1.f, DefaultHeadRadius)));
    FPTHeadSection S = BuildEyesSection(One, nullptr, 50.f, 24);
    const TArray<FProcMeshTangent> NoTangents;
    HeadMesh->CreateMeshSection(0, S.Verts, S.Tris, S.Normals, S.UVs, S.Colors, NoTangents, /*collision=*/false);
    if (HeadMaterial) HeadMesh->SetMaterial(0, HeadMaterial); // material plano (vertex color blanco)
    UpdateHeadCollision();
}

void APTLobbyCharacter::SaveHeadBlob(const TArray<uint8>& Blob)
{
    UPTHeadSaveGame* Save = Cast<UPTHeadSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UPTHeadSaveGame::StaticClass()));
    if (!Save) return;
    Save->Blob = Blob;
    UGameplayStatics::SaveGameToSlot(Save, PTHeadSaveSlot, 0);
}

void APTLobbyCharacter::LoadHead()
{
    // Carga y REPLICA la combinación EQUIPADA del Locker (cabeza equipada + cuerpo equipado).
    UPTLockerSubsystem* L = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr;
    if (!L) return;

    const TArray<uint8>& Head = L->GetEquippedHeadBaked();
    const TArray<uint8>& Body = L->GetEquippedBodyPNG();
    if (Head.Num() == 0)
    {
        // Slot "Default" (o nada equipado): look base → cabeza ESFERA blanca por defecto + cuerpo equipado.
        ApplyDefaultSphereHead();
        if (Body.Num() > 0) ApplyBodyPaintFromPNG(Body); else ClearBodyPaint();
        bLocallyBakedHead = true; // no re-aplicar un blob replicado viejo encima del default
        return;
    }

    TArray<uint8> Blob;
    AssembleReplicatedBlob(Head, Body, Blob); // cabeza equipada + (si hay) cuerpo equipado
    if (Blob.Num() == 0) return;

    ApplyHeadBlobLocal(Blob);
    bLocallyBakedHead = true; // es MI look equipado; no pisarlo con el blob que vuelve replicado
    if (APTPlayerState* PS = GetPlayerState<APTPlayerState>()) PS->UploadHead(Blob);
}

void APTLobbyCharacter::ApplyLookPreview(int32 HeadIdx, int32 BodyIdx)
{
    UPTLockerSubsystem* L = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr;
    if (!L) return;

    static const TArray<uint8> Empty; // referencia estable para los casos "vacío/default"
    const TArray<uint8>& Head = (HeadIdx >= 0 && L->IsHeadSlotUsed(HeadIdx)) ? L->GetHeadBaked(HeadIdx) : Empty;
    const TArray<uint8>& Body = (BodyIdx >= 0 && L->IsBodySlotUsed(BodyIdx)) ? L->GetBodyPNG(BodyIdx)   : Empty;

    if (Head.Num() == 0)
    {
        // Sin cabeza en ese slot → esfera default + el cuerpo elegido (o limpio).
        ApplyDefaultSphereHead();
        if (Body.Num() > 0) ApplyBodyPaintFromPNG(Body); else ClearBodyPaint();
        return;
    }

    TArray<uint8> Blob;
    AssembleReplicatedBlob(Head, Body, Blob);
    if (Blob.Num() > 0) ApplyHeadBlobLocal(Blob); // SOLO local: no UploadHead → los demás no ven el preview
}

// ── PNG helpers (BGRA8 ↔ PNG) ─────────────────────────────────────────────────
static bool PT_EncodePNG_BGRA(const TArray<FColor>& Px, int32 N, TArray<uint8>& Out)
{
    if (Px.Num() != N * N || N <= 0) return false;
    IImageWrapperModule& Mod = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TSharedPtr<IImageWrapper> W = Mod.CreateImageWrapper(EImageFormat::PNG);
    if (!W.IsValid()) return false;
    if (!W->SetRaw(Px.GetData(), (int64)Px.Num() * sizeof(FColor), N, N, ERGBFormat::BGRA, 8)) return false;
    const TArray64<uint8>& Comp = W->GetCompressed(100);
    Out.SetNumUninitialized(Comp.Num());
    FMemory::Memcpy(Out.GetData(), Comp.GetData(), Comp.Num());
    return Out.Num() > 0;
}
static bool PT_DecodePNG_BGRA(const TArray<uint8>& In, TArray<FColor>& OutPx, int32& OutN)
{
    if (In.Num() == 0) return false;
    IImageWrapperModule& Mod = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
    TSharedPtr<IImageWrapper> W = Mod.CreateImageWrapper(EImageFormat::PNG);
    if (!W.IsValid() || !W->SetCompressed(In.GetData(), In.Num())) return false;
    TArray64<uint8> Raw;
    if (!W->GetRaw(ERGBFormat::BGRA, 8, Raw)) return false;
    OutN = W->GetWidth();
    OutPx.SetNumUninitialized(OutN * W->GetHeight());
    FMemory::Memcpy(OutPx.GetData(), Raw.GetData(), FMath::Min<int64>(Raw.Num(), (int64)OutPx.Num() * sizeof(FColor)));
    return true;
}

// ── Blob combinado: geometría + centro + PNG cabeza + PNG cuerpo ───────────────
static const uint32 PT_HEADBLOB_MAGIC = 0x50544832; // 'PTH2'

// Serializador de bajo nivel del blob combinado (reusado por BuildHeadBlob y AssembleReplicatedBlob).
static void PT_SerializeHeadBlob(const TArray<uint8>& Geo, const FVector& Center,
                                 const TArray<uint8>& HeadPNG, const TArray<uint8>& BodyPNG, TArray<uint8>& Out)
{
    Out.Reset();
    FMemoryWriter Ar(Out);
    uint32 Magic = PT_HEADBLOB_MAGIC; Ar << Magic;
    int32 GN = Geo.Num();     Ar << GN; if (GN) Ar.Serialize(const_cast<uint8*>(Geo.GetData()), GN);
    FVector C = Center;       Ar << C;
    int32 HN = HeadPNG.Num(); Ar << HN; if (HN) Ar.Serialize(const_cast<uint8*>(HeadPNG.GetData()), HN);
    int32 BN = BodyPNG.Num(); Ar << BN; if (BN) Ar.Serialize(const_cast<uint8*>(BodyPNG.GetData()), BN);
}

void APTLobbyCharacter::BuildHeadBlob(const TArray<FPTHeadSection>& Secs, TArray<uint8>& OutBlob) const
{
    TArray<uint8> Geo;  SectionsToBlob(Secs, Geo);                       // geometría (ya comprimida)
    TArray<uint8> HeadPNG; PT_EncodePNG_BGRA(HeadPaintPixels, HeadPaintN, HeadPNG);
    TArray<uint8> BodyPNG; PT_EncodePNG_BGRA(PaintPixels,     PaintTexN, BodyPNG);

    PT_SerializeHeadBlob(Geo, HeadPaintCenterLocal, HeadPNG, BodyPNG, OutBlob);

    const int32 GN = Geo.Num(), HN = HeadPNG.Num(), BN = BodyPNG.Num();

    // ── DEBUG de tamaño del blob (para elegir la mejor resolución de textura) ──
    // El blob viaja en chunks de 8 KB por RPC confiable; UE corta la conexión si se encolan más de
    // ~256 bunches confiables de una. Por eso logueamos el desglose y el conteo de chunks, y avisamos
    // cuando se acerca al límite peligroso.
    const int32 kChunkBytes = 8 * 1024;
    const int32 kSafeChunks = 180; // margen bajo el límite real (~256 bunches confiables por canal)
    const int32 Chunks = FMath::DivideAndRoundUp(OutBlob.Num(), kChunkBytes);
    auto KB = [](int32 B){ return B / 1024.0f; };
    UE_LOG(LogTemp, Warning,
        TEXT("[HeadBlob] geom=%.1f KB | cabeza PNG=%.1f KB (%dx%d) | cuerpo PNG=%.1f KB (%dx%d) | TOTAL=%.1f KB -> %d chunks (limite seguro ~%d)"),
        KB(GN), KB(HN), HeadPaintN, HeadPaintN, KB(BN), PaintTexN, PaintTexN, KB(OutBlob.Num()), Chunks, kSafeChunks);

    // ── Print en PANTALLA (para tunear la resolución en vivo) ──
    if (GEngine)
    {
        const FColor Safe   = FColor(120, 230, 120);
        const FColor Warn   = FColor(240, 210, 90);
        const FColor Danger = FColor(255, 90, 90);
        const float  Dur    = 12.f;

        GEngine->AddOnScreenDebugMessage(7001, Dur, FColor(200, 200, 255),
            FString::Printf(TEXT("CABEZA: %.0f KB  (%dx%d)"), KB(HN), HeadPaintN, HeadPaintN));
        GEngine->AddOnScreenDebugMessage(7002, Dur, FColor(200, 200, 255),
            FString::Printf(TEXT("CUERPO: %.0f KB  (%dx%d)"), KB(BN), PaintTexN, PaintTexN));

        const FColor TotalCol = (Chunks > kSafeChunks) ? Danger : (Chunks > kSafeChunks * 3 / 4 ? Warn : Safe);
        const TCHAR* Estado   = (Chunks > kSafeChunks) ? TEXT("!! SUPERA EL LIMITE !!")
                              : (Chunks > kSafeChunks * 3 / 4 ? TEXT("(cerca del limite)") : TEXT("(OK)"));
        GEngine->AddOnScreenDebugMessage(7003, Dur, TotalCol,
            FString::Printf(TEXT("BLOB TOTAL: %.0f KB  ->  %d / %d chunks  %s"),
                KB(OutBlob.Num()), Chunks, kSafeChunks, Estado));
    }
}

bool APTLobbyCharacter::ParseHeadBlob(const TArray<uint8>& Blob, TArray<FPTHeadSection>& OutSecs,
                                      FVector& OutCenter, TArray<uint8>& OutHeadPNG, TArray<uint8>& OutBodyPNG) const
{
    OutCenter = FVector::ZeroVector; OutHeadPNG.Reset(); OutBodyPNG.Reset();
    if (Blob.Num() < (int32)sizeof(uint32)) return false;

    uint32 Magic = 0; FMemory::Memcpy(&Magic, Blob.GetData(), sizeof(uint32));
    if (Magic != PT_HEADBLOB_MAGIC)
        return BlobToSections(Blob, OutSecs); // blob viejo = solo geometría

    FMemoryReader Ar(Blob);
    uint32 M = 0; Ar << M;
    int32 GN = 0; Ar << GN;
    TArray<uint8> Geo; Geo.SetNumUninitialized(GN); if (GN) Ar.Serialize(Geo.GetData(), GN);
    Ar << OutCenter;
    int32 HN = 0; Ar << HN; OutHeadPNG.SetNumUninitialized(HN); if (HN) Ar.Serialize(OutHeadPNG.GetData(), HN);
    int32 BN = 0; Ar << BN; OutBodyPNG.SetNumUninitialized(BN); if (BN) Ar.Serialize(OutBodyPNG.GetData(), BN);
    return BlobToSections(Geo, OutSecs);
}

void APTLobbyCharacter::ApplyHeadPaintFromPNG(const TArray<uint8>& PNG, const FVector& CenterLocal)
{
    HeadPaintCenterLocal = CenterLocal;
    TArray<FColor> Px; int32 N = 0;
    if (PNG.Num() == 0 || !PT_DecodePNG_BGRA(PNG, Px, N) || N <= 0) return;
    HeadPaintN = N;
    HeadPaintPixels = MoveTemp(Px);
    HeadPaintTex = UTexture2D::CreateTransient(N, N, PF_B8G8R8A8);
    if (!HeadPaintTex) return;
    HeadPaintTex->SRGB = true; HeadPaintTex->Filter = TF_Bilinear;
    HeadPaintTex->AddressX = TA_Wrap; HeadPaintTex->AddressY = TA_Clamp;
    HeadPaintTex->AddToRoot();
    FTexture2DMipMap& Mip = HeadPaintTex->GetPlatformData()->Mips[0];
    void* D = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(D, HeadPaintPixels.GetData(), HeadPaintPixels.Num() * sizeof(FColor));
    Mip.BulkData.Unlock();
    HeadPaintTex->UpdateResource();
}

void APTLobbyCharacter::ApplyBodyPaintFromPNG(const TArray<uint8>& PNG)
{
    InitCharacterPaint(); // asegura PaintTex + CharPaintMID
    TArray<FColor> Px; int32 N = 0;
    if (PNG.Num() == 0 || !PT_DecodePNG_BGRA(PNG, Px, N) || N <= 0 || !PaintTex) return;
    if (N != PaintTexN || Px.Num() != PaintPixels.Num()) return; // tamaños deben coincidir (1024)
    PaintPixels = MoveTemp(Px);
    FTexture2DMipMap& Mip = PaintTex->GetPlatformData()->Mips[0];
    void* D = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(D, PaintPixels.GetData(), PaintPixels.Num() * sizeof(FColor));
    Mip.BulkData.Unlock();
    PaintTex->UpdateResource();
}

// Aplica un blob combinado LOCALMENTE (geometría + texturas + materiales). Lo usan LoadHead y
// ApplyReplicatedHead (pawns remotos).
void APTLobbyCharacter::ApplyHeadBlobLocal(const TArray<uint8>& Blob)
{
    TArray<FPTHeadSection> Secs; FVector Center; TArray<uint8> HeadPNG, BodyPNG;
    if (!ParseHeadBlob(Blob, Secs, Center, HeadPNG, BodyPNG) || Secs.Num() == 0) return;

    // Texturas de pintura primero (para poder crear el material de la cabeza antes de aplicar la malla).
    if (HeadPNG.Num() > 0) ApplyHeadPaintFromPNG(HeadPNG, Center);
    if (BodyPNG.Num() > 0) ApplyBodyPaintFromPNG(BodyPNG);
    HeadColorMID = CreateHeadPaintMID(); // M_HeadPaint + textura + centro (idéntico al esculpido)

    ApplyHeadSections(Secs); // usa HeadColorMID en las secciones de arcilla
    UpdateHeadCollision();
}

void APTLobbyCharacter::TryApplyReplicatedHead()
{
    // Si el PlayerState ya trae una cabeza (otros jugadores, o al viajar a Lvl-01), aplicarla.
    if (const APTPlayerState* PS = GetPlayerState<APTPlayerState>())
        if (PS->HeadBlob.Num() > 0)
        {
            ApplyReplicatedHead();
            return;
        }

    // Sin cabeza replicada aún: si es el pawn local, cargar la guardada (disco) y replicarla.
    if (IsLocallyControlled())
        LoadHead();
}

void APTLobbyCharacter::BeginPlay()
{
    Super::BeginPlay();
    TryApplyReplicatedHead();
    InitCharacterPaint(); // deja el RT enganchado desde el arranque (evita ver el material gris)
}

void APTLobbyCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    TryApplyReplicatedHead(); // el PlayerState (y su cabeza) puede llegar después del BeginPlay
}

void APTLobbyCharacter::SetSculptPose(bool bEnable, UAnimationAsset* PoseAnim)
{
    USkeletalMeshComponent* M = GetMesh();
    if (!M) return;

    if (bEnable)
    {
        // Recto y quieto mientras esculpís: apagar la física (jiggle del physics asset) y
        // bypassear el AnimBP (que hace el baile y/o el AnimDynamics).
        M->SetSimulatePhysics(false);
        M->SetAllBodiesSimulatePhysics(false);
        M->PutAllRigidBodiesToSleep();
        // Cortar cualquier montage en curso (ej: el salto que seguía en loop).
        if (UAnimInstance* AI = M->GetAnimInstance())
            AI->StopAllMontages(0.f);

        UAnimationAsset* Anim = PoseAnim ? PoseAnim : SculptPoseAnim;
        M->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        if (Anim)
        {
            // Reproducir la pose recta en loop (una pose estática = queda quieta). NO pausar:
            // pausar congelaba en la última pose del baile en vez de tomar la pose recta.
            M->bPauseAnims = false;
            M->PlayAnimation(Anim, /*bLooping*/ true);
        }
        else
        {
            // Sin pose asignada: ref pose (frame 0) y congelar.
            M->SetPosition(0.f, false);
            M->bPauseAnims = true;
        }
        // Evaluar la pose YA, para que GetSocketLocation devuelva la posición recta (no la vieja).
        M->RefreshBoneTransforms();
    }
    else
    {
        // Restaurar la animación normal: volver al AnimBP y RE-INICIALIZARLO para que el grafo
        // (idle/caminar + el jiggle por AnimDynamics) vuelva a correr desde cero. Sin el InitAnim,
        // a veces quedaba congelado en la última pose del SingleNode (bug: "no vuelve la animación").
        //
        M->bPauseAnims = false;
        M->SetAnimationMode(EAnimationMode::AnimationBlueprint);
        M->InitAnim(true);

        // Restaurar el JIGGLE del physics asset EXACTAMENTE como estaba. Al entrar a esculpir se
        // apagó todo con SetAllBodiesSimulatePhysics(false) + PutAllRigidBodiesToSleep(), y los
        // huesos de jiggle son de tipo "Simulated" (simulan solos, no dependen del flag del
        // componente), así que ni SetSimulatePhysics(true) ni SetAllBodiesSimulatePhysics(true)
        // los devuelven bien (el segundo además ragdollea todo). La forma correcta es RECREAR el
        // estado físico desde el physics asset: cada cuerpo vuelve a su tipo original
        // (Kinematic sigue la animación, Simulated hace el jiggle) → queda igual que al inicio.
        M->RecreatePhysicsState();
    }
}

void APTLobbyCharacter::SetFacingArrowVisible(bool bVisible)
{
    if (FacingArrow) FacingArrow->SetHiddenInGame(!bVisible);
}

void APTLobbyCharacter::UpdateNameTag()
{
    if (!NameTag) return;

    // Modo captura dev: ocultar todos los nombres flotantes (PTHideNames).
    UPTGameInstance* CapGI = GetGameInstance<UPTGameInstance>();
    if (CapGI && CapGI->AreNamesHidden())
    {
        NameTag->SetVisibility(false);
        return;
    }

    // Globo de chat activo: mostrar el mensaje (a todos, incluso a uno mismo) sin pisarlo.
    if (GetWorld() && GetWorld()->GetTimeSeconds() < ChatBubbleUntil)
    {
        NameTag->SetVisibility(true);
        return;
    }

    // Tu PROPIO tag: se ve en el LOBBY y en el MENÚ principal, pero NO en gameplay (Lvl-01).
    // bForceFlying solo se activa en gameplay (ApplyGameplayMovementMode), así que sirve de "estoy jugando".
    if (IsLocallyControlled() && bForceFlying)
    {
        NameTag->SetVisibility(false);
        return;
    }
    NameTag->SetVisibility(true);

    if (UPTNameTagWidget* W = Cast<UPTNameTagWidget>(NameTag->GetUserWidgetObject()))
    {
        // DisplayName lo setea el GameMode y se replica; a veces (sobre todo tras el seamless travel)
        // puede llegar vacío. Caer al nombre nativo del PlayerState (el "?Name=" de Steam).
        FString N;
        bool bHost = false;
        if (const APTPlayerState* PS = GetPlayerState<APTPlayerState>())
        {
            N = PS->GetDisplayNameSafe();
            if (N.IsEmpty()) N = PS->GetPlayerName();
            bHost = PS->bIsHost;
        }
        // Fallback para el menú principal (sin sesión / sin DisplayName): usar el nick local de Steam.
        if (N.IsEmpty() && IsLocallyControlled())
            if (UMultiplayerSessionsSubsystem* S = GetGameInstance()
                    ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
                N = S->GetLocalPlayerDisplayName();
        // Modo captura: reemplazar el nick real por "Player N" (local, no se replica).
        if (CapGI && CapGI->IsCaptureMode())
            if (const APTPlayerState* PS = GetPlayerState<APTPlayerState>())
            {
                const FString CapName = CapGI->GetCaptureName(PS);
                if (!CapName.IsEmpty()) N = CapName;
            }
        W->SetPlayerName(N);
        W->SetHost(bHost); // corona del host, igual que en la lista de jugadores
    }
}

void APTLobbyCharacter::Multicast_ShowChatBubble_Implementation(const FString& Text, bool bGuess)
{
    if (NameTag)
    {
        ChatBubbleUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + ChatBubbleDuration : 0.f;
        NameTag->SetVisibility(true);
        if (UPTNameTagWidget* W = Cast<UPTNameTagWidget>(NameTag->GetUserWidgetObject()))
        {
            // El globo de acierto se traduce ACÁ, en cada cliente: el servidor manda Text vacío
            // para que cada uno lo lea en su idioma (y para no filtrar nunca la palabra).
            if (bGuess) W->ShowGuessMessage(PTText::GetStr(TEXT("BUBBLE_GUESSED_IT"))); // verde
            else        W->ShowMessage(Text);
        }
    }

    // Confetti al adivinar, desde la posición del jugador.
    if (bGuess && ConfettiFX)
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), ConfettiFX, GetActorLocation());
}

void APTLobbyCharacter::Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller) return;

    const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
    const FVector Forward = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
    const FVector Right   = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);

    AddMovementInput(Forward, Axis.Y);
    AddMovementInput(Right,   Axis.X);
}

void APTLobbyCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(-Axis.Y); // no invertido (default, igual que Lvl-01)
}

void APTLobbyCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction)
            EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APTLobbyCharacter::Move);
        if (LookAction)
            EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &APTLobbyCharacter::Look);
        if (JumpAction)
        {
            EIC->BindAction(JumpAction, ETriggerEvent::Started,   this, &APTLobbyCharacter::OnJumpPressed);
            EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &APTLobbyCharacter::OnJumpReleased);
        }
    }

    // Descenso en vuelo: Ctrl izquierdo (tecla legacy, no interfiere con Enhanced Input).
    PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Pressed,  this, &APTLobbyCharacter::OnDescendPressed);
    PlayerInputComponent->BindKey(EKeys::LeftControl, IE_Released, this, &APTLobbyCharacter::OnDescendReleased);
}

// ── Vuelo (modo creativo Minecraft) ─────────────────────────────────────────

void APTLobbyCharacter::OnJumpPressed()
{
    // Ya NO hay doble-toque para alternar vuelo: el modo lo decide el nivel. En Lvl-01 siempre se
    // vuela (bForceFlying, lo pone el gameplay) y en el lobby/menú siempre se camina.
    if (bFlying) bAscend = true;
    else
    {
        Jump();
        if (JumpMontage) Server_PlayJump(); // reproducir la anim de salto en todos
    }
}

void APTLobbyCharacter::Server_PlayJump_Implementation()
{
    Multicast_PlayJump();
}

void APTLobbyCharacter::Multicast_PlayJump_Implementation()
{
    if (JumpMontage) PlayAnimMontage(JumpMontage);
}

void APTLobbyCharacter::OnJumpReleased()
{
    bAscend = false;
    if (!bFlying) StopJumping();
}

void APTLobbyCharacter::ToggleFly()
{
    // En el gameplay (Lvl-01) el vuelo es obligatorio: caminar se sentía con lag y el nivel es un
    // vacío sin piso. El doble-espacio no debe poder apagarlo.
    if (bForceFlying) return;
    SetFlyingMode(!bFlying);
}

void APTLobbyCharacter::ApplyGameplayMovementMode()
{
    bForceFlying = true;
    if (UCharacterMovementComponent* M = GetCharacterMovement())
        M->bOrientRotationToMovement = false; // en el gameplay no orientamos al movimiento...
    // ...sino que el personaje mira hacia donde apunta la cámara (controller yaw). Así los demás
    // ven a dónde estás mirando en el Lvl-01. La rotación del actor se replica a los otros clientes.
    // (En el lobby queda al revés: el constructor deja bUseControllerRotationYaw=false y
    //  bOrientRotationToMovement=true, para que el personaje mire hacia donde camina.)
    bUseControllerRotationYaw = true;
    SetFlyingMode(true);
}

void APTLobbyCharacter::SetFlyingMode(bool bEnable)
{
    if (bForceFlying) bEnable = true; // no se puede salir del vuelo donde es obligatorio
    bFlying = bEnable;
    UCharacterMovementComponent* M = GetCharacterMovement();
    if (bFlying)
    {
        M->MaxFlySpeed     = FlySpeed;
        M->MaxAcceleration = FlyAcceleration; // rampa progresiva de 0 a máxima
        M->SetMovementMode(MOVE_Flying);
    }
    else
    {
        M->MaxAcceleration = DefaultMaxAccel; // restaurar para caminar
        M->SetMovementMode(MOVE_Walking);
        bAscend = bDescend = false;
    }
}

void APTLobbyCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    // Actualizar el cartel del nombre cada ~0.5s (el DisplayName se replica, puede tardar).
    NameTagAccum += DeltaSeconds;
    if (NameTagAccum >= 0.5f)
    {
        NameTagAccum = 0.f;
        UpdateNameTag();

        // Espectador dev: ocultar por completo el personaje (en TODAS las máquinas). El flag se
        // replica en el PlayerState; polleamos acá para cubrir cualquier orden de llegada.
        if (const APTPlayerState* SpecPS = GetPlayerState<APTPlayerState>())
        {
            const bool bSpec = SpecPS->bIsDevSpectator;
            if (bSpec != bSpectatorHiddenApplied)
            {
                bSpectatorHiddenApplied = bSpec;
                SetActorHiddenInGame(bSpec);
                SetActorEnableCollision(!bSpec);
            }
        }

        // Cabeza custom: aplicarla si el PlayerState tiene una versión distinta a la puesta.
        // Cubre CUALQUIER orden de llegada (blob antes que el pawn, pawn antes que el blob,
        // re-edición, seamless travel) — los OnRep solos se perdían carreras y no reintentaban.
        if (const APTPlayerState* PS = GetPlayerState<APTPlayerState>())
        {
            // Se compara contra LocalHeadVersion (lo que este cliente REALMENTE reensambló), no
            // contra HeadVersion (que replica antes que los datos): si no, se aplicaría una cabeza
            // a medio bajar y no se volvería a intentar.
            const int32 Have = FMath::Max(PS->LocalHeadVersion, HasAuthority() ? PS->HeadVersion : 0);
            if (Have != AppliedHeadVersion && PS->HeadBlob.Num() > 0)
            {
                AppliedHeadVersion = Have;
                ApplyReplicatedHead();
            }
        }
    }

    // Vuelo obligatorio (Lvl-01): si algo dejó el movimiento en caminar, volver a vuelo.
    if (bForceFlying && GetCharacterMovement()
        && GetCharacterMovement()->MovementMode != MOVE_Flying)
    {
        SetFlyingMode(true);
    }

    if (!bFlying) return;
    if (bAscend)  AddMovementInput(FVector::UpVector,  1.f);
    if (bDescend) AddMovementInput(FVector::UpVector, -1.f);
}

// ── SPIKE: pintado de la piel del personaje por Render Target (UV) ─────────────

void APTLobbyCharacter::InitCharacterPaint()
{
    if (PaintTex || !GetMesh()) return; // ya inicializado / sin mesh

    PaintTexN = FMath::Max(64, PaintRTSize);

    // Textura CPU-backed (BGRA) que el material muestrea por UV. Empieza transparente (alpha 0 = sin
    // pintar). Se pinta en el buffer CPU y se sube por regiones con UpdateTextureRegions.
    PaintTex = UTexture2D::CreateTransient(PaintTexN, PaintTexN, PF_B8G8R8A8);
    if (!PaintTex) return;
    PaintTex->SRGB = true;                       // guardamos bytes sRGB → el material los decodifica bien
    PaintTex->CompressionSettings = TC_VectorDisplacementmap; // sin compresión (nítido)
    PaintTex->Filter = TF_Bilinear;
    PaintTex->AddToRoot();

    PaintPixels.Init(FColor(0, 0, 0, 0), PaintTexN * PaintTexN);

    // Subir el estado inicial (transparente) al mip 0.
    FTexture2DMipMap& Mip = PaintTex->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Data, PaintPixels.GetData(), PaintPixels.Num() * sizeof(FColor));
    Mip.BulkData.Unlock();
    PaintTex->UpdateResource();

    // Material dinámico del personaje con la textura enganchada en "PaintTex".
    if (UMaterialInterface* Base = GetMesh()->GetMaterial(0))
    {
        CharPaintMID = GetMesh()->CreateDynamicMaterialInstance(0, Base);
        if (CharPaintMID)
        {
            CharPaintMID->SetTextureParameterValue(PaintTexParam, PaintTex);
            // Color base del cuerpo (donde no hay pintura) = blanco por defecto. El VectorParameter
            // real del material se llama "Color" ("BaseColor" es el PIN de salida, no un parámetro,
            // por eso antes el default salía NEGRO). Seteamos ambos por robustez: el que no exista es
            // un no-op inofensivo.
            CharPaintMID->SetVectorParameterValue(TEXT("Color"), BodyBaseColor);
            CharPaintMID->SetVectorParameterValue(BodyBaseColorParam, BodyBaseColor);
        }
    }
}

const FSkeletalMeshLODRenderData* APTLobbyCharacter::EnsureSkinnedCache()
{
    USkeletalMeshComponent* SK = GetMesh();
    if (!SK) return nullptr;
    USkeletalMesh* SkelAsset = SK->GetSkeletalMeshAsset();
    if (!SkelAsset) return nullptr;
    FSkeletalMeshRenderData* RD = SkelAsset->GetResourceForRendering();
    if (!RD || !RD->LODRenderData.IsValidIndex(0)) return nullptr;
    FSkeletalMeshLODRenderData& LOD = RD->LODRenderData[0];

    // Reusar el cache si ya se computó en este mismo frame (una estampa de trazo puede llamar muchas
    // veces por frame; recomputar el skinning cada vez sería carísimo).
    const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
    if (CachedPosTime == Now && CachedWorldPos.Num() > 0) return &LOD;

    FSkinWeightVertexBuffer* SkinBuf = LOD.GetSkinWeightVertexBuffer();
    if (!SkinBuf) return nullptr;
    TArray<FMatrix44f> RefToLocals;
    SK->GetCurrentRefToLocalMatrices(RefToLocals, 0);
    TArray<FVector3f> Pos;
    USkinnedMeshComponent::ComputeSkinnedPositions(SK, Pos, RefToLocals, LOD, *SkinBuf);
    if (Pos.Num() == 0) return nullptr;

    const FTransform& X = SK->GetComponentTransform();
    CachedWorldPos.SetNum(Pos.Num());
    for (int32 i = 0; i < Pos.Num(); ++i) CachedWorldPos[i] = X.TransformPosition(FVector(Pos[i]));
    CachedPosTime = Now;
    return &LOD;
}

bool APTLobbyCharacter::RaycastSkinnedMeshUV(const FVector& Origin, const FVector& Dir,
                                             FVector2D& OutUV, FVector& OutPoint, FVector& OutNormal) const
{
    const FSkeletalMeshLODRenderData* LODp = const_cast<APTLobbyCharacter*>(this)->EnsureSkinnedCache();
    if (!LODp) return false;
    const FSkeletalMeshLODRenderData& LOD = *LODp;
    const FRawStaticIndexBuffer16or32Interface* Idx = LOD.MultiSizeIndexContainer.GetIndexBuffer();
    if (!Idx) return false;

    const FVector Start = Origin;
    const FVector End   = Origin + Dir * 100000.f;

    // Interseca el rayo contra CADA triángulo y se queda con el más cercano al origen.
    float   BestDistSq = TNumericLimits<float>::Max();
    int32   BestBase   = INDEX_NONE;
    FVector BestPoint  = FVector::ZeroVector;
    FVector BestNormal = FVector::UpVector;
    const int32 NumIdx = Idx->Num();
    for (int32 i = 0; i + 2 < NumIdx; i += 3)
    {
        const uint32 A = Idx->Get(i), B = Idx->Get(i + 1), C = Idx->Get(i + 2);
        if (!CachedWorldPos.IsValidIndex(A) || !CachedWorldPos.IsValidIndex(B) || !CachedWorldPos.IsValidIndex(C)) continue;
        const FVector PA = CachedWorldPos[A], PB = CachedWorldPos[B], PC = CachedWorldPos[C];

        FVector IP, HitN;
        if (FMath::SegmentTriangleIntersection(Start, End, PA, PB, PC, IP, HitN))
        {
            const float D = (IP - Start).SizeSquared();
            if (D < BestDistSq) { BestDistSq = D; BestBase = i; BestPoint = IP; BestNormal = HitN; }
        }
    }
    if (BestBase == INDEX_NONE) return false;
    OutPoint  = BestPoint;
    OutNormal = BestNormal.GetSafeNormal();

    const uint32 A = Idx->Get(BestBase), B = Idx->Get(BestBase + 1), C = Idx->Get(BestBase + 2);
    const FVector Bary = FMath::ComputeBaryCentric2D(BestPoint, CachedWorldPos[A], CachedWorldPos[B], CachedWorldPos[C]);
    const FVector2D UVA(LOD.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(A, 0));
    const FVector2D UVB(LOD.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(B, 0));
    const FVector2D UVC(LOD.StaticVertexBuffers.StaticMeshVertexBuffer.GetVertexUV(C, 0));
    OutUV = Bary.X * UVA + Bary.Y * UVB + Bary.Z * UVC;
    return true;
}

void APTLobbyCharacter::MarkDirty(int32 X, int32 Y)
{
    if (DirtyMaxX < DirtyMinX) { DirtyMinX = DirtyMaxX = X; DirtyMinY = DirtyMaxY = Y; return; }
    DirtyMinX = FMath::Min(DirtyMinX, X); DirtyMaxX = FMath::Max(DirtyMaxX, X);
    DirtyMinY = FMath::Min(DirtyMinY, Y); DirtyMaxY = FMath::Max(DirtyMaxY, Y);
}

void APTLobbyCharacter::PaintBodyWorldSphere(const FVector& P, float R, FLinearColor Color)
{
    InitCharacterPaint();
    const FSkeletalMeshLODRenderData* LODp = EnsureSkinnedCache();
    if (!LODp || !PaintTex || PaintPixels.Num() == 0) return;
    const FSkeletalMeshLODRenderData& LOD = *LODp;
    const FRawStaticIndexBuffer16or32Interface* Idx = LOD.MultiSizeIndexContainer.GetIndexBuffer();
    if (!Idx) return;

    const int32  N   = PaintTexN;
    const FColor Src = Color.ToFColor(true); // bytes sRGB (la textura es SRGB=true)
    const float  R2  = R * R;
    const auto&  UVBuf = LOD.StaticVertexBuffers.StaticMeshVertexBuffer;
    const int32  NumIdx = Idx->Num();

    for (int32 i = 0; i + 2 < NumIdx; i += 3)
    {
        const uint32 A = Idx->Get(i), B = Idx->Get(i + 1), C = Idx->Get(i + 2);
        if (!CachedWorldPos.IsValidIndex(A) || !CachedWorldPos.IsValidIndex(B) || !CachedWorldPos.IsValidIndex(C)) continue;
        const FVector WA = CachedWorldPos[A], WB = CachedWorldPos[B], WC = CachedWorldPos[C];

        // Descartar triángulos lejos de la esfera del pincel (barato).
        FBox Tb(ForceInit); Tb += WA; Tb += WB; Tb += WC;
        if (Tb.ComputeSquaredDistanceToPoint(P) > R2) continue;

        // UV → coordenadas de téxel.
        FVector2D TA = FVector2D(UVBuf.GetVertexUV(A, 0)) * N;
        FVector2D TB = FVector2D(UVBuf.GetVertexUV(B, 0)) * N;
        FVector2D TC = FVector2D(UVBuf.GetVertexUV(C, 0)) * N;

        // RASTERIZACIÓN CONSERVADORA (anti-costura): expandir el triángulo hacia afuera unos téxeles.
        // Así se pintan los téxeles del "gutter" justo afuera del borde de la isla UV, que es de donde el
        // filtrado saca el color base y muestra la costura. Es por-triángulo y el pintado real se filtra
        // más abajo por la distancia 3D al pincel → NO se filtra color a otras islas lejanas del atlas
        // (nada de espejado). Expandimos desde el centroide del triángulo.
        {
            const float SeamPad = 3.0f; // téxeles de sobre-pintado en el borde de cada isla
            const FVector2D Ctr = (TA + TB + TC) / 3.0;
            auto Grow = [&](const FVector2D& V) -> FVector2D
            {
                const FVector2D d = V - Ctr; const double L = d.Size();
                return (L > 1e-4) ? V + (d / L) * SeamPad : V;
            };
            TA = Grow(TA); TB = Grow(TB); TC = Grow(TC);
        }

        // Denominador baricéntrico 2D (área*2); si es ~0, el triángulo es degenerado en el UV.
        const double Den = (double)(TB.Y - TC.Y) * (TA.X - TC.X) + (double)(TC.X - TB.X) * (TA.Y - TC.Y);
        if (FMath::Abs(Den) < 1e-8) continue;
        const double InvDen = 1.0 / Den;

        // Recorrer la caja de téxeles del triángulo (con 1 téxel de margen para el borde suave).
        int32 MinX = FMath::FloorToInt(FMath::Min3(TA.X, TB.X, TC.X)) - 1;
        int32 MaxX = FMath::CeilToInt (FMath::Max3(TA.X, TB.X, TC.X)) + 1;
        int32 MinY = FMath::FloorToInt(FMath::Min3(TA.Y, TB.Y, TC.Y)) - 1;
        int32 MaxY = FMath::CeilToInt (FMath::Max3(TA.Y, TB.Y, TC.Y)) + 1;
        MinX = FMath::Clamp(MinX, 0, N - 1); MaxX = FMath::Clamp(MaxX, 0, N - 1);
        MinY = FMath::Clamp(MinY, 0, N - 1); MaxY = FMath::Clamp(MaxY, 0, N - 1);

        for (int32 py = MinY; py <= MaxY; ++py)
        for (int32 px = MinX; px <= MaxX; ++px)
        {
            const double fx = px + 0.5, fy = py + 0.5;
            // Baricéntricas del centro del téxel respecto del triángulo en el UV.
            const double L1 = ((double)(TB.Y - TC.Y) * (fx - TC.X) + (double)(TC.X - TB.X) * (fy - TC.Y)) * InvDen;
            const double L2 = ((double)(TC.Y - TA.Y) * (fx - TC.X) + (double)(TA.X - TC.X) * (fy - TC.Y)) * InvDen;
            const double L3 = 1.0 - L1 - L2;
            // Pequeña tolerancia = derrama ~1 téxel fuera de la isla (tapa la costura bajo el filtrado).
            const double Tol = 0.02;
            if (L1 < -Tol || L2 < -Tol || L3 < -Tol) continue;

            // Reconstruir la posición 3D de este téxel y medir distancia a la esfera del pincel.
            const FVector WP = L1 * WA + L2 * WB + L3 * WC;
            const float   D2 = (float)(WP - P).SizeSquared();
            if (D2 > R2) continue;

            const float t = FMath::Sqrt(D2) / R;             // 0 centro → 1 borde
            const float a = 1.f - FMath::SmoothStep(0.7f, 1.f, t); // cobertura con borde suave
            if (a <= 0.f) continue;

            FColor& Dst = PaintPixels[py * N + px];
            const float inv = 1.f - a;
            Dst.R = (uint8)FMath::Clamp(FMath::RoundToInt(Src.R * a + Dst.R * inv), 0, 255);
            Dst.G = (uint8)FMath::Clamp(FMath::RoundToInt(Src.G * a + Dst.G * inv), 0, 255);
            Dst.B = (uint8)FMath::Clamp(FMath::RoundToInt(Src.B * a + Dst.B * inv), 0, 255);
            Dst.A = (uint8)FMath::Clamp(FMath::RoundToInt(255  * a + Dst.A * inv), 0, 255);
            MarkDirty(px, py);
        }
    }
}

void APTLobbyCharacter::FlushBodyPaint()
{
    if (!PaintTex || DirtyMaxX < DirtyMinX || DirtyMaxY < DirtyMinY) return;

    const int32 N  = PaintTexN;
    const int32 x0 = DirtyMinX, y0 = DirtyMinY;
    const int32 w  = DirtyMaxX - DirtyMinX + 1;
    const int32 h  = DirtyMaxY - DirtyMinY + 1;

    // Copiar el rectángulo sucio a un buffer propio (UpdateTextureRegions lo consume en el render thread).
    uint8* Buf = (uint8*)FMemory::Malloc(w * h * 4);
    for (int32 yy = 0; yy < h; ++yy)
        FMemory::Memcpy(Buf + yy * w * 4, &PaintPixels[(y0 + yy) * N + x0], w * 4);

    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(x0, y0, 0, 0, w, h);
    PaintTex->UpdateTextureRegions(0, 1, Region, (uint32)(w * 4), 4, Buf,
        [](uint8* Src, const FUpdateTextureRegion2D* Reg) { FMemory::Free(Src); delete Reg; });

    // Reset del rectángulo sucio.
    DirtyMinX = DirtyMinY = 0; DirtyMaxX = DirtyMaxY = -1;
}

void APTLobbyCharacter::PaintCharacterAtCursor(FLinearColor Color, float BrushPixels)
{
    APlayerController* PC = Cast<APlayerController>(GetController());
    if (!PC) PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
    if (!PC) return;

    float MX = 0.f, MY = 0.f;
    if (!PC->GetMousePosition(MX, MY)) return;
    FVector Origin, Dir;
    if (!PC->DeprojectScreenPositionToWorld(MX, MY, Origin, Dir)) return;

    FVector2D UV; FVector Pt, N;
    if (RaycastSkinnedMeshUV(Origin, Dir, UV, Pt, N))
    {
        PaintBodyWorldSphere(Pt, FMath::Max(1.f, BrushPixels), Color);
        FlushBodyPaint();
    }
}

// ─── Pintado 2D de la CABEZA (proyección esférica desde centro fijo; igual al cuerpo, sin atlas) ───

// UV esférica de un punto local respecto del centro fijo. u wrapea atrás; v de polo a polo.
static FVector2D PT_HeadSphUV(const FVector& LocalPos, const FVector& Center)
{
    FVector d = LocalPos - Center;
    if (!d.Normalize()) return FVector2D(0.5, 0.5);
    const float u = (float)(FMath::Atan2(d.Y, d.X) / (2.0 * PI)) + 0.5f;      // [0,1)
    const float v = (float)(FMath::Acos(FMath::Clamp(d.Z, -1.f, 1.f)) / PI);  // [0,1]
    return FVector2D(u, v);
}

void APTLobbyCharacter::InitHeadPaint(const FVector& CenterLocal)
{
    HeadPaintN = FMath::Clamp(HeadPaintTexSize, 256, 4096);
    HeadPaintCenterLocal = CenterLocal;
    HeadPaintPixels.Init(FColor(0, 0, 0, 0), HeadPaintN * HeadPaintN); // transparente = sin pintar
    HDirtyMinX = HDirtyMinY = 0; HDirtyMaxX = HDirtyMaxY = -1;

    HeadPaintTex = UTexture2D::CreateTransient(HeadPaintN, HeadPaintN, PF_B8G8R8A8);
    if (!HeadPaintTex) return;
    HeadPaintTex->SRGB = true;
    HeadPaintTex->Filter = TF_Bilinear;
    HeadPaintTex->AddressX = TA_Wrap;   // la costura esférica (u=0/1) queda continua
    HeadPaintTex->AddressY = TA_Clamp;
    HeadPaintTex->AddToRoot();
    FTexture2DMipMap& Mip = HeadPaintTex->GetPlatformData()->Mips[0];
    void* Data = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(Data, HeadPaintPixels.GetData(), HeadPaintPixels.Num() * sizeof(FColor));
    Mip.BulkData.Unlock();
    HeadPaintTex->UpdateResource();
}

UMaterialInstanceDynamic* APTLobbyCharacter::CreateHeadPaintMID()
{
    if (!HeadPaintMaterial || !HeadPaintTex) return nullptr;
    UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(HeadPaintMaterial, this);
    if (!MID) return nullptr;
    MID->SetTextureParameterValue(TEXT("PaintTex"), HeadPaintTex);
    MID->SetVectorParameterValue(TEXT("Center"), HeadPaintCenterLocal);
    return MID;
}

void APTLobbyCharacter::PaintHeadWorldSphere(UProceduralMeshComponent* ClayMesh, const FVector& P, float R, FLinearColor Color, bool bErase)
{
    if (!ClayMesh || !HeadPaintTex || HeadPaintPixels.Num() == 0) return;

    const int32  N   = HeadPaintN;
    const FColor Src = Color.ToFColor(true);
    const float  R2  = R * R;
    const FTransform Xf = ClayMesh->GetComponentTransform();

    auto MarkH = [this, N](int32 X, int32 Y)
    {
        X = ((X % N) + N) % N; // wrap en X (costura)
        if (Y < 0 || Y >= N) return;
        if (HDirtyMaxX < HDirtyMinX) { HDirtyMinX = HDirtyMaxX = X; HDirtyMinY = HDirtyMaxY = Y; return; }
        HDirtyMinX = FMath::Min(HDirtyMinX, X); HDirtyMaxX = FMath::Max(HDirtyMaxX, X);
        HDirtyMinY = FMath::Min(HDirtyMinY, Y); HDirtyMaxY = FMath::Max(HDirtyMaxY, Y);
    };

    for (int32 s = 0; s < ClayMesh->GetNumSections(); ++s)
    {
        const FProcMeshSection* Sec = ClayMesh->GetProcMeshSection(s);
        if (!Sec || Sec->ProcVertexBuffer.Num() == 0) continue;
        const TArray<FProcMeshVertex>& VB = Sec->ProcVertexBuffer;
        const TArray<uint32>& IB = Sec->ProcIndexBuffer;

        for (int32 i = 0; i + 2 < IB.Num(); i += 3)
        {
            const uint32 A = IB[i], B = IB[i + 1], C = IB[i + 2];
            if (!VB.IsValidIndex(A) || !VB.IsValidIndex(B) || !VB.IsValidIndex(C)) continue;
            const FVector LA = VB[A].Position, LB = VB[B].Position, LC = VB[C].Position;
            const FVector WA = Xf.TransformPosition(LA), WB = Xf.TransformPosition(LB), WC = Xf.TransformPosition(LC);

            FBox Tb(ForceInit); Tb += WA; Tb += WB; Tb += WC;
            if (Tb.ComputeSquaredDistanceToPoint(P) > R2) continue;

            // UV esférica por vértice.
            FVector2D UA = PT_HeadSphUV(LA, HeadPaintCenterLocal);
            FVector2D UB = PT_HeadSphUV(LB, HeadPaintCenterLocal);
            FVector2D UC = PT_HeadSphUV(LC, HeadPaintCenterLocal);
            // Costura del mapa esférico: solo "desdoblar" cuando el triángulo CRUZA de verdad la costura
            // u=0/1 (tiene vértices cerca de 0 Y cerca de 1). El chequeo viejo (span>0.5) también se
            // disparaba en triángulos grandes/oblicuos de formas complejas que NO cruzan la costura,
            // desdoblándolos mal → el trazo saltaba a otro lado / salía sucio. Estricto = menos falsos.
            const float uMin = FMath::Min3(UA.X, UB.X, UC.X), uMax = FMath::Max3(UA.X, UB.X, UC.X);
            const bool bCrossesSeam = (uMin < 0.25f) && (uMax > 0.75f);
            if (bCrossesSeam)
            {
                if (UA.X < 0.5f) UA.X += 1.f;
                if (UB.X < 0.5f) UB.X += 1.f;
                if (UC.X < 0.5f) UC.X += 1.f;
            }
            const FVector2D TA = UA * N, TB = UB * N, TC = UC * N;

            const double Den = (double)(TB.Y - TC.Y) * (TA.X - TC.X) + (double)(TC.X - TB.X) * (TA.Y - TC.Y);
            if (FMath::Abs(Den) < 1e-8) continue;
            const double InvDen = 1.0 / Den;

            int32 MinX = FMath::FloorToInt(FMath::Min3(TA.X, TB.X, TC.X)) - 1;
            int32 MaxX = FMath::CeilToInt (FMath::Max3(TA.X, TB.X, TC.X)) + 1;
            int32 MinY = FMath::FloorToInt(FMath::Min3(TA.Y, TB.Y, TC.Y)) - 1;
            int32 MaxY = FMath::CeilToInt (FMath::Max3(TA.Y, TB.Y, TC.Y)) + 1;
            MinY = FMath::Clamp(MinY, 0, N - 1); MaxY = FMath::Clamp(MaxY, 0, N - 1);

            for (int32 py = MinY; py <= MaxY; ++py)
            for (int32 pxu = MinX; pxu <= MaxX; ++pxu) // pxu puede exceder N (costura) → se wrapea
            {
                const double fx = pxu + 0.5, fy = py + 0.5;
                const double L1 = ((double)(TB.Y - TC.Y) * (fx - TC.X) + (double)(TC.X - TB.X) * (fy - TC.Y)) * InvDen;
                const double L2 = ((double)(TC.Y - TA.Y) * (fx - TC.X) + (double)(TA.X - TC.X) * (fy - TC.Y)) * InvDen;
                const double L3 = 1.0 - L1 - L2;
                const double Tol = 0.02;
                if (L1 < -Tol || L2 < -Tol || L3 < -Tol) continue;

                const FVector WP = L1 * WA + L2 * WB + L3 * WC;
                const float   D2 = (float)(WP - P).SizeSquared();
                if (D2 > R2) continue;

                const int32 px = ((pxu % N) + N) % N;
                FColor& Dst = HeadPaintPixels[py * N + px];
                if (bErase)
                {
                    // Borrar TODO el footprint del pincel (sin el degradé del borde): si solo borrara donde
                    // el falloff es >0, quedaría un anillo de rastros de la pintura vieja en el borde.
                    if (Dst.A != 0) { Dst = FColor(0, 0, 0, 0); MarkH(px, py); }
                    continue;
                }

                const float t = FMath::Sqrt(D2) / R;
                const float a = 1.f - FMath::SmoothStep(0.7f, 1.f, t);
                if (a <= 0.f) continue;
                const float inv = 1.f - a;
                Dst.R = (uint8)FMath::Clamp(FMath::RoundToInt(Src.R * a + Dst.R * inv), 0, 255);
                Dst.G = (uint8)FMath::Clamp(FMath::RoundToInt(Src.G * a + Dst.G * inv), 0, 255);
                Dst.B = (uint8)FMath::Clamp(FMath::RoundToInt(Src.B * a + Dst.B * inv), 0, 255);
                Dst.A = (uint8)FMath::Clamp(FMath::RoundToInt(255  * a + Dst.A * inv), 0, 255);
                MarkH(px, py);
            }
        }
    }
}

void APTLobbyCharacter::ClearHeadPaintCone(UProceduralMeshComponent* RefMesh, const FVector& P, float R)
{
    if (!RefMesh || !HeadPaintTex || HeadPaintPixels.Num() == 0) return;
    const int32 N = HeadPaintN;
    const FTransform Xf = RefMesh->GetComponentTransform();
    const FVector LocalP = Xf.InverseTransformPosition(P);
    FVector dir = LocalP - HeadPaintCenterLocal;
    const float dist = dir.Size();
    if (dist < 1e-3f) return;
    dir /= dist;

    // R (mundo) → local → medio ángulo del cono que subtiende la esfera del pincel desde el centro.
    const float MeshScale = Xf.GetScale3D().GetAbsMax();
    const float Rlocal    = (MeshScale > 1e-4f) ? R / MeshScale : R;
    const float Ang       = FMath::Clamp(FMath::Atan2(Rlocal, dist), 0.f, (float)PI);
    const float CosAng    = FMath::Cos(Ang);

    const FVector2D UV0 = PT_HeadSphUV(LocalP, HeadPaintCenterLocal);
    const int32 cx   = FMath::RoundToInt(UV0.X * N);
    const int32 cy   = FMath::RoundToInt(UV0.Y * N);
    const int32 vRad = FMath::CeilToInt((Ang / PI) * N) + 2;
    const int32 MinY = FMath::Clamp(cy - vRad, 0, N - 1);
    const int32 MaxY = FMath::Clamp(cy + vRad, 0, N - 1);

    for (int32 py = MinY; py <= MaxY; ++py)
    {
        // Ancho en u para ESTA fila: el cono se ensancha en azimut cerca de los polos (sin(v)→0).
        const float vv   = (py + 0.5f) / N;
        const float sinv = FMath::Sin(vv * PI);
        float uHalf = (sinv > 1e-3f) ? (Ang / (2.f * PI)) / sinv : 1.f;
        uHalf = FMath::Min(uHalf, 0.5f); // media vuelta como máximo
        const int32 uRad = FMath::CeilToInt(uHalf * N) + 2;

        for (int32 pxu = cx - uRad; pxu <= cx + uRad; ++pxu)
        {
            const int32 px = ((pxu % N) + N) % N; // wrap azimutal (costura u=0/1)
            const float u  = (px + 0.5f) / N;
            const float az = (u - 0.5f) * 2.f * PI;
            const float po = vv * PI;
            const float sp = FMath::Sin(po);
            const FVector TexDir(sp * FMath::Cos(az), sp * FMath::Sin(az), FMath::Cos(po));
            if (FVector::DotProduct(TexDir, dir) < CosAng) continue; // fuera del cono

            FColor& Dst = HeadPaintPixels[py * N + px];
            if (Dst.A == 0) continue;
            Dst = FColor(0, 0, 0, 0);
            if (HDirtyMaxX < HDirtyMinX) { HDirtyMinX = HDirtyMaxX = px; HDirtyMinY = HDirtyMaxY = py; }
            else { HDirtyMinX = FMath::Min(HDirtyMinX, px); HDirtyMaxX = FMath::Max(HDirtyMaxX, px);
                   HDirtyMinY = FMath::Min(HDirtyMinY, py); HDirtyMaxY = FMath::Max(HDirtyMaxY, py); }
        }
    }
}

void APTLobbyCharacter::ClearHeadPaint()
{
    if (!HeadPaintTex || HeadPaintPixels.Num() == 0) return;
    for (FColor& C : HeadPaintPixels) C = FColor(0, 0, 0, 0);
    HDirtyMinX = HDirtyMinY = 0; HDirtyMaxX = HeadPaintN - 1; HDirtyMaxY = HeadPaintN - 1;
    FlushHeadPaint();
    RecomputeHeadPaintBytes();
}

static void PT_UploadWholeTex(UTexture2D* Tex, const TArray<FColor>& Px); // definido más abajo

void APTLobbyCharacter::ClearBodyPaint()
{
    if (!PaintTex || PaintPixels.Num() == 0) return;
    for (FColor& C : PaintPixels) C = FColor(0, 0, 0, 0);
    // Subir la textura COMPLETA (igual que UndoBodyPaint, que anda seguro), en vez del flush parcial:
    // así el reset se ve siempre, sin depender de la región sucia.
    PT_UploadWholeTex(PaintTex, PaintPixels);
    // Descartar cualquier región sucia pendiente (ya subimos todo). Sentinel "vacío" del código.
    DirtyMinX = DirtyMinY = 0; DirtyMaxX = DirtyMaxY = -1;
    RecomputeBodyPaintBytes();
}

void APTLobbyCharacter::RecomputeHeadPaintBytes()
{
    TArray<uint8> P;
    CachedHeadPngBytes = (HeadPaintPixels.Num() > 0 && PT_EncodePNG_BGRA(HeadPaintPixels, HeadPaintN, P)) ? P.Num() : 0;
}
void APTLobbyCharacter::RecomputeBodyPaintBytes()
{
    TArray<uint8> P;
    CachedBodyPngBytes = (PaintPixels.Num() > 0 && PT_EncodePNG_BGRA(PaintPixels, PaintTexN, P)) ? P.Num() : 0;
}

// ── Undo de pintura (snapshot por trazo) ──────────────────────────────────────
static void PT_UploadWholeTex(UTexture2D* Tex, const TArray<FColor>& Px)
{
    if (!Tex || Px.Num() == 0) return;
    FTexture2DMipMap& Mip = Tex->GetPlatformData()->Mips[0];
    void* D = Mip.BulkData.Lock(LOCK_READ_WRITE);
    FMemory::Memcpy(D, Px.GetData(), Px.Num() * sizeof(FColor));
    Mip.BulkData.Unlock();
    Tex->UpdateResource();
}

void APTLobbyCharacter::PushHeadPaintUndo()
{
    if (HeadPaintPixels.Num() == 0) return;
    HeadPaintUndoStack.Add(HeadPaintPixels); // copia del estado ANTES del trazo
    while (HeadPaintUndoStack.Num() > MaxPaintUndo) HeadPaintUndoStack.RemoveAt(0);
}
bool APTLobbyCharacter::UndoHeadPaint()
{
    if (HeadPaintUndoStack.Num() == 0 || !HeadPaintTex) return false;
    HeadPaintPixels = MoveTemp(HeadPaintUndoStack.Last());
    HeadPaintUndoStack.Pop();
    PT_UploadWholeTex(HeadPaintTex, HeadPaintPixels);
    RecomputeHeadPaintBytes();
    return true;
}
void APTLobbyCharacter::PushBodyPaintUndo()
{
    if (PaintPixels.Num() == 0) return;
    BodyPaintUndoStack.Add(PaintPixels);
    while (BodyPaintUndoStack.Num() > MaxPaintUndo) BodyPaintUndoStack.RemoveAt(0);
}
bool APTLobbyCharacter::UndoBodyPaint()
{
    if (BodyPaintUndoStack.Num() == 0 || !PaintTex) return false;
    PaintPixels = MoveTemp(BodyPaintUndoStack.Last());
    BodyPaintUndoStack.Pop();
    PT_UploadWholeTex(PaintTex, PaintPixels);
    RecomputeBodyPaintBytes();
    return true;
}

void APTLobbyCharacter::FlushHeadPaint()
{
    if (!HeadPaintTex || HDirtyMaxX < HDirtyMinX || HDirtyMaxY < HDirtyMinY) return;
    const int32 N  = HeadPaintN;
    const int32 x0 = HDirtyMinX, y0 = HDirtyMinY;
    const int32 w  = HDirtyMaxX - HDirtyMinX + 1;
    const int32 h  = HDirtyMaxY - HDirtyMinY + 1;

    uint8* Buf = (uint8*)FMemory::Malloc(w * h * 4);
    for (int32 yy = 0; yy < h; ++yy)
        FMemory::Memcpy(Buf + yy * w * 4, &HeadPaintPixels[(y0 + yy) * N + x0], w * 4);

    FUpdateTextureRegion2D* Region = new FUpdateTextureRegion2D(x0, y0, 0, 0, w, h);
    HeadPaintTex->UpdateTextureRegions(0, 1, Region, (uint32)(w * 4), 4, Buf,
        [](uint8* Src, const FUpdateTextureRegion2D* Reg) { FMemory::Free(Src); delete Reg; });

    HDirtyMinX = HDirtyMinY = 0; HDirtyMaxX = HDirtyMaxY = -1;
}
