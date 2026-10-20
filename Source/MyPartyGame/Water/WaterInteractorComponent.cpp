#include "WaterInteractorComponent.h"
#include "WaterSim.h"
#include "Kismet/GameplayStatics.h"

UWaterInteractorComponent::UWaterInteractorComponent()
{
	PrimaryComponentTick.bCanEverTick = false; // lo procesa el AWaterSim (un solo lugar)
}

void UWaterInteractorComponent::BeginPlay()
{
	Super::BeginPlay();
	if (bAutoFindWater && GetWorld())
	{
		if (AActor* A = UGameplayStatics::GetActorOfClass(GetWorld(), AWaterSim::StaticClass()))
		{
			Water = Cast<AWaterSim>(A);
			if (Water.IsValid()) Water->RegisterInteractor(this);
		}
	}
}

void UWaterInteractorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Water.IsValid()) Water->UnregisterInteractor(this);
	Super::EndPlay(Reason);
}
