// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTPlayerState.h"
#include "PTLobbyCharacter.h"
#include "Net/UnrealNetwork.h"

void APTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTPlayerState, DisplayName);
    DOREPLIFETIME(APTPlayerState, bIsHost);
    DOREPLIFETIME(APTPlayerState, bIsReady);
    DOREPLIFETIME(APTPlayerState, bHasGuessedThisTurn);
    DOREPLIFETIME(APTPlayerState, GameScore);
    DOREPLIFETIME(APTPlayerState, HeadBlob);
    DOREPLIFETIME(APTPlayerState, HeadVersion);
}

void APTPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
    Super::CopyProperties(NewPlayerState);
    if (APTPlayerState* PT = Cast<APTPlayerState>(NewPlayerState))
    {
        PT->DisplayName = DisplayName;
        PT->bIsHost     = bIsHost;
        PT->HeadBlob    = HeadBlob;    // la cabeza custom viaja al Lvl-01 (seamless travel)
        PT->HeadVersion = HeadVersion; // ...y su versión, para que el pawn nuevo la aplique
        // bHasGuessedThisTurn NO se copia: es estado por-turno, arranca en false en el juego.
    }
}

void APTPlayerState::OnRep_HeadBlob()
{
    // Cuando llega/actualiza la cabeza replicada, aplicarla al pawn de este jugador.
    if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(GetPawn()))
        Char->ApplyReplicatedHead();
}

void APTPlayerState::Server_SetDisplayName(const FString& InName)
{
    if (HasAuthority())
    {
        DisplayName = InName;
        OnRep_DisplayName(); // El host no recibe su propio OnRep; llamarlo manual.
    }
}

void APTPlayerState::Server_SetHost(bool bInHost)
{
    if (HasAuthority()) { bIsHost = bInHost; }
}

void APTPlayerState::OnRep_DisplayName() { /* El HUD del lobby lee DisplayName por polling, no necesita reaccionar acá. */ }
