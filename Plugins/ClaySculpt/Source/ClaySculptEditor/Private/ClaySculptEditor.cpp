#include "ClaySculptEditor.h"
#include "ClaySculptEditorModeCommands.h"

#define LOCTEXT_NAMESPACE "ClaySculptEditor"

void FClaySculptEditorModule::StartupModule()
{
	FClaySculptEditorModeCommands::Register();
}

void FClaySculptEditorModule::ShutdownModule()
{
	FClaySculptEditorModeCommands::Unregister();
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FClaySculptEditorModule, ClaySculptEditor)
