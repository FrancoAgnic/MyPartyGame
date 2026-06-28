// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameState.h"
#include "Net/UnrealNetwork.h"

void APTGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTGameState, LobbyState);
    DOREPLIFETIME(APTGameState, SessionDisplayName);
    DOREPLIFETIME(APTGameState, SessionCode);
    DOREPLIFETIME(APTGameState, MaxPlayers);
}
