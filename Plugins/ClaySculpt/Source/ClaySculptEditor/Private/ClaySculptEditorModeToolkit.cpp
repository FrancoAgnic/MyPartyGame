#include "ClaySculptEditorModeToolkit.h"
#include "ClaySculptEditorMode.h"

#define LOCTEXT_NAMESPACE "ClaySculptEditorModeToolkit"

FClaySculptEditorModeToolkit::FClaySculptEditorModeToolkit()
{
}

void FClaySculptEditorModeToolkit::Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode)
{
	FModeToolkit::Init(InitToolkitHost, InOwningMode);
}

FName FClaySculptEditorModeToolkit::GetToolkitFName() const
{
	return FName("ClaySculptEditorMode");
}

FText FClaySculptEditorModeToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("DisplayName", "Clay Sculpt");
}

void FClaySculptEditorModeToolkit::GetToolPaletteNames(TArray<FName>& PaletteNames) const
{
	PaletteNames.Add(TEXT("Tools"));
}

#undef LOCTEXT_NAMESPACE
