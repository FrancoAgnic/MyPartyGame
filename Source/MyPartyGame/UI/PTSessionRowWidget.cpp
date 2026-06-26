// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSessionRowWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

bool UPTSessionRowWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (JoinButton)
        JoinButton->OnClicked.AddDynamic(this, &UPTSessionRowWidget::OnJoinClicked);

    return true;
}

void UPTSessionRowWidget::Init(const FOnlineSessionSearchResult& InResult,
    const FString& InName, int32 InCurrent, int32 InMax)
{
    StoredResult = InResult;

    if (NameText)
        NameText->SetText(FText::FromString(InName));

    if (PlayersText)
        PlayersText->SetText(FText::FromString(FString::Printf(TEXT("%d/%d"), InCurrent, InMax)));

    const bool bFull = InCurrent >= InMax;
    if (JoinButton)     JoinButton->SetIsEnabled(!bFull);
    if (JoinButtonText) JoinButtonText->SetText(FText::FromString(bFull ? TEXT("FULL") : TEXT("JOIN")));

    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();
}

void UPTSessionRowWidget::OnJoinClicked()
{
    if (Sessions)
        Sessions->JoinSession(StoredResult, TEXT(""));
}
