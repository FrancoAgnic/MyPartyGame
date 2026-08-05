// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasPawnSilla.h"
#include "SillasAbilityComponent.h"
#include "SillasBalanceData.h"
#include "SillasGameState.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerState.h" // tipo completo para GetNameSafe(GetPlayerState()) en 5.8
#include "GameFramework/SpringArmComponent.h"
// Definiciones completas de los assets de input: FObjectFinder (5.8) guarda
// TObjectPtr y la conversión a UObject* exige el tipo completo, no el forward.
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ASillasPawnSilla::ASillasPawnSilla()
{
    // Tick solo lo usa el server para drenar/regenerar stamina.
    PrimaryActorTick.bCanEverTick = true;

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

    Habilidad = CreateDefaultSubobject<USillasAbilityComponent>(TEXT("Habilidad"));

    // Defaults de input: assets del template en /Game/Input (el BP puede pisarlos).
    static ConstructorHelpers::FObjectFinder<UInputAction> IAMove(
        TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
    if (IAMove.Succeeded()) MoveAction = IAMove.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IALook(
        TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
    if (IALook.Succeeded()) LookAction = IALook.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IAMouseLook(
        TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
    if (IAMouseLook.Succeeded()) MouseLookAction = IAMouseLook.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IAJump(
        TEXT("/Game/Input/Actions/IA_Jump.IA_Jump"));
    if (IAJump.Succeeded()) JumpAction = IAJump.Object;
}

void ASillasPawnSilla::BeginPlay()
{
    Super::BeginPlay();

    StaminaActual = GetBalance()->StaminaSprintSeg;
    AplicarVelocidad();
}

void ASillasPawnSilla::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (!HasAuthority()) return;

    // D10: drenar stamina solo si sprintea Y se mueve; regenerar si no sprintea.
    // (El sprint parado no gasta pero tampoco sirve — y el sonido de Fase 4 va
    // a delatar el movimiento real, no la tecla.)
    const bool bMoviendose = GetVelocity().SizeSquared2D() > 25.f;
    const USillasBalanceData* B = GetBalance();

    if (bSprint && bMoviendose)
    {
        StaminaActual -= DeltaSeconds;
        if (StaminaActual <= 0.f)
        {
            StaminaActual = 0.f;
            bSprint = false;
            OnRep_Sprint(); // el listen host no recibe su propio OnRep
        }
    }
    else if (!bSprint)
    {
        StaminaActual = FMath::Min(StaminaActual + B->StaminaRegenPorSeg * DeltaSeconds,
                                   B->StaminaSprintSeg);
    }
}

void ASillasPawnSilla::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (KitIMC)
    {
        if (const APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                Subsystem->RemoveMappingContext(KitIMC);
            }
        }
    }
    Super::EndPlay(EndPlayReason);
}

void ASillasPawnSilla::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASillasPawnSilla, bSprint);
    DOREPLIFETIME(ASillasPawnSilla, StaminaActual);
}

void ASillasPawnSilla::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Kit D10 sin assets: crear acciones + contexto en runtime.
    if (!SprintAction)
    {
        UInputAction* IA = NewObject<UInputAction>(this, TEXT("IA_Sprint_Runtime"));
        IA->ValueType = EInputActionValueType::Boolean;
        SprintAction = IA;
    }
    if (!EmpujonAction)
    {
        UInputAction* IA = NewObject<UInputAction>(this, TEXT("IA_Empujon_Runtime"));
        IA->ValueType = EInputActionValueType::Boolean;
        EmpujonAction = IA;
    }
    if (!HabilidadAction)
    {
        UInputAction* IA = NewObject<UInputAction>(this, TEXT("IA_Habilidad_Runtime"));
        IA->ValueType = EInputActionValueType::Boolean;
        HabilidadAction = IA;
    }
    if (!KitIMC)
    {
        KitIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_KitSilla_Runtime"));
        KitIMC->MapKey(SprintAction,    EKeys::LeftShift);
        KitIMC->MapKey(EmpujonAction,   EKeys::LeftMouseButton);
        KitIMC->MapKey(HabilidadAction, EKeys::Q);
    }
    if (const APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(KitIMC, /*Priority=*/10);
        }
    }

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
        if (SprintAction)
        {
            Input->BindAction(SprintAction, ETriggerEvent::Started,   this, &ASillasPawnSilla::OnSprintPressed);
            Input->BindAction(SprintAction, ETriggerEvent::Completed, this, &ASillasPawnSilla::OnSprintReleased);
            Input->BindAction(SprintAction, ETriggerEvent::Canceled,  this, &ASillasPawnSilla::OnSprintReleased);
        }
        if (EmpujonAction)
            Input->BindAction(EmpujonAction, ETriggerEvent::Started, this, &ASillasPawnSilla::OnEmpujon);
        if (HabilidadAction)
            Input->BindAction(HabilidadAction, ETriggerEvent::Started, this, &ASillasPawnSilla::OnHabilidad);
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

