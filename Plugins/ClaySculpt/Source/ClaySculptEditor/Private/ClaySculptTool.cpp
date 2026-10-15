#include "ClaySculptTool.h"
#include "ClaySculptVolume.h"
#include "InteractiveToolManager.h"
#include "ToolContextInterfaces.h"                  // FToolBuilderState
#include "BaseBehaviors/ClickDragBehavior.h"
#include "Engine/World.h"
#include "EngineUtils.h"                            // TActorIterator
#include "CollisionQueryParams.h"

// ─── Builder ──────────────────────────────────────────────────────────────────
bool UClaySculptToolBuilder::CanBuildTool(const FToolBuilderState& SceneState) const
{
	return true;
}

UInteractiveTool* UClaySculptToolBuilder::BuildTool(const FToolBuilderState& SceneState) const
{
	UClaySculptTool* Tool = NewObject<UClaySculptTool>(SceneState.ToolManager);
	Tool->SetWorld(SceneState.World);

	// El volumen sobre el que se esculpe: el seleccionado, o el primero del nivel.
	AClaySculptVolume* Found = nullptr;
	for (AActor* A : SceneState.SelectedActors)
		if (AClaySculptVolume* V = Cast<AClaySculptVolume>(A)) { Found = V; break; }
	if (!Found && SceneState.World)
		for (TActorIterator<AClaySculptVolume> It(SceneState.World); It; ++It) { Found = *It; break; }
	Tool->SetVolume(Found);

	return Tool;
}

// ─── Tool ─────────────────────────────────────────────────────────────────────
void UClaySculptTool::Setup()
{
	UInteractiveTool::Setup();

	Properties = NewObject<UClaySculptToolProperties>(this);
	AddToolPropertySource(Properties);

	UClickDragInputBehavior* Behavior = NewObject<UClickDragInputBehavior>(this);
	Behavior->Initialize(this);
	AddInputBehavior(Behavior);
}

FInputRayHit UClaySculptTool::CanBeginClickDragSequence(const FInputDeviceRay& PressPos)
{
	// Siempre aceptamos el arrastre (esculpimos en cualquier punto del viewport).
	return FInputRayHit(0.0f);
}

void UClaySculptTool::OnClickPress(const FInputDeviceRay& PressPos) { StampAtRay(PressPos); }
void UClaySculptTool::OnClickDrag (const FInputDeviceRay& DragPos)  { StampAtRay(DragPos); }
void UClaySculptTool::OnClickRelease(const FInputDeviceRay& /*ReleasePos*/) {}
void UClaySculptTool::OnTerminateDragSequence() {}

void UClaySculptTool::StampAtRay(const FInputDeviceRay& Ray)
{
	if (!Volume || !TargetWorld || !Properties) return;

	const FVector Origin = Ray.WorldRay.Origin;
	const FVector Dir    = Ray.WorldRay.Direction;

	// Trazar contra lo que haya (la propia malla del clay o el piso). Si no golpea nada,
	// esculpir a AirDepth para poder construir en el aire.
	FVector StampPos;
	FHitResult Hit;
	FCollisionQueryParams P(SCENE_QUERY_STAT(ClaySculptTrace), /*bTraceComplex=*/true);
	if (TargetWorld->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * 1000000.0, ECC_Visibility, P))
		StampPos = Hit.ImpactPoint;
	else
		StampPos = Origin + Dir * Properties->AirDepth;

	Volume->ApplyStamp(StampPos, Properties->Shape, Properties->BrushSize, Properties->Mode, Properties->Color);
	Volume->RebuildNow();
}
