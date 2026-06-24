// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSessionRowWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "PTPasswordPromptWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"

bool UPTSessionRowWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (JoinButton)
        JoinButton->OnClicked.AddDynamic(this, &UPTSessionRowWidget::OnJoinClicked);

    if (PasswordPrompt)
        PasswordPrompt->OnPasswordConfirmed.AddDynamic(
            this, &UPTSessionRowWidget::OnPasswordConfirmedCallback);

    return true;
}

void UPTSessionRowWidget::Init(const FOnlineSessionSearchResult& InResult,
    const FString& InName, int32 InCurrent, int32 InMax, bool bInHasPassword)
{
    StoredResult = InResult;
    bHasPassword = bInHasPassword;

    if (NameText)
        NameText->SetText(FText::FromString(InName));

    if (PlayersText)
        PlayersText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), InCurrent, InMax)));

    if (LockIcon)
        LockIcon->SetVisibility(bHasPassword ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();
}

void UPTSessionRowWidget::OnJoinClicked()
{
    if (bHasPassword && PasswordPrompt)
    {
        PasswordPrompt->ShowPrompt();
    }
    else if (Sessions)
    {
        Sessions->JoinSession(StoredResult, TEXT(""));
    }
}

void UPTSessionRowWidget::OnPasswordConfirmedCallback(const FString& Password)
{
    if (Sessions)
        Sessions->JoinSession(StoredResult, Password);
}
