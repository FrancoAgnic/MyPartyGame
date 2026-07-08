#include "PTNameTagWidget.h"
#include "Components/TextBlock.h"

void UPTNameTagWidget::SetPlayerName(const FString& InName)
{
    if (NameText) NameText->SetText(FText::FromString(InName.Left(10))); // máximo 10 caracteres
}

void UPTNameTagWidget::ShowMessage(const FString& Msg)
{
    if (NameText) NameText->SetText(FText::FromString(Msg.Left(40))); // globo de chat (tope 40)
}
