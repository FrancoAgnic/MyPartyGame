// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasPawnCazador.h"
#include "SillasBalanceData.h"
#include "SillasGameMode.h"
#include "SillasGameState.h"
#include "SillasPawnSilla.h"
#include "SillasSenuelo.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ASillasPawnCazador::ASillasPawnCazador()
{
    // Tick solo lo usa el server para chequear el sentado mientras captura.
    PrimaryActorTick.bCanEverTick = true;

    GetCapsuleComponent()->InitCapsuleSize(35.f, 90.f);

    bUseControllerRotationYaw = false;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 500.f, 0.f);
    // D17: el cazador no salta ni sprinta — caza con oído y memoria.
    GetCharacterMovement()->NavAgentProps.bCanJump = false;

    // Cuerpo greybox: caja alta 40x40x180.
    CuerpoMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CuerpoMesh"));
    CuerpoMesh->SetupAttachment(GetCapsuleComponent());
    CuerpoMesh->SetRelativeLocation(FVector(0.f, 0.f, 0.f));
    CuerpoMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cubo(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cubo.Succeeded())
    {
        CuerpoMesh->SetStaticMesh(Cubo.Object);
        CuerpoMesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 1.8f));
    }

    SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
    SpringArm->SetupAttachment(GetCapsuleComponent());
    SpringArm->TargetArmLength = 450.f;
    SpringArm->bUsePawnControlRotation = true;

    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
    Camera->SetupAttachment(SpringArm);

    static ConstructorHelpers::FObjectFinder<UInputAction> IAMove(
        TEXT("/Game/Input/Actions/IA_Move.IA_Move"));
    if (IAMove.Succeeded()) MoveAction = IAMove.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IALook(
        TEXT("/Game/Input/Actions/IA_Look.IA_Look"));
    if (IALook.Succeeded()) LookAction = IALook.Object;

    static ConstructorHelpers::FObjectFinder<UInputAction> IAMouseLook(
        TEXT("/Game/Input/Actions/IA_MouseLook.IA_MouseLook"));
    if (IAMouseLook.Succeeded()) MouseLookAction = IAMouseLook.Object;
}

void ASillasPawnCazador::BeginPlay()
{
    Super::BeginPlay();
    AplicarEstadoAMovimiento();
}

void ASillasPawnCazador::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);

    if (HasAuthority() && bCapturando)
    {
        ChequearSentado();
    }
}

void ASillasPawnCazador::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // Sacar el contexto runtime del clic para no acumular contextos huérfanos
    // al re-poseer pawns (cada instancia crea el suyo).
    if (CapturaIMC)
    {
        if (const APlayerController* PC = Cast<APlayerController>(GetController()))
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
            {
                Subsystem->RemoveMappingContext(CapturaIMC);
            }
        }
    }
    Super::EndPlay(EndPlayReason);
}

void ASillasPawnCazador::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASillasPawnCazador, bCapturando);
    DOREPLIFETIME(ASillasPawnCazador, bAdolorido);
}

void ASillasPawnCazador::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    // Sin asset de captura: crear acción + contexto en runtime (clic izquierdo).
    if (!CaptureAction)
    {
        UInputAction* IA = NewObject<UInputAction>(this, TEXT("IA_Captura_Runtime"));
        IA->ValueType = EInputActionValueType::Boolean;
        CaptureAction = IA;
    }
    if (!CapturaIMC)
    {
        CapturaIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_Captura_Runtime"));
        CapturaIMC->MapKey(CaptureAction, EKeys::LeftMouseButton);
    }
    if (const APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(CapturaIMC, /*Priority=*/10);
        }
    }

    if (UEnhancedInputComponent* Input = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        if (MoveAction)
            Input->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASillasPawnCazador::Move);
        if (LookAction)
            Input->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASillasPawnCazador::Look);
        if (MouseLookAction)
            Input->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASillasPawnCazador::Look);
        if (CaptureAction)
        {
            Input->BindAction(CaptureAction, ETriggerEvent::Started,   this, &ASillasPawnCazador::OnCapturaPressed);
            Input->BindAction(CaptureAction, ETriggerEvent::Completed, this, &ASillasPawnCazador::OnCapturaReleased);
            Input->BindAction(CaptureAction, ETriggerEvent::Canceled,  this, &ASillasPawnCazador::OnCapturaReleased);
        }
    }
}

void ASillasPawnCazador::Move(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    if (!Controller) return;

    const FRotator YawRot(0.f, Controller->GetControlRotation().Yaw, 0.f);
    const FVector Fwd   = FRotationMatrix(YawRot).GetUnitAxis(EAxis::X);
    const FVector Right = FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y);
    AddMovementInput(Fwd, Axis.Y);
    AddMovementInput(Right, Axis.X);

    // Caminata de cola: el pawn mira al REVÉS de donde avanza (la cola apunta
    // al objetivo). La rotación del pawn la manda el cliente dueño (cosmética);
    // la validación del sentado es del server con esta misma orientación replicada.
    if (bCapturando)
    {
        const FVector Dir = (Fwd * Axis.Y + Right * Axis.X).GetSafeNormal2D();
        if (!Dir.IsNearlyZero())
        {
            SetActorRotation(FRotationMatrix::MakeFromX(-Dir).Rotator());
        }
    }
}