void ASillasPawnSilla::OnSprintPressed()
{
    if (!HasAuthority()) { bSprint = true; OnRep_Sprint(); } // predicción local
    Server_SetSprint(true);
}

void ASillasPawnSilla::OnSprintReleased()
{
    if (!HasAuthority()) { bSprint = false; OnRep_Sprint(); }
    Server_SetSprint(false);
}

void ASillasPawnSilla::Server_SetSprint_Implementation(bool bNuevo)
{
    if (bNuevo && StaminaActual <= 0.f) bNuevo = false; // sin nafta no hay sprint

    if (bSprint != bNuevo)
    {
        bSprint = bNuevo;
        OnRep_Sprint();
    }
}

void ASillasPawnSilla::OnEmpujon()
{
    Server_Empujar();
}

void ASillasPawnSilla::Server_Empujar_Implementation()
{
    const USillasBalanceData* B = GetBalance();
    const ASillasGameState* GS = GetWorld()->GetGameState<ASillasGameState>();
    if (!GS) return;

    // Cooldown server-side (anti spam).
    const float Ahora = GS->GetServerWorldTimeSeconds();
    if (Ahora - UltimoEmpujonServerTime < B->EmpujonCooldownSeg) return;

    // Buscar otra silla-jugador en el cono frontal (60°) dentro del alcance.
    const FVector MiPos = GetActorLocation();
    const FVector Frente = GetActorForwardVector();
    ASillasPawnSilla* Objetivo = nullptr;
    float MejorDist = B->EmpujonDistancia;

    for (TActorIterator<ASillasPawnSilla> It(GetWorld()); It; ++It)
    {
        if (*It == this) continue;
        const FVector Delta = It->GetActorLocation() - MiPos;
        if (FMath::Abs(Delta.Z) > 150.f) continue;
        const float Dist = Delta.Size2D();
        if (Dist > MejorDist) continue;
        if (FVector::DotProduct(Frente, Delta.GetSafeNormal2D()) < 0.5f) continue; // cos(60°)
        Objetivo  = *It;
        MejorDist = Dist;
    }

    if (!Objetivo) return;

    UltimoEmpujonServerTime = Ahora;

    // D2/D10: la traición física — impulso replicado por el movimiento del Character.
    const FVector Dir = (Objetivo->GetActorLocation() - MiPos).GetSafeNormal2D();
    Objetivo->LaunchCharacter(
        Dir * B->ImpulsoEmpujon + FVector(0.f, 0.f, B->ImpulsoEmpujonVertical),
        /*bXYOverride=*/true, /*bZOverride=*/true);

    UE_LOG(LogTemp, Log, TEXT("[Sillas] Empujón: %s empujó a %s."),
           *GetNameSafe(GetPlayerState()), *GetNameSafe(Objetivo->GetPlayerState()));
}

void ASillasPawnSilla::OnHabilidad()
{
    if (Habilidad)
    {
        Habilidad->IntentarActivar();
    }
}

void ASillasPawnSilla::OnRep_Sprint()
{
    AplicarVelocidad();
}

void ASillasPawnSilla::AplicarVelocidad()
{
    const USillasBalanceData* B = GetBalance();
    GetCharacterMovement()->MaxWalkSpeed =
        bSprint ? B->VelocidadSprintSilla : B->VelocidadCaminataSilla;
}

const USillasBalanceData* ASillasPawnSilla::GetBalance() const
{
    if (const ASillasGameState* GS = GetWorld()->GetGameState<ASillasGameState>())
    {
        if (GS->Balance) return GS->Balance;
    }
    return GetDefault<USillasBalanceData>();
}
