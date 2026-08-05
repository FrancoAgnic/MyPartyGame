// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasPawnSilla.h"
#include "SillasBalanceData.h"
#include "SillasGameState.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
// Definiciones completas de los assets de input: FObjectFinder (5.8) guarda
// TObjectPtr y la conversión a UObject* exige el tipo completo, no el forward.
#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "UObject/ConstructorHelpers.h"

ASillasPawnSilla::ASillasPawnSilla()
{
    PrimaryActorTick.bCanEverTick = false;

    // Cápsula del tamaño de la caja-silla (50x50x100 → radio 35, media altura 50).
    GetCapsuleComponent()->InitCapsuleSize(35.f, 50.f);

    // La silla no gira con la cámara: rota hacia donde se mueve, como un mueble
    // que se desplaza (la cámara orbita libre con el spring arm).
    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
    GetCharacterMovement()->JumpZVelocity = 420.f; // D10: las sillas saltan

    // Caja IDÉNTICA al señuelo (ver ASillasSenuelo — regla de oro del camuflaje).
    ChairMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ChairMesh"));
    ChairMesh->SetupAttachment(GetCapsuleComponent());
    ChairMesh->SetRelativeLocation(FVector(0.f, 0.f, -50.f));
    ChairMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); // colisiona la cápsula
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cubo(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cubo.Succeeded())
    {
        ChairMesh->SetStaticMesh(Cubo.Object);
        ChairMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.0f));
    }

    // Cámara tercera persona.
    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GetCapsuleComponent());
    SpringArm->TargetArmLength = 400.f;
    SpringArm->bUsePawnControlRotation = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);

    // Defaults de input: los assets del template ThirdPerson (el BP puede pisarlos).
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCDefault(
        TEXT("/Game/ThirdPerson/Input/IMC_Default.IMC_Default"));
    if (IMCDefault.Succeeded()) MappingContext = IMCDefault.Object;

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCMouse(
        TEXT("/Game/ThirdPerson/Input/IMC_MouseLook.IMC_MouseLook"));
    if (IMCMouse.Succeeded()) MouseLookMappingContext = IMCMouse.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IAMove(
        TEXT("/Game/ThirdPerson/Input/Actions/IA_Move.IA_Move"));
    if (IAMove.Succeeded()) MoveAction = IAMove.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IALook(
        TEXT("/Game/ThirdPerson/Input/Actions/IA_Look.IA_Look"));
    if (IALook.Succeeded()) LookAction = IALook.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IAMouseLook(
        TEXT("/Game/ThirdPerson/Input/Actions/IA_MouseLook.IA_MouseLook"));
    if (IAMouseLook.Succeeded()) MouseLookAction = IAMouseLook.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IAJump(
        TEXT("/Game/ThirdPerson/Input/Actions/IA_Jump.IA_Jump"));
    if (IAJump.Succeeded()) JumpAction = IAJump.Object;
}

void ASillasPawnSilla::BeginPlay()
{
    Super::BeginPlay();

    // Velocidad desde BalanceData (replicado vía GameState) — servidor y cliente
    // leen el mismo asset, así el movimiento predice sin rubber-banding.
    const USillasBalanceData* Balance = nullptr;
    if (const ASillasGameState* GS = GetWorld()->GetGameState<ASillasGameState>())
    {
        Balance = GS->Balance;
    }
    if (!Balance) Balance = GetDefault<USillasBalanceData>();

    GetCharacterMovement()->MaxWalkSpeed = Balance->VelocidadCaminataSilla;
}

void ASillasPawnSilla::PawnClientRestart()
{
    Super::PawnClientRestart();

    // Punto recomendado para (re)aplicar los mapping contexts del pawn local.
    if (const APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            Subsystem->ClearAllMappings();
            if (MappingContext)          Subsystem->AddMappingContext(MappingContext, 0);
            if (MouseLookMappingContext) Subsystem->AddMappingContext(MouseLookMappingContext, 1);
        }
    }
}

void ASillasPawnSilla::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction)
            Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASillasPawnSilla::Move);
        if (LookAction)
            Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASillasPawnSilla::Look);
        if (MouseLookAction)
            Input->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASillasPawnSilla::Look);
        if (JumpAction)
        {
            Input->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
            Input->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
        }
    }
}

void ASillasPawnSilla::Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller) return;

    // Movimiento relativo a la cámara (yaw del control), estilo tercera persona.
    const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
    AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), Axis.Y);
    AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), Axis.X);
}

void ASillasPawnSilla::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(Axis.Y);
}
