// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTFriendRowWidget.h"
#include "../PTTextTable.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"

bool UPTFriendRowWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (InviteButton)
        InviteButton->OnClicked.AddDynamic(this, &UPTFriendRowWidget::OnInviteClicked);

    return true;
}

void UPTFriendRowWidget::Init(const FPTFriendInfo& InFriend)
{
    Friend = InFriend;

    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    if (NameText)
        NameText->SetText(FText::FromString(Friend.DisplayName));

    if (StatusText)
    {
        const TCHAR* Key = Friend.bPlayingThisGame ? TEXT("FRIEND_STATUS_PLAYING")
                         : Friend.bOnline          ? TEXT("FRIEND_STATUS_ONLINE")
                                                    : TEXT("FRIEND_STATUS_OFFLINE");
        StatusText->SetText(PTText::Get(Key));
    }

    // Solo tiene sentido invitar a un amigo en línea (offline no recibiría la invitación).
    if (InviteButton)     InviteButton->SetIsEnabled(Friend.bOnline);
    if (InviteButtonText) InviteButtonText->SetText(PTText::Get(TEXT("FRIEND_INVITE")));
}

void UPTFriendRowWidget::OnInviteClicked()
{
    if (Sessions) Sessions->InviteFriend(Friend.UserId);

    // Feedback simple: deshabilitar y cambiar el texto a "Invitado" (evita spamear invitaciones).
    if (InviteButton)     InviteButton->SetIsEnabled(false);
    if (InviteButtonText) InviteButtonText->SetText(PTText::Get(TEXT("FRIEND_INVITED")));
}
