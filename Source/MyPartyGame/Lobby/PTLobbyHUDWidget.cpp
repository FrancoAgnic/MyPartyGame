// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyHUDWidget.h"
#include "PTGameState.h"
#include "PTPlayerState.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"

void UPTLobbyHUDWidget::ShowHUD()
{
    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);

    RefreshPlayerList();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            RefreshTimerHandle, this, &UPTLobbyHUDWidget::RefreshPlayerList, 1.0f, true);
    }
}

void UPTLobbyHUDWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }
    Super::NativeDestruct();
}

void UPTLobbyHUDWidget::RefreshPlayerList()
{
    if (!PlayersBox) return;

    APTGameState* PTGS = GetWorld() ? GetWorld()->GetGameState<APTGameState>() : nullptr;
    if (!PTGS) return;

    PlayersBox->ClearChildren();

    for (APlayerState* PS : PTGS->PlayerArray)
    {
        APTPlayerState* PTPS = Cast<APTPlayerState>(PS);
        if (!PTPS) continue;

        UTextBlock* Row = NewObject<UTextBlock>(this);
        const FString Label = PTPS->bIsHost
            ? FString::Printf(TEXT("%s (Host)"), *PTPS->DisplayName)
            : PTPS->DisplayName;
        Row->SetText(FText::FromString(Label));
        PlayersBox->AddChildToVerticalBox(Row);
    }

    if (RoomCodeText)
    {
        if (PTGS->SessionCode.IsEmpty())
        {
            RoomCodeText->SetVisibility(ESlateVisibility::Collapsed);
        }
        else
        {
            RoomCodeText->SetVisibility(ESlateVisibility::Visible);
            RoomCodeText->SetText(FText::FromString(FString::Printf(TEXT("Código: %s"), *PTGS->SessionCode)));
        }
    }
}
