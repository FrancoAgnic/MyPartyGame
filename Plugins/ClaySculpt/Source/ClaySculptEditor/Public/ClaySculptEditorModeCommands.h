#pragma once
#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"

class FClaySculptEditorModeCommands : public TCommands<FClaySculptEditorModeCommands>
{
public:
	FClaySculptEditorModeCommands();

	virtual void RegisterCommands() override;
	static TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> GetCommands();

	TSharedPtr<FUICommandInfo> SculptTool;
};
