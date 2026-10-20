// Stone Splash — simulación de agua interactiva en GPU (enfoque C, custom).
// Un campo de altura simulado con la ECUACIÓN DE ONDA, calculado en Render Targets (ping-pong).
// Los objetos (la piedra) inyectan "perturbaciones" (AddDisturbance) → nacen ondas que se propagan y
// rebotan. El material de la superficie lee la altura (GetHeightRT) para desplazar la malla + normales.
//
// Materiales que hay que autorear (ver recetas que te paso):
//  - SimMaterial  (post-process-like, lee TexPrev/TexCurr/TexForce + WaveC2/Damping → nueva altura en R)
//  - BrushMaterial(aditivo, pinta un pulso radial en TexForce: params CenterUV, RadiusUV, Strength)
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaterSim.generated.h"

class UTextureRenderTarget2D;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;
class UWaterInteractorComponent;

UCLASS()
class MYPARTYGAME_API AWaterSim : public AActor
{
	GENERATED_BODY()
public:
	AWaterSim();
	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Inyecta una perturbación en la superficie en una posición de MUNDO (la piedra la llama al tocar
	 *  el agua y mientras se hunde). Strength = alto/hondo del pulso; Radius = radio en cm. */
	UFUNCTION(BlueprintCallable, Category="WaterSim")
	void AddDisturbance(const FVector& WorldPos, float Strength, float Radius);

	/** Altura simulada (en cm, relativa a la superficie en reposo) en una posición de mundo. Para flotar
	 *  la piedra sobre la ola. OJO: lee del GPU (readback), usar con moderación (pocas muestras/frame). */
	UFUNCTION(BlueprintCallable, Category="WaterSim")
	float GetHeightAtWorld(const FVector& WorldPos) const;

	/** El Render Target con la altura ACTUAL (el material de la superficie lo muestrea). */
	UFUNCTION(BlueprintCallable, Category="WaterSim")
	UTextureRenderTarget2D* GetHeightRT() const { return RTCurr; }

	/** Mapea una posición de mundo (XY) a UV [0..1] del dominio de simulación. */
	UFUNCTION(BlueprintCallable, Category="WaterSim")
	FVector2D WorldToUV(const FVector& WorldPos) const;

	/** Interacción SELECTIVA: solo los objetos con un UWaterInteractorComponent generan ondas. */
	void RegisterInteractor(UWaterInteractorComponent* C);
	void UnregisterInteractor(UWaterInteractorComponent* C);

protected:
	virtual void BeginPlay() override;

	// Dominio: cuadrado centrado en el actor, de SimWorldSize cm de lado, a resolución SimResolution px.
	UPROPERTY(EditAnywhere, Category="WaterSim") float SimWorldSize = 6000.f;
	UPROPERTY(EditAnywhere, Category="WaterSim") int32 SimResolution = 512;
	// Velocidad de onda al cuadrado (c²). Estable ~0.1..0.25. Más alto = ondas más rápidas (puede explotar).
	UPROPERTY(EditAnywhere, Category="WaterSim") float WaveC2 = 0.2f;
	// Amortiguación por paso (0.9..0.999). Más bajo = las ondas se calman antes.
	UPROPERTY(EditAnywhere, Category="WaterSim") float Damping = 0.996f;
	// Pasos de simulación por frame (más = ondas más vivas, más costo).
	UPROPERTY(EditAnywhere, Category="WaterSim") int32 StepsPerFrame = 1;

	UPROPERTY(EditAnywhere, Category="WaterSim|Materials") UMaterialInterface* SimMaterial = nullptr;
	UPROPERTY(EditAnywhere, Category="WaterSim|Materials") UMaterialInterface* BrushMaterial = nullptr;
	// Material del plano DEBUG: samplea el RT de altura (param "HeightTex") y lo muestra en gris.
	UPROPERTY(EditAnywhere, Category="WaterSim|Materials") UMaterialInterface* DebugMaterial = nullptr;
	UPROPERTY(EditAnywhere, Category="WaterSim") float DebugHeightScale = 2.f;

	// DEBUG: plano opcional que muestra la altura (gris) para validar la sim antes del material lindo.
	UPROPERTY(VisibleAnywhere, Category="WaterSim") UStaticMeshComponent* DebugPlane = nullptr;
	UPROPERTY(EditAnywhere, Category="WaterSim") bool bShowDebugPlane = true;
	UPROPERTY(Transient) UMaterialInstanceDynamic* DebugMID = nullptr;

	// DEBUG: suelta "gotas" al azar cada DebugRippleInterval seg para validar la sim (ondas que se
	// propagan y rebotan) sin necesitar la piedra todavía. Apagar cuando conectemos la piedra (C3).
	UPROPERTY(EditAnywhere, Category="WaterSim|Debug") bool bDebugAutoRipple = false;
	UPROPERTY(EditAnywhere, Category="WaterSim|Debug") float DebugRippleInterval = 0.8f;
	UPROPERTY(EditAnywhere, Category="WaterSim|Debug") float DebugRippleStrength = 6.f;
	UPROPERTY(EditAnywhere, Category="WaterSim|Debug") float DebugRippleRadius = 150.f;
	// Goteo CONSTANTE en el centro (cada frame) para aislar el problema: si la fuerza se pinta pero la
	// altura sigue 0 → falla el SimMaterial; si la fuerza también es 0 → falla el BrushMaterial.
	UPROPERTY(EditAnywhere, Category="WaterSim|Debug") bool bDebugConstantDrip = false;

private:
	// Ping-pong de altura: Prev (t-1), Curr (t), Next (t+1). Rotan cada paso.
	UPROPERTY(Transient) UTextureRenderTarget2D* RTPrev = nullptr;
	UPROPERTY(Transient) UTextureRenderTarget2D* RTCurr = nullptr;
	UPROPERTY(Transient) UTextureRenderTarget2D* RTNext = nullptr;
	// Fuerzas/perturbaciones del frame (se acumulan y se limpian cada step).
	UPROPERTY(Transient) UTextureRenderTarget2D* RTForce = nullptr;

	UPROPERTY(Transient) UMaterialInstanceDynamic* SimMID = nullptr;
	UPROPERTY(Transient) UMaterialInstanceDynamic* BrushMID = nullptr;

	struct FPendingDisturb { FVector2D UV; float Strength; float RadiusUV; };
	TArray<FPendingDisturb> Pending;

	float DebugRippleAccum = 0.f;
	float DebugReadAccum = 0.f;

	// Objetos registrados que generan ondas (interacción selectiva) + su posición del frame anterior (velocidad).
	TArray<TWeakObjectPtr<UWaterInteractorComponent>> Interactors;
	TMap<TWeakObjectPtr<UWaterInteractorComponent>, FVector> LastInteractorPos;
	void ProcessInteractors(float Dt);

	void InitRTs();
	void SimStep();
};
