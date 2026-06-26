// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTEnterCodeWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"

bool UPTEnterCodeWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (JoinButton) JoinButton->OnClicked.AddDynamic(this, &UPTEnterCodeWidget::OnJoinClicked);
    if (BackButton) BackButton->OnClicked.AddDynamic(this, &UPTEnterCodeWidget::OnBackClicked);

    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    return true;
}

void UPTEnterCodeWidget::ShowPanel()
{
    if (CodeInput) CodeInput->SetText(FText::GetEmpty());
    SetVisibility(ESlateVisibility::Visible);
}

void UPTEnterCodeWidget::OnJoinClicked()
{
    if (!Sessions || !CodeInput) return;
    Sessions->JoinSessionByCode(CodeInput->GetText().ToString());
}

void UPTEnterCodeWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
