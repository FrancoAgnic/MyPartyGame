// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasPlayerState.h"
#include "Net/UnrealNetwork.h"

void ASillasPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASillasPlayerState, Rol);
    DOREPLIFETIME(ASillasPlayerState, bEliminadoEstaRonda);
    DOREPLIFETIME(ASillasPlayerState, PuntosMatch);
}

void ASillasPlayerState::Server_SetRol(ESillasRole InRol)
{
    if (HasAuthority())
    {
        Rol = InRol;
        OnRep_Rol(); // el host no recibe su propio OnRep; llamarlo manual (idioma del template)
    }
}

void ASillasPlayerState::Server_MarcarEliminado()
{
    if (HasAuthority()) { bEliminadoEstaRonda = true; }
}

void ASillasPlayerState::Server_SumarPuntos(int32 Puntos)
{
    if (HasAuthority()) { PuntosMatch += Puntos; }
}

void ASillasPlayerState::Server_ResetRonda()
{
    if (HasAuthority())
    {
        Rol = ESillasRole::Silla;
        bEliminadoEstaRonda = false;
        OnRep_Rol();
    }
}

void ASillasPlayerState::OnRep_Rol()
{
    // Fase 1: el HUD/pawn reaccionan por polling o en la posesión; hook listo
    // para cuando la Fase 2 necesite reaccionar al cambio de rol en cliente.
}
