#include "Rock.h"
#include "BuoyancyComponent.h"
#include "BuoyancyTypes.h"
#include "Components/StaticMeshComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/ConstructorHelpers.h"

ARock::ARock()
{
	PrimaryActorTick.bCanEverTick = false;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	SetRootComponent(Mesh);
	Mesh->SetSimulatePhysics(true);
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECollisionChannel::ECC_PhysicsBody);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetLinearDamping(0.1f);
	Mesh->SetAngularDamping(0.5f);
	// Malla por defecto (esfera del engine) para que ande "out of the box"; se puede cambiar por una roca.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereMesh.Succeeded()) Mesh->SetStaticMesh(SphereMesh.Object);

	Buoyancy = CreateDefaultSubobject<UBuoyancyComponent>(TEXT("Buoyancy"));
	// Un pontón central: suficiente para una piedra (bota/se hunde según masa). Más pontones (en el BP,
	// BuoyancyData) dan un asentamiento/rotación más ricos si se quiere.
	Buoyancy->AddCustomPontoon(50.f, FVector::ZeroVector);
}

void ARock::BeginPlay()
{
	Super::BeginPlay();

	SetActorScale3D(FVector(FMath::Max(0.05f, SizeScale)));

	if (Mesh)
	{
		Mesh->SetMassOverrideInKg(NAME_None, FMath::Max(0.1f, Mass), true);
		Mesh->WakeAllRigidBodies();
	}

	// Ajustar el radio del pontón al tamaño elegido.
	if (Buoyancy && Buoyancy->BuoyancyData.Pontoons.Num() > 0)
		Buoyancy->BuoyancyData.Pontoons[0].Radius = FMath::Max(5.f, PontoonRadius * SizeScale);

	if (Buoyancy)
	{
		Buoyancy->OnEnteredWaterDelegate.AddDynamic(this, &ARock::HandleEnteredWater);
		Buoyancy->OnExitedWaterDelegate.AddDynamic(this, &ARock::HandleExitedWater);
	}
}

void ARock::HandleEnteredWater(const FSphericalPontoon& Pontoon)
{
	if (bSplashed) return;
	const float Speed = Mesh ? FMath::Abs(Mesh->GetPhysicsLinearVelocity().Z) : 0.f;
	if (Speed < SplashMinSpeed) return;
	bSplashed = true;

	// WaterSurfacePosition = punto de la superficie del agua bajo el pontón → ahí cae el splash.
	const FVector Loc = Pontoon.WaterSurfacePosition;
	const FVector Vel = Mesh ? Mesh->GetPhysicsLinearVelocity() : FVector::ZeroVector;
	const FVector Normal = Pontoon.WaterPlaneNormal.IsNearlyZero() ? FVector::UpVector : Pontoon.WaterPlaneNormal;
	const float Intensity = FMath::Clamp(Speed / FMath::Max(1.f, SplashRefSpeed), 0.25f, 2.5f);
	const float Size = FMath::Max(0.05f, SizeScale) * Intensity;

	if (SplashFX)
	{
		// Sistema en mundo (auto-destruye al terminar). Le pasamos los datos del impacto como user-params
		// para que la simulación de fluido reaccione en tiempo real (fuerza, tamaño, dirección).
		if (UNiagaraComponent* NC = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), SplashFX, Loc, Normal.Rotation(), FVector(Size), /*bAutoDestroy=*/true))
		{
			NC->SetVariableFloat (ParamImpactSpeed,    Speed);
			NC->SetVariableFloat (ParamImpactSize,     Size);
			NC->SetVariableVec3  (ParamImpactVelocity, Vel);
			NC->SetVariableVec3  (ParamSurfaceNormal,  Normal);
		}
	}
	if (SplashSound)
		UGameplayStatics::PlaySoundAtLocation(this, SplashSound, Loc, Intensity);

	OnSplash(Loc, Speed, Size); // hook de BP para ondas/decal extra
}

void ARock::HandleExitedWater(const FSphericalPontoon& Pontoon)
{
	// Si la piedra salió del todo del agua (rebote/salto), rearmar el splash para la próxima entrada.
	if (Buoyancy && !Buoyancy->IsInWaterBody()) bSplashed = false;
}
