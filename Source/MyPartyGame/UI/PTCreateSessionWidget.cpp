// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTCreateSessionWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/TextBlock.h"

bool UPTCreateSessionWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (ConfirmButton) ConfirmButton->OnClicked.AddDynamic(this, &UPTCreateSessionWidget::OnConfirmClicked);
    if (BackButton)    BackButton->OnClicked.AddDynamic(this, &UPTCreateSessionWidget::OnBackClicked);
    if (MinusButton)   MinusButton->OnClicked.AddDynamic(this, &UPTCreateSessionWidget::OnMinusClicked);
    if (PlusButton)    PlusButton->OnClicked.AddDynamic(this, &UPTCreateSessionWidget::OnPlusClicked);

    return true;
}

void UPTCreateSessionWidget::ShowPanel(int32 DefaultMaxPlayers)
{
    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    if (PrivateCheckbox) PrivateCheckbox->SetIsChecked(false);

    CurrentMaxPlayers = FMath::Clamp(DefaultMaxPlayers,
        UMultiplayerSessionsSubsystem::MinPlayersAllowed, UMultiplayerSessionsSubsystem::MaxPlayersAllowed);
    RefreshMaxPlayersText();

    SetVisibility(ESlateVisibility::Visible);
}

void UPTCreateSessionWidget::RefreshMaxPlayersText()
{
    if (MaxPlayersText) MaxPlayersText->SetText(FText::AsNumber(CurrentMaxPlayers));
}

void UPTCreateSessionWidget::OnMinusClicked()
{
    CurrentMaxPlayers = FMath::Max(UMultiplayerSessionsSubsystem::MinPlayersAllowed, CurrentMaxPlayers - 1);
    RefreshMaxPlayersText();
}

void UPTCreateSessionWidget::OnPlusClicked()
{
    CurrentMaxPlayers = FMath::Min(UMultiplayerSessionsSubsystem::MaxPlayersAllowed, CurrentMaxPlayers + 1);
    RefreshMaxPlayersText();
}

void UPTCreateSessionWidget::OnConfirmClicked()
{
    if (!Sessions) return;

    const bool bPrivate = PrivateCheckbox && PrivateCheckbox->IsChecked();
    Sessions->CreateSession(CurrentMaxPlayers, bPrivate);
}

void UPTCreateSessionWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
