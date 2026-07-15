// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTControlRowWidget.h"
#include "Components/TextBlock.h"

void UPTControlRowWidget::SetRow(const FText& ActionLabel, const FText& KeyName)
{
    if (KeyText)    KeyText->SetText(KeyName);
    if (ActionText) ActionText->SetText(ActionLabel);
}
