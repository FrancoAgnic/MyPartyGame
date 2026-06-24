// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTCreateSessionWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/SpinBox.h"

bool UPTCreateSessionWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (ConfirmButton) ConfirmButton->OnClicked.AddDynamic(this, &UPTCreateSessionWidget::OnConfirmClicked);
    if (BackButton)    BackButton->OnClicked.AddDynamic(this, &UPTCreateSessionWidget::OnBackClicked);
    return true;
}

void UPTCreateSessionWidget::ShowPanel(int32 DefaultMaxPlayers)
{
    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    if (NameInput)       NameInput->SetText(FText::GetEmpty());
    if (PasswordInput)   PasswordInput->SetText(FText::GetEmpty());
    if (MaxPlayersInput) MaxPlayersInput->SetValue(static_cast<float>(DefaultMaxPlayers));

    SetVisibility(ESlateVisibility::Visible);
}

void UPTCreateSessionWidget::OnConfirmClicked()
{
    if (!Sessions) return;

    const FString Name = NameInput     ? NameInput->GetText().ToString()                     : TEXT("Room");
    const FString Pass = PasswordInput ? PasswordInput->GetText().ToString()                 : TEXT("");
    const int32   Max  = MaxPlayersInput ? FMath::RoundToInt(MaxPlayersInput->GetValue())    : 4;

    Sessions->CreateSession(Max, Name, Pass);
}

void UPTCreateSessionWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
