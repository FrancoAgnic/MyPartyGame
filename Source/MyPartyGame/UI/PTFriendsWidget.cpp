// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTFriendsWidget.h"
#include "PTFriendRowWidget.h"
#include "../PTTextTable.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/PanelWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UPTFriendsWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    if (RefreshButton)     RefreshButton->OnClicked.AddDynamic(this, &UPTFriendsWidget::OnRefreshClicked);
    if (SteamInviteButton) SteamInviteButton->OnClicked.AddDynamic(this, &UPTFriendsWidget::OnSteamInviteClicked);
    if (BackButton)        BackButton->OnClicked.AddDynamic(this, &UPTFriendsWidget::OnBackClicked);

    if (TitleText) TitleText->SetText(PTText::Get(TEXT("FRIENDS_TITLE")));

    if (Sessions && !bBound)
    {
        Sessions->OnFriendsListUpdated.AddUObject(this, &UPTFriendsWidget::OnFriendsListUpdated);
        Sessions->OnInviteWarning.AddUObject(this, &UPTFriendsWidget::OnInviteWarning);
        bBound = true;
    }
    if (StatusText) StatusText->SetVisibility(ESlateVisibility::Collapsed);

    // Leer la lista al abrir. Si aún no hay datos, Rebuild deja el panel vacío hasta el callback.
    if (Sessions) Sessions->ReadFriends();
    Rebuild();
}

void UPTFriendsWidget::NativeDestruct()
{
    if (Sessions && bBound)
    {
        Sessions->OnFriendsListUpdated.RemoveAll(this);
        Sessions->OnInviteWarning.RemoveAll(this);
        bBound = false;
    }
    Super::NativeDestruct();
}

void UPTFriendsWidget::ShowPanel()
{
    SetVisibility(ESlateVisibility::Visible);
    PlayPopIn();
    if (Sessions) Sessions->ReadFriends();
}

void UPTFriendsWidget::OnRefreshClicked()
{
    if (Sessions) Sessions->ReadFriends();
}

void UPTFriendsWidget::OnSteamInviteClicked()
{
    if (Sessions) Sessions->ShowSteamInviteOverlay();
}

void UPTFriendsWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTFriendsWidget::OnFriendsListUpdated()
{
    Rebuild();
}

void UPTFriendsWidget::OnInviteWarning(const FString& MsgKey)
{
    if (!StatusText) return;
    StatusText->SetText(PTText::Get(FName(*MsgKey)));
    StatusText->SetVisibility(ESlateVisibility::Visible);
}

void UPTFriendsWidget::Rebuild()
{
    if (!FriendsBox) return;
    FriendsBox->ClearChildren();

    static const TArray<FPTFriendInfo> EmptyFriends;
    const TArray<FPTFriendInfo>& Friends = Sessions ? Sessions->GetFriends() : EmptyFriends;

    int32 Shown = 0;
    if (RowWidgetClass)
    {
        for (const FPTFriendInfo& F : Friends)
        {
            // Ocultar desconectados: la lista se enfoca en a quién SÍ podés invitar/seguir.
            if (!F.bOnline) continue;

            UPTFriendRowWidget* Row = CreateWidget<UPTFriendRowWidget>(this, RowWidgetClass);
            if (!Row) continue;
            Row->Init(F);
            FriendsBox->AddChild(Row);
            ++Shown;
        }
    }

    if (EmptyText)
    {
        EmptyText->SetText(PTText::Get(TEXT("FRIENDS_EMPTY")));
        EmptyText->SetVisibility(Shown == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }
}
