// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTOffscreenChatWidget.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"

void UPTOffscreenChatWidget::SetBubble(const FString& Text, bool bGuess, float ArrowAngleDeg)
{
    if (TxtMessage) TxtMessage->SetText(FText::FromString(Text));
    if (Arrow)      Arrow->SetRenderTransformAngle(ArrowAngleDeg); // apunta hacia el jugador
    OnBubbleSet(bGuess);
}
