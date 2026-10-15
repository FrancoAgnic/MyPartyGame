#pragma once
#include "CoreMinimal.h"
#include "Toolkits/BaseToolkit.h"

class FClaySculptEditorModeToolkit : public FModeToolkit
{
public:
	FClaySculptEditorModeToolkit();

	virtual void Init(const TSharedPtr<IToolkitHost>& InitToolkitHost, TWeakObjectPtr<UEdMode> InOwningMode) override;

	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual void GetToolPaletteNames(TArray<FName>& PaletteNames) const override;
};
