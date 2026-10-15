#pragma once
#include "CoreMinimal.h"
#include "Tools/UEdMode.h"
#include "ClaySculptEditorMode.generated.h"

/** Modo de editor para esculpir Clay Sculpt Volumes con el mouse en el viewport. */
UCLASS()
class UClaySculptEditorMode : public UEdMode
{
	GENERATED_BODY()
public:
	const static FEditorModeID EM_ClaySculptEditorModeId;

	UClaySculptEditorMode();

	virtual void Enter() override;
	virtual void CreateToolkit() override;
	virtual TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetModeCommands() const override;
};
