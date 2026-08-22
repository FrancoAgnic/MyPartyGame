// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTFindSessionsWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "PTSessionRowWidget.h"
#include "../PTGameInstance.h"
#include "../PTTextTable.h"
#include "Components/ScrollBox.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

bool UPTFindSessionsWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (RefreshButton)    RefreshButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnRefreshClicked);
    if (BackButton)       BackButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnBackClicked);
    if (ReconnectButton)  ReconnectButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnReconnectClicked);
    if (PublicTabButton)  PublicTabButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnPublicTabClicked);
    if (FriendsTabButton) FriendsTabButton->OnClicked.AddDynamic(this, &UPTFindSessionsWidget::OnFriendsTabClicked);

    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    return true;
}

void UPTFindSessionsWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    PlayPopIn();

    // "Reconectar a la partida anterior": solo si hay una última partida guardada.
    if (ReconnectButton)
    {
        const UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance());
        ReconnectButton->SetVisibility((GI && GI->HasLastGame())
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    SwitchTab(ActiveTab);
    RefreshList();
}

void UPTFindSessionsWidget::RefreshList()
{
    if (!Sessions) return;
    // Leer amigos primero (para poder filtrar la pestaña Amigos por dueño) y luego buscar sesiones.
    Sessions->ReadFriends();
    Sessions->FindSessions(20);
}

void UPTFindSessionsWidget::OnRefreshClicked() { RefreshList(); }

void UPTFindSessionsWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTFindSessionsWidget::OnReconnectClicked()
{
    if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
        GI->ReconnectToLastGame();
}

void UPTFindSessionsWidget::OnPublicTabClicked()  { SwitchTab(0); }
void UPTFindSessionsWidget::OnFriendsTabClicked() { SwitchTab(1); }

void UPTFindSessionsWidget::SwitchTab(int32 Tab)
{
    ActiveTab = Tab;
    // Mostrar la lista de la pestaña activa (si el WBP no tiene FriendsBox, siempre queda la pública).
    if (ResultsBox) ResultsBox->SetVisibility(ActiveTab == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (FriendsBox) FriendsBox->SetVisibility(ActiveTab == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ApplyTabVisual();
    UpdateEmptyLabels();
}

void UPTFindSessionsWidget::ApplyTabVisual()
{
    // Doble señal para que SIEMPRE se note cuál está activa, sin depender del estilo del botón:
    // (1) color de fondo y (2) opacidad (la inactiva se atenúa).
    const bool bPublicActive = (ActiveTab == 0);
    if (PublicTabButton)
    {
        PublicTabButton->SetBackgroundColor(bPublicActive ? TabActiveColor : TabInactiveColor);
        PublicTabButton->SetRenderOpacity(bPublicActive ? 1.0f : 0.45f);
    }
    if (FriendsTabButton)
    {
        FriendsTabButton->SetBackgroundColor(!bPublicActive ? TabActiveColor : TabInactiveColor);
        FriendsTabButton->SetRenderOpacity(!bPublicActive ? 1.0f : 0.45f);
    }
}

void UPTFindSessionsWidget::UpdateEmptyLabels()
{
    // Cada cartel se ve solo si SU pestaña está activa Y no tiene partidas.
    if (EmptyPublicText)
        EmptyPublicText->SetVisibility((ActiveTab == 0 && LastPublicCount == 0)
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (EmptyFriendsText)
        EmptyFriendsText->SetVisibility((ActiveTab == 1 && LastFriendsCount == 0)
            ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UPTFindSessionsWidget::PopulateResults(
    const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful)
{
    if (!RowWidgetClass) return;

    if (ResultsBox) ResultsBox->ClearChildren();
    if (FriendsBox) FriendsBox->ClearChildren();

    int32 PublicCount = 0;
    int32 FriendsCount = 0;

    for (const FOnlineSessionSearchResult& Result : Results)
    {
        const bool bFriendsOnly = UMultiplayerSessionsSubsystem::GetIsFriendsOnlyFromResult(Result);

        // Elegir la lista destino:
        //  - Pública      → pestaña Públicas (ResultsBox).
        //  - Solo amigos  → pestaña Amigos (FriendsBox), SOLO si el dueño es amigo nuestro.
        UScrollBox* TargetBox = nullptr;
        if (!bFriendsOnly)
        {
            TargetBox = ResultsBox;
        }
        else if (Sessions)
        {
            const FString OwnerId = UMultiplayerSessionsSubsystem::GetOwnerSteamIdFromResult(Result);
            if (Sessions->IsFriendSteamId(OwnerId))
                TargetBox = FriendsBox ? FriendsBox : ResultsBox;
            // privada solo amigos de un NO-amigo → no se muestra en ningún lado.
        }
        if (!TargetBox) continue;

        UPTSessionRowWidget* Row = CreateWidget<UPTSessionRowWidget>(this, RowWidgetClass);
        if (!Row) continue;

        const FString Name = UMultiplayerSessionsSubsystem::GetServerNameFromResult(Result);
        const int32 Open   = Result.Session.NumOpenPublicConnections;
        const int32 Max    = Result.Session.SessionSettings.NumPublicConnections;
        const int32 Advertised = UMultiplayerSessionsSubsystem::GetCurrentPlayersFromResult(Result);
        const int32 Current    = (Advertised >= 0) ? Advertised : (Max - Open);

        Row->Init(Result, Name, Current, Max);
        TargetBox->AddChild(Row);

        if (TargetBox == FriendsBox) ++FriendsCount; else ++PublicCount;
    }

    LastPublicCount  = PublicCount;
    LastFriendsCount = FriendsCount;

    if (EmptyPublicText)  EmptyPublicText->SetText(PTText::Get(TEXT("FIND_SESSIONS_EMPTY")));
    if (EmptyFriendsText) EmptyFriendsText->SetText(PTText::Get(TEXT("FIND_FRIENDS_EMPTY")));
    UpdateEmptyLabels();
}
