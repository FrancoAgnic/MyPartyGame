#include "PTNameTagWidget.h"
#include "Components/TextBlock.h"

bool UPTNameTagWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (NameText) DefaultColor = NameText->GetColorAndOpacity().GetSpecifiedColor();
    return true;
}

void UPTNameTagWidget::SetPlayerName(const FString& InName)
{
    if (!NameText) return;
    NameText->SetText(FText::FromString(InName.Left(10))); // máximo 10 caracteres
    NameText->SetColorAndOpacity(FSlateColor(DefaultColor));
}

void UPTNameTagWidget::ShowMessage(const FString& Msg)
{
    if (!NameText) return;
    NameText->SetText(FText::FromString(Msg.Left(40))); // globo de chat (tope 40)
    NameText->SetColorAndOpacity(FSlateColor(DefaultColor));
}

void UPTNameTagWidget::ShowGuessMessage(const FString& Msg)
{
    if (!NameText) return;
    NameText->SetText(FText::FromString(Msg.Left(40)));
    NameText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
}
