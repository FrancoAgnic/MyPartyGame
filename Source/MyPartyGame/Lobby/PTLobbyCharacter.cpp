// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyCharacter.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
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

    // Vuelo sin inercia: frenar apenas se sueltan los inputs (para/arranca al toque).
    GetCharacterMovement()->BrakingDecelerationFlying = 100000.f;
    GetCharacterMovement()->bUseSeparateBrakingFriction = true;
    GetCharacterMovement()->BrakingFriction = 10.f;

    // ACharacter + CharacterMovementComponent replican movimiento y rotación automáticamente.
    SetReplicates(true);
    SetReplicateMovement(true);
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

    // Descenso en vuelo: Shift izquierdo (tecla legacy, no interfiere con Enhanced Input).
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed,  this, &APTLobbyCharacter::OnDescendPressed);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &APTLobbyCharacter::OnDescendReleased);
}

// ── Vuelo (modo creativo Minecraft) ─────────────────────────────────────────

void APTLobbyCharacter::OnJumpPressed()
{
    const float Now = GetWorld()->GetTimeSeconds();
    if (Now - LastJumpTime < DoubleTapWindow)
        ToggleFly(); // doble toque de espacio → alterna vuelo
    LastJumpTime = Now;

    if (bFlying) bAscend = true;
    else         Jump();
}

void APTLobbyCharacter::OnJumpReleased()
{
    bAscend = false;
    if (!bFlying) StopJumping();
}

void APTLobbyCharacter::ToggleFly()
{
    bFlying = !bFlying;
    UCharacterMovementComponent* M = GetCharacterMovement();
    if (bFlying)
    {
        M->MaxFlySpeed = FlySpeed;
        M->SetMovementMode(MOVE_Flying);
    }
    else
    {
        M->SetMovementMode(MOVE_Walking);
        bAscend = bDescend = false;
    }
}

void APTLobbyCharacter::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bFlying) return;
    if (bAscend)  AddMovementInput(FVector::UpVector,  1.f);
    if (bDescend) AddMovementInput(FVector::UpVector, -1.f);
}
