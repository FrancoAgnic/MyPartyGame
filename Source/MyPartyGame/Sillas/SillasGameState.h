// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 0 — Esqueleto del GameState del modo Sillas.
// La máquina de estados de ronda (Lobby → IntroRonda → Musica → Silencio → FinRonda)
// con timers server-authoritative llega en Fase 1 (D3, D11).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "SillasGameState.generated.h"

UCLASS()
class MYPARTYGAME_API ASillasGameState : public AGameState
{
    GENERATED_BODY()
};
