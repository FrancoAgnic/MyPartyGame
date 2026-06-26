// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTCreateSessionWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/Button.h"
#include "Components/SpinBox.h"
#include "Components/CheckBox.h"

bool UPTCreateSessionWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (ConfirmButton) ConfirmButton->OnClicked.AddDynamic(this, &UPTCreateSessionWidget::OnConfirmClicked);
    if (BackButton)    BackButton->OnClicked.AddDynamic(this, &UPTCreateSessionWidget::OnBackClicked);

    if (MaxPlayersInput)
    {
        MaxPlayersInput->SetMinValue(1.0f);
        MaxPlayersInput->SetMaxValue(static_cast<float>(UMultiplayerSessionsSubsystem::MaxPlayersAllowed));
    }

    return true;
}

void UPTCreateSessionWidget::ShowPanel(int32 DefaultMaxPlayers)
{
    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    if (PrivateCheckbox) PrivateCheckbox->SetIsChecked(false);
    if (MaxPlayersInput) MaxPlayersInput->SetValue(static_cast<float>(DefaultMaxPlayers));

    SetVisibility(ESlateVisibility::Visible);
}

void UPTCreateSessionWidget::OnConfirmClicked()
{
    if (!Sessions) return;

    const bool  bPrivate = PrivateCheckbox && PrivateCheckbox->IsChecked();
    const int32 Max      = MaxPlayersInput  ? FMath::RoundToInt(MaxPlayersInput->GetValue()) : 4;

    Sessions->CreateSession(Max, bPrivate);
}

void UPTCreateSessionWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
