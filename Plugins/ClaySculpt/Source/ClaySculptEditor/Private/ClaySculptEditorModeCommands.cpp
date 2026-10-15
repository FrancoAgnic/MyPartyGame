#include "ClaySculptEditorModeCommands.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ClaySculptEditorModeCommands"

FClaySculptEditorModeCommands::FClaySculptEditorModeCommands()
	: TCommands<FClaySculptEditorModeCommands>(
		"ClaySculptEditorMode",
		NSLOCTEXT("ClaySculptEditorMode", "ClaySculptEditorModeCommands", "Clay Sculpt Editor Mode"),
		NAME_None,
		FAppStyle::GetAppStyleSetName())
{
}

void FClaySculptEditorModeCommands::RegisterCommands()
{
	UI_COMMAND(SculptTool, "Sculpt", "Esculpe el Clay Sculpt Volume bajo el cursor (click-arrastre)",
		EUserInterfaceActionType::ToggleButton, FInputChord());
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> FClaySculptEditorModeCommands::GetCommands()
{
	FClaySculptEditorModeCommands& Commands = FClaySculptEditorModeCommands::Get();
	TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> Result;
	TArray<TSharedPtr<FUICommandInfo>>& Tools = Result.FindOrAdd(TEXT("Tools"));
	Tools.Add(Commands.SculptTool);
	return Result;
}

#undef LOCTEXT_NAMESPACE
