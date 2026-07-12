// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyCharacter.h"
#include "PTPlayerState.h"
#include "PTNameTagWidget.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "ProceduralMeshComponent.h"
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
}

static const TCHAR* PTHeadSaveSlot = TEXT("PTHeadCustom");

TArray<FPTHeadSection> APTLobbyCharacter::ExtractSections(UProceduralMeshComponent* Src) const
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
        for (const FProcMeshVertex& V : Sec->ProcVertexBuffer)
        {
            H.Verts.Add(V.Position); H.Normals.Add(V.Normal); H.UVs.Add(V.UV0); H.Colors.Add(V.Color);
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
        if (HeadMaterial) HeadMesh->SetMaterial(s, HeadMaterial);
    }
}

void APTLobbyCharacter::SetHeadMeshFrom(UProceduralMeshComponent* Src)
{
    if (!HeadMesh || !Src) return;
    ApplyHeadSections(ExtractSections(Src));
    SaveHead(); // hornear = guardar tu cabeza (v1 local)
}

void APTLobbyCharacter::ClearHeadMesh()
{
    if (HeadMesh) HeadMesh->ClearAllMeshSections();
}

void APTLobbyCharacter::SaveHead()
{
    if (!HeadMesh) return;
    UPTHeadSaveGame* Save = Cast<UPTHeadSaveGame>(
        UGameplayStatics::CreateSaveGameObject(UPTHeadSaveGame::StaticClass()));
    if (!Save) return;
    Save->Sections = ExtractSections(HeadMesh);
    UGameplayStatics::SaveGameToSlot(Save, PTHeadSaveSlot, 0);
}

void APTLobbyCharacter::LoadHead()
{
    if (!UGameplayStatics::DoesSaveGameExist(PTHeadSaveSlot, 0)) return;
    if (UPTHeadSaveGame* Save = Cast<UPTHeadSaveGame>(UGameplayStatics::LoadGameFromSlot(PTHeadSaveSlot, 0)))
        if (Save->Sections.Num() > 0)
            ApplyHeadSections(Save->Sections);
}

void APTLobbyCharacter::BeginPlay()
{
    Super::BeginPlay();
    // v1 LOCAL: solo el pawn del jugador local restaura SU cabeza guardada (no se toca a los demás).
    if (IsLocallyControlled())
        LoadHead();
}

void APTLobbyCharacter::SetSculptPose(bool bEnable)
{
    USkeletalMeshComponent* M = GetMesh();
    if (!M) return;

    if (bEnable)
    {
        // Recto y quieto mientras esculpís: apagar la física (jiggle del physics asset) y
        // bypassear el AnimBP (que hace el baile y/o el AnimDynamics) → queda en pose base.
        M->SetSimulatePhysics(false);
        M->SetAllBodiesSimulatePhysics(false);
        M->PutAllRigidBodiesToSleep();
        // Cortar cualquier montage en curso (ej: el salto que seguía en loop).
        if (UAnimInstance* AI = M->GetAnimInstance())
            AI->StopAllMontages(0.f);
        M->SetAnimationMode(EAnimationMode::AnimationSingleNode);
        if (SculptPoseAnim)
            M->PlayAnimation(SculptPoseAnim, /*bLooping*/ true); // pose de referencia quieta
        else
            M->SetPosition(0.f, false);                          // sin pose: ref pose (frame 0)
        M->bPauseAnims = true;                                   // congelar del todo (no avanza)
    }
    else
    {
        // Restaurar la animación normal (el AnimBP vuelve a manejar baile + jiggle).
        M->bPauseAnims = false;
        M->SetAnimationMode(EAnimationMode::AnimationBlueprint);
    }
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
            W->SetPlayerName(PS->DisplayName);
}

void APTLobbyCharacter::Multicast_ShowChatBubble_Implementation(const FString& Text, bool bGuess)
{
    if (NameTag)
    {
        ChatBubbleUntil = GetWorld() ? GetWorld()->GetTimeSeconds() + 2.f : 0.f;
        NameTag->SetVisibility(true);
        if (UPTNameTagWidget* W = Cast<UPTNameTagWidget>(NameTag->GetUserWidgetObject()))
        {
            if (bGuess) W->ShowGuessMessage(Text); // verde
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
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastJumpTime < DoubleTapWindow)
        ToggleFly(); // doble toque de espacio → alterna vuelo
    LastJumpTime = Now;

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
    SetFlyingMode(!bFlying);
}

void APTLobbyCharacter::SetFlyingMode(bool bEnable)
{
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
    if (NameTagAccum >= 0.5f) { NameTagAccum = 0.f; UpdateNameTag(); }

    if (!bFlying) return;
    if (bAscend)  AddMovementInput(FVector::UpVector,  1.f);
    if (bDescend) AddMovementInput(FVector::UpVector, -1.f);
}