void ASillasPawnCazador::Look(const FInputActionValue& Value)
{
    const FVector2D Axis = Value.Get<FVector2D>();
    AddControllerYawInput(Axis.X);
    AddControllerPitchInput(Axis.Y);
}

void ASillasPawnCazador::OnCapturaPressed()
{
    // Predicción básica local; el server valida (fase Silencio, sin dolor) y
    // corrige por OnRep si no corresponde.
    if (!HasAuthority())
    {
        bCapturando = true;
        OnRep_Estado();
    }
    Server_SetCapturando(true);
}

void ASillasPawnCazador::OnCapturaReleased()
{
    if (!HasAuthority())
    {
        bCapturando = false;
        OnRep_Estado();
    }
    Server_SetCapturando(false);
}

void ASillasPawnCazador::Server_SetCapturando_Implementation(bool bNueva)
{
    if (bNueva)
    {
        // D5+D11: capturar solo se puede en el Silencio; jamás adolorido.
        const ASillasGameState* GS = GetWorld()->GetGameState<ASillasGameState>();
        if (!GS || GS->Fase != ESillasFase::Silencio || bAdolorido)
        {
            bNueva = false;
        }
    }

    if (bCapturando != bNueva)
    {
        bCapturando = bNueva;
        OnRep_Estado(); // el listen host no recibe su propio OnRep
    }
}

void ASillasPawnCazador::CancelarCapturaServer()
{
    if (!HasAuthority()) return;
    if (bCapturando)
    {
        bCapturando = false;
        OnRep_Estado();
    }
}

void ASillasPawnCazador::AplicarDolorServer()
{
    if (!HasAuthority()) return;

    bCapturando = false;
    bAdolorido  = true;
    OnRep_Estado();

    GetWorldTimerManager().SetTimer(DolorTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
    {
        bAdolorido = false;
        OnRep_Estado();
    }), GetBalance()->DuracionDolorSeg, /*bLoop=*/false);
}

void ASillasPawnCazador::OnRep_Estado()
{
    AplicarEstadoAMovimiento();
}

void ASillasPawnCazador::AplicarEstadoAMovimiento()
{
    const USillasBalanceData* B = GetBalance();
    UCharacterMovementComponent* Mv = GetCharacterMovement();

    if (bAdolorido)
    {
        Mv->MaxWalkSpeed = B->VelocidadCazador * B->MultiplicadorVelocidadDolor;
    }
    else if (bCapturando)
    {
        Mv->MaxWalkSpeed = B->VelocidadCaminataCola;
    }
    else
    {
        Mv->MaxWalkSpeed = B->VelocidadCazador;
    }

    // Capturando: la rotación la maneja Move() (de espaldas); normal: mirar
    // hacia donde camina.
    Mv->bOrientRotationToMovement = !bCapturando;
}

void ASillasPawnCazador::ChequearSentado()
{
    const USillasBalanceData* B = GetBalance();
    const FVector MiPos    = GetActorLocation();
    const FVector DirCola  = -GetActorForwardVector(); // de espaldas: la cola es -forward
    const float   CosLimite = FMath::Cos(FMath::DegreesToRadians(B->AnguloSentadoValidoGrados));

    AActor* Mejor      = nullptr;
    float   MejorDist  = B->DistanciaSentadoValido;

    auto Considerar = [&](AActor* Candidato)
    {
        const FVector Delta = Candidato->GetActorLocation() - MiPos;
        if (FMath::Abs(Delta.Z) > 150.f) return;          // misma planta
        const float Dist = Delta.Size2D();
        if (Dist > MejorDist) return;
        const FVector DirA = Delta.GetSafeNormal2D();
        if (FVector::DotProduct(DirCola, DirA) < CosLimite) return; // fuera del cono de cola
        Mejor     = Candidato;
        MejorDist = Dist;
    };

    for (TActorIterator<ASillasSenuelo> It(GetWorld()); It; ++It)   Considerar(*It);
    for (TActorIterator<ASillasPawnSilla> It(GetWorld()); It; ++It) Considerar(*It);

    if (Mejor)
    {
        if (ASillasGameMode* GM = GetWorld()->GetAuthGameMode<ASillasGameMode>())
        {
            GM->ResolverSentado(this, Mejor);
        }
    }
}

const USillasBalanceData* ASillasPawnCazador::GetBalance() const
{
    if (const ASillasGameState* GS = GetWorld()->GetGameState<ASillasGameState>())
    {
        if (GS->Balance) return GS->Balance;
    }
    return GetDefault<USillasBalanceData>();
}
