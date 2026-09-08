// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTChatShoutWidget.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"

bool UPTChatShoutWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (ShoutText) DefaultTextColor = ShoutText->GetColorAndOpacity().GetSpecifiedColor();
    return true;
}

float UPTChatShoutWidget::ShowShout(const FString& Text, bool bGuess)
{
    if (ShoutText)
    {
        ShoutText->SetText(FText::FromString(Text));
        ShoutText->SetColorAndOpacity(FSlateColor(bGuess ? GuessColor : DefaultTextColor));
    }

    if (PopAnim)
    {
        PlayAnimation(PopAnim);                       // tu animación define escala/hold/fade
        return FMath::Max(0.1f, (float)PopAnim->GetEndTime());
    }
    return FMath::Max(0.1f, FallbackSeconds);         // sin animación: se muestra fijo unos segundos
}
