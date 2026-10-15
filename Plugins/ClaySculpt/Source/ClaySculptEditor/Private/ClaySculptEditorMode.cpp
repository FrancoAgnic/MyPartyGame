#include "ClaySculptEditorMode.h"
#include "ClaySculptEditorModeToolkit.h"
#include "ClaySculptEditorModeCommands.h"
#include "ClaySculptTool.h"
#include "Tools/EdModeInteractiveToolsContext.h"
#include "InteractiveToolManager.h"

#define LOCTEXT_NAMESPACE "ClaySculptEditorMode"

const FEditorModeID UClaySculptEditorMode::EM_ClaySculptEditorModeId = TEXT("EM_ClaySculptEditorMode");

UClaySculptEditorMode::UClaySculptEditorMode()
{
	Info = FEditorModeInfo(
		EM_ClaySculptEditorModeId,
		LOCTEXT("ClaySculptEditorModeName", "Clay Sculpt"),
		FSlateIcon(),
		true);
}

void UClaySculptEditorMode::Enter()
{
	UEdMode::Enter();

	const FClaySculptEditorModeCommands& Commands = FClaySculptEditorModeCommands::Get();
	RegisterTool(Commands.SculptTool, TEXT("ClaySculptTool"), NewObject<UClaySculptToolBuilder>(this));

	// Activar la tool de esculpido por defecto al entrar al modo.
	GetInteractiveToolsContext()->StartTool(TEXT("ClaySculptTool"));
}

void UClaySculptEditorMode::CreateToolkit()
{
	Toolkit = MakeShareable(new FClaySculptEditorModeToolkit);
}

TMap<FName, TArray<TSharedPtr<FUICommandInfo>>> UClaySculptEditorMode::GetModeCommands() const
{
	return FClaySculptEditorModeCommands::GetCommands();
}

#undef LOCTEXT_NAMESPACE
