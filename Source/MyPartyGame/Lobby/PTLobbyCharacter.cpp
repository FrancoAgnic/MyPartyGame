// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyCharacter.h"
#include "PTPlayerState.h"
#include "PTNameTagWidget.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/WidgetComponent.h"
#include "ProceduralMeshComponent.h"
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

void APTLobbyCharacter::SetHeadMeshFrom(UProceduralMeshComponent* Src)
{
    if (!HeadMesh || !Src) return;

    HeadMesh->ClearAllMeshSections();
    const int32 NumSec = Src->GetNumSections();
    for (int32 s = 0; s < NumSec; ++s)
    {
        const FProcMeshSection* Sec = Src->GetProcMeshSection(s);
        if (!Sec || Sec->ProcVertexBuffer.Num() == 0) continue;

        TArray<FVector>          Verts;    Verts.Reserve(Sec->ProcVertexBuffer.Num());
        TArray<FVector>          Normals;  Normals.Reserve(Sec->ProcVertexBuffer.Num());
        TArray<FVector2D>        UVs;      UVs.Reserve(Sec->ProcVertexBuffer.Num());
        TArray<FColor>           Colors;   Colors.Reserve(Sec->ProcVertexBuffer.Num());
        TArray<FProcMeshTangent> Tangents; Tangents.Reserve(Sec->ProcVertexBuffer.Num());
        for (const FProcMeshVertex& V : Sec->ProcVertexBuffer)
        {
            Verts.Add(V.Position); Normals.Add(V.Normal); UVs.Add(V.UV0);
            Colors.Add(V.Color);   Tangents.Add(V.Tangent);
        }
        TArray<int32> Tris;
        Tris.Reserve(Sec->ProcIndexBuffer.Num());
        for (uint32 Idx : Sec->ProcIndexBuffer) Tris.Add((int32)Idx);

        HeadMesh->CreateMeshSection(s, Verts, Tris, Normals, UVs, Colors, Tangents, /*bCreateCollision=*/false);
    }

    // Reusar el material de arcilla del volumen de origen (para que se vea igual).
    if (UMaterialInterface* Mat = Src->GetMaterial(0))
        HeadMesh->SetMaterial(0, Mat);
}

void APTLobbyCharacter::ClearHeadMesh()
{
    if (HeadMesh) HeadMesh->ClearAllMeshSections();
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
