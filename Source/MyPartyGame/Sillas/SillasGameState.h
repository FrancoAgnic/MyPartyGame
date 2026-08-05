// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 1 — GameState del modo Sillas: la máquina de estados de ronda replicada.
// Los timers que la mueven son server-authoritative y viven en ASillasGameMode
// (convención 3 del plan); acá solo está el estado replicado que los clientes
// leen para HUD/audio. Hereda de APTGameState para conservar en la arena la
// info de sesión (SessionDisplayName/SessionCode/MaxPlayers) que el lobby ya
// mostraba — no viaja con el seamless travel, ASillasGameMode la re-puebla.

#pragma once
#include "CoreMinimal.h"
#include "PTGameState.h"
#include "SillasGameState.generated.h"

class USillasBalanceData;

// D3/D11: patrón fijo Musica (corta, cazador baila) ↔ Silencio (largo, caza).
UENUM(BlueprintType)
enum class ESillasFase : uint8
{
    Esperando  UMETA(DisplayName="Esperando"),   // llegaron del lobby, ronda no arrancó
    IntroRonda UMETA(DisplayName="IntroRonda"),  // countdown + asignación de roles visible
    Musica     UMETA(DisplayName="Musica"),
    Silencio   UMETA(DisplayName="Silencio"),
    FinRonda   UMETA(DisplayName="FinRonda"),
    FinMatch   UMETA(DisplayName="FinMatch")
};

UCLASS()
class MYPARTYGAME_API ASillasGameState : public APTGameState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // Config de balance compartida con los clientes: los pawns leen velocidades
    // de acá para que servidor y cliente muevan igual (sin rubber-banding).
    // Solo replica si es el asset DA_SillasBalance (referencia estable); si el
    // GameMode usó el fallback NewObject, en clientes queda null y los pawns
    // caen a GetDefault<USillasBalanceData>() — mismos números, así que da igual.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    TObjectPtr<USillasBalanceData> Balance;

    UPROPERTY(ReplicatedUsing=OnRep_Fase, BlueprintReadOnly, Category="Sillas")
    ESillasFase Fase = ESillasFase::Esperando;

    // Momento (en tiempo de servidor sincronizado) en que termina la fase actual.
    // El HUD calcula el countdown localmente contra GetServerWorldTimeSeconds().
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    float FaseTerminaEnServerTime = 0.f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    int32 RondaActual = 0;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    int32 SillasVivas = 0;

    // Cuántas sillas-jugador había al empezar la ronda (para la intensificación D12 y el HUD).
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    int32 SillasAlInicioDeRonda = 0;

    UFUNCTION(BlueprintPure, Category="Sillas")
    float GetSegundosRestantesDeFase() const;

    UFUNCTION() void OnRep_Fase();
};
