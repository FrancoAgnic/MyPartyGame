// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 0 — Esqueleto del GameMode del modo Sillas (mapa de juego, no el lobby).
// Hereda de AGameMode (igual que PTLobbyGameMode) por InactivePlayerArray/reconexión.
// Asignación de roles, spawn de señuelos y flujo de ronda llegan en Fase 1.
// Gameplay server-authoritative siempre: este GameMode solo existe en el servidor.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SillasGameMode.generated.h"

UCLASS()
class MYPARTYGAME_API ASillasGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    ASillasGameMode();
};
