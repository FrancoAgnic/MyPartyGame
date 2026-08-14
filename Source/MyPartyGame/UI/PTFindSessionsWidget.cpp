// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTFindSessionsWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "PTSessionRowWidget.h"
#include "../PTGameInstance.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"

bool UPTFindSessionsWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (RefreshButton)   RefreshButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnRefreshClicked);
    if (BackButton)      BackButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnBackClicked);
    if (ReconnectButton) ReconnectButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnReconnectClicked);

    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    return true;
}

void UPTFindSessionsWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    // "Reconectar a la partida anterior": solo si hay una última partida guardada (pública o privada).
    if (ReconnectButton)
    {
        const UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance());
        ReconnectButton->SetVisibility((GI && GI->HasLastGame())
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}

void UPTFindSessionsWidget::OnReconnectClicked()
{
    if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
        GI->ReconnectToLastGame();
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
        // Fase 5 — esta lista es solo para partidas públicas; las privadas se unen por código.
        if (UMultiplayerSessionsSubsystem::GetHasPasswordFromResult(Result)) continue;

        UPTSessionRowWidget* Row = CreateWidget<UPTSessionRowWidget>(this, RowWidgetClass);
        if (!Row) continue;

        const FString Name = UMultiplayerSessionsSubsystem::GetServerNameFromResult(Result);
        const int32 Open   = Result.Session.NumOpenPublicConnections;
        const int32 Max    = Result.Session.SessionSettings.NumPublicConnections;
        // Jugadores actuales: preferir el nº anunciado por el host (clave CUR_PLAYERS, se actualiza al
        // entrar/salir). Si el resultado no lo trae, caer al cálculo por conexiones abiertas.
        const int32 Advertised = UMultiplayerSessionsSubsystem::GetCurrentPlayersFromResult(Result);
        const int32 Current    = (Advertised >= 0) ? Advertised : (Max - Open);

        Row->Init(Result, Name, Current, Max);
        ResultsBox->AddChild(Row);
    }
}
