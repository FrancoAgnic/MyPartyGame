#include "PTNameTagWidget.h"
#include "Components/TextBlock.h"

void UPTNameTagWidget::SetPlayerName(const FString& InName)
{
    if (NameText) NameText->SetText(FText::FromString(InName.Left(10))); // máximo 10 caracteres
}
