// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameState.h"
#include "Net/UnrealNetwork.h"
#include "PTLobbyPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void APTGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTGameState, LobbyState);
    DOREPLIFETIME(APTGameState, SessionDisplayName);
    DOREPLIFETIME(APTGameState, SessionCode);
    DOREPLIFETIME(APTGameState, MaxPlayers);
    DOREPLIFETIME(APTGameState, CountdownSecondsRemaining);
    DOREPLIFETIME(APTGameState, MatchTurnDuration);
    DOREPLIFETIME(APTGameState, MatchNumRounds);
    DOREPLIFETIME(APTGameState, MatchRevealFraction);
    DOREPLIFETIME(APTGameState, bMatchFriendsOnly);
    DOREPLIFETIME(APTGameState, MatchWordPackTitle);
    DOREPLIFETIME(APTGameState, MatchWordPackId);
    DOREPLIFETIME(APTGameState, MatchWordPackPreviewURL);
    DOREPLIFETIME(APTGameState, MatchMapTitle);
    DOREPLIFETIME(APTGameState, MatchMapModId);
    DOREPLIFETIME(APTGameState, bHostSettingsPanelOpen);
}

void APTGameState::Multicast_LobbyChat_Implementation(const FString& Name, const FString& Message)
{
    OnLobbyChat.Broadcast(Name, Message);
}

void APTGameState::OnRep_MatchMapModId()
{
    // (Cliente) Cambió el mapa elegido por el host → asegurarse de tenerlo localmente (auto-descarga/
    // recepción por chunks). Lo maneja el PlayerController local, que tiene los RPC al servidor.
    if (UWorld* W = GetWorld())
        if (APlayerController* PC = W->GetFirstPlayerController())
            if (APTLobbyPlayerController* LPC = Cast<APTLobbyPlayerController>(PC))
                LPC->EnsureSelectedMapAvailable(MatchMapModId);
}
