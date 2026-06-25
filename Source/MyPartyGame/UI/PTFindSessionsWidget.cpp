// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTFindSessionsWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "PTSessionRowWidget.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"

bool UPTFindSessionsWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (RefreshButton)      RefreshButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnRefreshClicked);
    if (BackButton)         BackButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnBackClicked);
    if (JoinByCodeButton)   JoinByCodeButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnJoinByCodeClicked);

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

void UPTFindSessionsWidget::OnJoinByCodeClicked()
{
    if (!Sessions || !CodeInput) return;
    Sessions->JoinSessionByCode(CodeInput->GetText().ToString());
}

void UPTFindSessionsWidget::PopulateResults(
    const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful)
{
    if (!ResultsBox || !RowWidgetClass) return;

    ResultsBox->ClearChildren();

    for (const FOnlineSessionSearchResult& Result : Results)
    {
        // Fase 5 — esta lista es solo para partidas públicas; las privadas se unen por código.
        if (UMultiplayerSessionsSubsystem::GetHasPasswordFromResult(Result)) continue;

        UPTSessionRowWidget* Row = CreateWidget<UPTSessionRowWidget>(this, RowWidgetClass);
        if (!Row) continue;

        const FString Name = UMultiplayerSessionsSubsystem::GetServerNameFromResult(Result);
        const int32 Open   = Result.Session.NumOpenPublicConnections;
        const int32 Max    = Result.Session.SessionSettings.NumPublicConnections;

        Row->Init(Result, Name, Max - Open, Max, /*bInHasPassword=*/false);
        ResultsBox->AddChild(Row);
    }
}
