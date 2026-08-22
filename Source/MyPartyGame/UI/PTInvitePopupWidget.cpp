// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTInvitePopupWidget.h"
#include "../PTTextTable.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"

bool UPTInvitePopupWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (AcceptButton) AcceptButton->OnClicked.AddDynamic(this, &UPTInvitePopupWidget::OnAcceptClicked);
    if (RejectButton) RejectButton->OnClicked.AddDynamic(this, &UPTInvitePopupWidget::OnRejectClicked);
    return true;
}

UMultiplayerSessionsSubsystem* UPTInvitePopupWidget::Sessions() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr;
}

void UPTInvitePopupWidget::Setup(const FString& FromName, float TimeoutSeconds)
{
    if (MessageText)
    {
        FFormatOrderedArguments Args;
        Args.Add(FText::FromString(FromName));
        MessageText->SetText(PTText::Format(TEXT("INVITE_POPUP_MSG"), Args));
    }
    TimeTotal = FMath::Max(1.f, TimeoutSeconds);
    TimeLeft  = TimeTotal;
    if (TimeBar) TimeBar->SetPercent(1.f);

    PlayPopIn(); // aparece con el "blop"
}

void UPTInvitePopupWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bClosed) return;

    TimeLeft -= InDeltaTime;
    if (TimeBar) TimeBar->SetPercent(FMath::Clamp(TimeLeft / TimeTotal, 0.f, 1.f));

    if (TimeLeft <= 0.f)
    {
        // Se acabó el tiempo = rechazar (sin unir) y cerrar.
        if (UMultiplayerSessionsSubsystem* S = Sessions()) S->DeclineReceivedInvite();
        Dismiss();
    }
}

void UPTInvitePopupWidget::OnAcceptClicked()
{
    if (UMultiplayerSessionsSubsystem* S = Sessions()) S->AcceptReceivedInvite();
    Dismiss();
}

void UPTInvitePopupWidget::OnRejectClicked()
{
    if (UMultiplayerSessionsSubsystem* S = Sessions()) S->DeclineReceivedInvite();
    Dismiss();
}

void UPTInvitePopupWidget::Dismiss()
{
    if (bClosed) return;
    bClosed = true;
    RemoveFromParent();
}
