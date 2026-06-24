// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTFindSessionsWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "PTSessionRowWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"

bool UPTFindSessionsWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (RefreshButton) RefreshButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnRefreshClicked);
    if (BackButton)    BackButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnBackClicked);

    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    return true;
}

void UPTFindSessionsWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
}

void UPTFindSessionsWidget::OnRefreshClicked()
{
    if (Sessions) Sessions->FindSessions(20);
}

void UPTFindSessionsWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTFindSessionsWidget::PopulateResults(
    const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful)
{
    if (!ResultsBox || !RowWidgetClass) return;

    ResultsBox->ClearChildren();

    for (const FOnlineSessionSearchResult& Result : Results)
    {
        UPTSessionRowWidget* Row = CreateWidget<UPTSessionRowWidget>(this, RowWidgetClass);
        if (!Row) continue;

        const FString Name  = UMultiplayerSessionsSubsystem::GetServerNameFromResult(Result);
        const bool bHasPass = UMultiplayerSessionsSubsystem::GetHasPasswordFromResult(Result);
        const int32 Open    = Result.Session.NumOpenPublicConnections;
        const int32 Max     = Result.Session.SessionSettings.NumPublicConnections;

        Row->Init(Result, Name, Max - Open, Max, bHasPass);
        ResultsBox->AddChild(Row);
    }
}
