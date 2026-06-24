// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTPlayerState.h"
#include "Net/UnrealNetwork.h"

void APTPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTPlayerState, DisplayName);
    DOREPLIFETIME(APTPlayerState, bIsReady);
    DOREPLIFETIME(APTPlayerState, bIsHost);
}

void APTPlayerState::Server_SetDisplayName(const FString& InName)
{
    if (HasAuthority())
    {
        DisplayName = InName;
        OnRep_DisplayName(); // El host no recibe su propio OnRep; llamarlo manual.
    }
}

void APTPlayerState::Server_SetReady(bool bInReady)
{
    if (HasAuthority())
    {
        bIsReady = bInReady;
        OnRep_IsReady();
    }
}

void APTPlayerState::Server_SetHost(bool bInHost)
{
    if (HasAuthority()) { bIsHost = bInHost; }
}

void APTPlayerState::OnRep_DisplayName() { /* TODO (Fase 5): refrescar UI del lobby */ }
void APTPlayerState::OnRep_IsReady()     { /* TODO (Fase 5): refrescar UI del lobby */ }
