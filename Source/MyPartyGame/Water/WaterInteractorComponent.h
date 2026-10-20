// Stone Splash — interacción SELECTIVA con el agua (estilo FluidFlux).
// Agregá este componente al personaje o a los objetos que QUERÉS que generen ondas en el AWaterSim.
// Lo que NO tenga este componente, no interactúa. El AWaterSim recorre los componentes registrados y,
// según su movimiento cerca de la superficie, inyecta perturbaciones (ondas) que luego se propagan,
// rebotan en costa/bordes y se suman entre sí (eso lo hace la sim).
#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WaterInteractorComponent.generated.h"

class AWaterSim;

UCLASS(ClassGroup=(WaterFX), meta=(BlueprintSpawnableComponent))
class MYPARTYGAME_API UWaterInteractorComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UWaterInteractorComponent();

	// Fuerza base de la onda que genera este objeto al moverse en el agua.
	UPROPERTY(EditAnywhere, Category="WaterFX") float Strength = 4.f;
	// Radio de la perturbación en cm (tamaño del objeto en el agua).
	UPROPERTY(EditAnywhere, Category="WaterFX") float Radius = 120.f;
	// Velocidad mínima (cm/s) para generar ondas (correr/caer). Quieto = no molesta el agua.
	UPROPERTY(EditAnywhere, Category="WaterFX") float MinSpeed = 40.f;
	// A qué velocidad la onda llega a fuerza "plena" (para normalizar).
	UPROPERTY(EditAnywhere, Category="WaterFX") float RefSpeed = 400.f;
	// Distancia a la superficie (cm) para considerar que está "tocando" el agua.
	UPROPERTY(EditAnywhere, Category="WaterFX") float SurfaceTolerance = 70.f;
	// Clase de agua con la que interactúa (si hay varias). Null = la primera AWaterSim del nivel.
	UPROPERTY(EditAnywhere, Category="WaterFX") bool bAutoFindWater = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	TWeakObjectPtr<AWaterSim> Water;
};
