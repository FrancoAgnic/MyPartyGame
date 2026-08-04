// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 0 — Esqueleto del PlayerState del modo Sillas.
// Hereda de APTPlayerState para conservar DisplayName/bIsHost del template.
// El rol (ESillasRole {Silla, Cazador}) y los puntos (D7b) llegan en Fase 1/5.
//
// PENDIENTE (Fase 1): verificar qué pasa con la clase del PlayerState en el
// seamless travel Lobby → L_TestArena — el motor conserva el APTPlayerState del
// lobby en vez de crear este subclass. Resolver ahí (override de
// HandleSeamlessTravelPlayer o registrar esta clase también en el lobby).

#pragma once
#include "CoreMinimal.h"
#include "PTPlayerState.h"
#include "SillasPlayerState.generated.h"

UCLASS()
class MYPARTYGAME_API ASillasPlayerState : public APTPlayerState
{
    GENERATED_BODY()
};
