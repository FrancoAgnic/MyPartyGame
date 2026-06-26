// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameState.h"
#include "Net/UnrealNetwork.h"

void APTGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTGameState, LobbyState);
    DOREPLIFETIME(APTGameState, SessionDisplayName);
    DOREPLIFETIME(APTGameState, SessionCode);
}

void APTGameState::OnRep_LobbyState()
{
    // TODO (Fase 5): notificar a la UI del lobby el cambio de estado.
}
