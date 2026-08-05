// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasGameState.h"
#include "Net/UnrealNetwork.h"

void ASillasGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASillasGameState, Balance);
    DOREPLIFETIME(ASillasGameState, Fase);
    DOREPLIFETIME(ASillasGameState, FaseTerminaEnServerTime);
    DOREPLIFETIME(ASillasGameState, RondaActual);
    DOREPLIFETIME(ASillasGameState, SillasVivas);
    DOREPLIFETIME(ASillasGameState, SillasAlInicioDeRonda);
}

float ASillasGameState::GetSegundosRestantesDeFase() const
{
    return FMath::Max(0.f, FaseTerminaEnServerTime - GetServerWorldTimeSeconds());
}

void ASillasGameState::OnRep_Fase()
{
    // Hook para clientes: acá va a reaccionar el audio (Fase 4: música/silencio)
    // y el HUD (Fase 5). En Fase 1 no hay nada que hacer todavía.
}
