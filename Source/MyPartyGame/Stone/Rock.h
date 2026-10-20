// Stone Splash — piedra con física que interactúa con el agua (Water plugin + Buoyancy).
// Solo con poner Play (y una piedra arriba del agua) cae, entra al agua y, según su masa, se hunde o
// flota/bota, con un splash + ondas al impactar escalados por la velocidad de entrada.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Rock.generated.h"

class UStaticMeshComponent;
class UBuoyancyComponent;
class UNiagaraSystem;
class USoundBase;
struct FSphericalPontoon;

UCLASS()
class MYPARTYGAME_API ARock : public AActor
{
	GENERATED_BODY()
public:
	ARock();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, Category="Rock") UStaticMeshComponent* Mesh;
	UPROPERTY(VisibleAnywhere, Category="Rock") UBuoyancyComponent*  Buoyancy;

	// ── Piedra ──
	// Masa en kg. Más pesada → se HUNDE; liviana → flota/bota. Es lo que hace que cada piedra
	// interactúe distinto con el agua según su peso.
	UPROPERTY(EditAnywhere, Category="Rock") float Mass = 60.f;
	// Escala visual de la piedra (1 = default). También escala el radio de flotación.
	UPROPERTY(EditAnywhere, Category="Rock") float SizeScale = 1.f;
	// Radio base del "pontón" de flotación (muestra de agua). Se multiplica por SizeScale.
	UPROPERTY(EditAnywhere, Category="Rock|Buoyancy") float PontoonRadius = 50.f;

	// ── Splash (cosmético, por cliente) ──
	// Sistema Niagara del splash. Para el look realista en tiempo real: un sistema de Niagara Fluids.
	// Se le pasan user-parameters del impacto (abajo) para que la simulación reaccione a la fuerza/tamaño.
	UPROPERTY(EditAnywhere, Category="Rock|Splash") UNiagaraSystem* SplashFX = nullptr;
	UPROPERTY(EditAnywhere, Category="Rock|Splash") USoundBase*     SplashSound = nullptr;
	// Velocidad mínima de entrada (cm/s) para disparar el splash (evita micro-salpicones al asentarse).
	UPROPERTY(EditAnywhere, Category="Rock|Splash") float SplashMinSpeed = 120.f;
	// Velocidad de referencia para normalizar la intensidad del splash (a esta velocidad, intensidad ~1).
	UPROPERTY(EditAnywhere, Category="Rock|Splash") float SplashRefSpeed = 900.f;
	// Nombres de los user-parameters que el sistema Niagara debe exponer para reaccionar al impacto.
	// (Si tu Niagara no los tiene, se ignoran sin romper nada.)
	UPROPERTY(EditAnywhere, Category="Rock|Splash") FName ParamImpactSpeed    = TEXT("ImpactSpeed");
	UPROPERTY(EditAnywhere, Category="Rock|Splash") FName ParamImpactSize     = TEXT("ImpactSize");
	UPROPERTY(EditAnywhere, Category="Rock|Splash") FName ParamImpactVelocity = TEXT("ImpactVelocity");
	UPROPERTY(EditAnywhere, Category="Rock|Splash") FName ParamSurfaceNormal  = TEXT("SurfaceNormal");

	/** Hook para el BP: sumar ondas/decal/más polish en el impacto (Location en la superficie del agua). */
	UFUNCTION(BlueprintImplementableEvent, Category="Rock")
	void OnSplash(const FVector& Location, float Speed, float Size);

	UFUNCTION() void HandleEnteredWater(const FSphericalPontoon& Pontoon);
	UFUNCTION() void HandleExitedWater(const FSphericalPontoon& Pontoon);

private:
	bool bSplashed = false; // un splash por caída; se rearma al salir del agua (piedra que salta)
};
