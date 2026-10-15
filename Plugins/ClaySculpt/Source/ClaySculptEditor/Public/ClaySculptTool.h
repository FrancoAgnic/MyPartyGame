#pragma once
#include "CoreMinimal.h"
#include "InteractiveTool.h"
#include "InteractiveToolBuilder.h"
#include "BaseBehaviors/BehaviorTargetInterfaces.h" // IClickDragBehaviorTarget
#include "InputState.h"                              // FInputDeviceRay / FInputRayHit
#include "ClaySculptVolume.h"                        // EClayStampShape / EClayEditMode
#include "ClaySculptTool.generated.h"

class AClaySculptVolume;

UCLASS()
class UClaySculptToolBuilder : public UInteractiveToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UInteractiveTool* BuildTool(const FToolBuilderState& SceneState) const override;
};

/** Parámetros de la brocha (aparecen en el panel del modo). */
UCLASS()
class UClaySculptToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, Category="Brush", meta=(ClampMin="10.0", ClampMax="2000.0"))
	float BrushSize = 160.f;

	UPROPERTY(EditAnywhere, Category="Brush")
	EClayStampShape Shape = EClayStampShape::Sphere;

	UPROPERTY(EditAnywhere, Category="Brush")
	EClayEditMode Mode = EClayEditMode::Add;

	UPROPERTY(EditAnywhere, Category="Brush")
	FLinearColor Color = FLinearColor(0.8f, 0.6f, 0.5f, 1.f);

	// Distancia del sello cuando el rayo no golpea nada (para construir en el aire).
	UPROPERTY(EditAnywhere, Category="Brush", meta=(ClampMin="50.0", ClampMax="8000.0"))
	float AirDepth = 400.f;
};

UCLASS()
class UClaySculptTool : public UInteractiveTool, public IClickDragBehaviorTarget
{
	GENERATED_BODY()
public:
	virtual void Setup() override;

	void SetWorld(UWorld* World) { TargetWorld = World; }
	void SetVolume(AClaySculptVolume* V) { Volume = V; }

	// IClickDragBehaviorTarget
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;
	virtual void OnClickPress(const FInputDeviceRay& PressPos) override;
	virtual void OnClickDrag(const FInputDeviceRay& DragPos) override;
	virtual void OnClickRelease(const FInputDeviceRay& ReleasePos) override;
	virtual void OnTerminateDragSequence() override;

protected:
	UPROPERTY() TObjectPtr<UClaySculptToolProperties> Properties;
	UPROPERTY() TObjectPtr<UWorld> TargetWorld;
	UPROPERTY() TObjectPtr<AClaySculptVolume> Volume;

	void StampAtRay(const FInputDeviceRay& Ray);
};
