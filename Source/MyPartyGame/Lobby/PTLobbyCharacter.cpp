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
#include "StaticMeshResources.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/Compression.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "EnhancedInputComponent.h"
#include "InputActionValue.h"

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
                if (bPainted) Col = PC.ToFColor(true);
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
        UMaterialInterface* Mat = (H.bEye && EyeMaterial) ? EyeMaterial : HeadMaterial;
        if (Mat) HeadMesh->SetMaterial(s, Mat);
    }
}

void APTLobbyCharacter::BakeAndReplicateHead(UProceduralMeshComponent* ClaySrc, APTSculptVolume* PaintSource,
                                             const TArray<FVector4>& LocalEyes)
{
    if (!HeadMesh || !ClaySrc) return;

    // Componer: arcilla (con pintura horneada) + una sección de OJOS (esferas).
    TArray<FPTHeadSection> Secs = ExtractSections(ClaySrc, PaintSource);
    if (LocalEyes.Num() > 0)
    {
        FPTHeadSection Eyes = BuildEyesSection(LocalEyes, HeadEyeMesh, HeadEyeBaseSize);
        if (Eyes.Verts.Num() > 0) Secs.Add(MoveTemp(Eyes));
    }

    ApplyHeadSections(Secs);
    UpdateHeadCollision();
    SaveHead(Secs);               // persistencia local (disco)

    // Replicar: serializar+comprimir → mandar al servidor → PlayerState → OnRep en todos.
    TArray<uint8> Blob;
    SectionsToBlob(Secs, Blob);
    if (APTPlayerState* PS = GetPlayerState<APTPlayerState>()) PS->UploadHead(Blob);
}

void APTLobbyCharacter::ApplyReplicatedHead()
{
    const APTPlayerState* PS = GetPlayerState<APTPlayerState>();
    if (!PS || PS->HeadBlob.Num() == 0) return;
    TArray<FPTHeadSection> Secs;
    if (BlobToSections(PS->HeadBlob, Secs) && Secs.Num() > 0)
    {
        ApplyHeadSections(Secs);
        UpdateHeadCollision();
    }
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

void APTLobbyCharacter::SaveHead(const TArray<FPTHeadSection>& Secs)
{
    UPTHeadSaveGame* Save = Cast<UPTHeadSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UPTHeadSaveGame::StaticClass()));
    if (!Save) return;
    Save->Sections = Secs;
    UGameplayStatics::SaveGameToSlot(Save, PTHeadSaveSlot, 0);
}

void APTLobbyCharacter::LoadHead()
{
    if (!UGameplayStatics::DoesSaveGameExist(PTHeadSaveSlot, 0)) return;
    if (UPTHeadSaveGame* Save = Cast<UPTHeadSaveGame>(UGameplayStatics::LoadGameFromSlot(PTHeadSaveSlot, 0)))
        if (Save->Sections.Num() > 0)
        {
            ApplyHeadSections(Save->Sections);
            UpdateHeadCollision();
            // Replicar tu cabeza guardada (comprimida) para que los demás la vean sin re-esculpir.
            TArray<uint8> Blob;
            SectionsToBlob(Save->Sections, Blob);
            if (APTPlayerState* PS = GetPlayerState<APTPlayerState>()) PS->UploadHead(Blob);
        }
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
        // Restaurar la animación normal (el AnimBP vuelve a manejar baile + jiggle).
        M->bPauseAnims = false;
        M->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    }
}

void APTLobbyCharacter::SetFacingArrowVisible(bool bVisible)
{
    if (FacingArrow) FacingArrow->SetHiddenInGame(!bVisible);
}

void APTLobbyCharacter::UpdateNameTag()
{
    if (!NameTag) return;

    // Globo de chat activo: mostrar el mensaje (a todos, incluso a uno mismo) sin pisarlo.
    if (GetWorld() && GetWorld()->GetTimeSeconds() < ChatBubbleUntil)
    {
        NameTag->SetVisibility(true);
        return;
    }

    // Sin burbuja: no mostrar tu propio nombre (solo ves los de los demás).
    if (IsLocallyControlled())
    {
        NameTag->SetVisibility(false);
        return;
    }
    NameTag->SetVisibility(true);

    if (UPTNameTagWidget* W = Cast<UPTNameTagWidget>(NameTag->GetUserWidgetObject()))
        if (const APTPlayerState* PS = GetPlayerState<APTPlayerState>())
        {
            // DisplayName lo setea el GameMode y se replica; a veces (sobre todo tras el seamless
            // travel) puede llegar vacío. Caer al nombre nativo del PlayerState (el "?Name=" de
            // Steam) evita el cartel en blanco.
            FString N = PS->GetDisplayNameSafe();
            if (N.IsEmpty()) N = PS->GetPlayerName();
            W->SetPlayerName(N);
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
        M->bOrientRotationToMovement = false; // en el gameplay molesta; en el lobby queda lindo
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
