// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTPasswordPromptWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"

bool UPTPasswordPromptWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (ConfirmButton) ConfirmButton->OnClicked.AddDynamic(this, &UPTPasswordPromptWidget::OnConfirmClicked);
    if (CancelButton)  CancelButton->OnClicked.AddDynamic(this, &UPTPasswordPromptWidget::OnCancelClicked);
    return true;
}

void UPTPasswordPromptWidget::ShowPrompt()
{
    if (PasswordInput) PasswordInput->SetText(FText::GetEmpty());
    SetVisibility(ESlateVisibility::Visible);
}

void UPTPasswordPromptWidget::OnConfirmClicked()
{
    const FString Pass = PasswordInput ? PasswordInput->GetText().ToString() : TEXT("");
    SetVisibility(ESlateVisibility::Collapsed);
    OnPasswordConfirmed.Broadcast(Pass);
}

void UPTPasswordPromptWidget::OnCancelClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
