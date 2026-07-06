// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTPlayerState.h"
#include "Net/UnrealNetwork.h"

void APTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTPlayerState, DisplayName);
    DOREPLIFETIME(APTPlayerState, bIsHost);
    DOREPLIFETIME(APTPlayerState, bHasGuessedThisTurn);
}

void APTPlayerState::CopyProperties(APlayerState* NewPlayerState)
{
    Super::CopyProperties(NewPlayerState);
    if (APTPlayerState* PT = Cast<APTPlayerState>(NewPlayerState))
    {
        PT->DisplayName = DisplayName;
        PT->bIsHost     = bIsHost;
        // bHasGuessedThisTurn NO se copia: es estado por-turno, arranca en false en el juego.
    }
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
