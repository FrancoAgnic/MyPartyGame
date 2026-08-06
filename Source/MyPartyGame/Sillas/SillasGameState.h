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

// FASE 5 — Línea del feed de eventos (capturas, eliminaciones, rondas).
USTRUCT(BlueprintType)
struct FSillasFeedEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FString Texto;

    // Momento del evento en tiempo de servidor (el HUD descarta las viejas).
    UPROPERTY(BlueprintReadOnly)
    float ServerTime = 0.f;
};

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
    ASillasGameState();

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

    // Total de rondas del match (D7; configurable desde el lobby, D8).
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    int32 RondasTotales = 5;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    int32 SillasVivas = 0;

    // Cuántas sillas-jugador había al empezar la ronda (para la intensificación D12 y el HUD).
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    int32 SillasAlInicioDeRonda = 0;

    UFUNCTION(BlueprintPure, Category="Sillas")
    float GetSegundosRestantesDeFase() const;

    // FASE 5 — feed de eventos (últimas ~6 líneas). Escribir solo en el server.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Sillas")
    TArray<FSillasFeedEntry> Feed;

    // Solo servidor: agrega una línea al feed y recorta las viejas.
    void AgregarFeed(const FString& Texto);

    // FASE 2 — VFX greybox de rotura: pedazos con física en cada cliente
    // (cosmético local, no replicado; el server solo manda el epicentro).
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_EfectoRoturaSilla(FVector Epicentro);

    // FASE 4 — Sonido posicional one-shot en todos los clientes (taunt, jadeo).
    // El asset viaja como referencia (todos lo tienen); la atenuación se arma
    // inline con el radio pedido.
    UFUNCTION(NetMulticast, Unreliable)
    void Multicast_SonidoEnPosicion(FVector Posicion, USoundBase* Sonido, float RadioAudible);

    // FASE 4 — Música por fases (D3/D12): loop 2D que suena en cada cliente
    // durante la fase Musica, con pitch intensificado al caer sillas.
    // Editable en BP_SillasGameState.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Sillas|Audio")
    TObjectPtr<USoundBase> MusicaSound;

    UFUNCTION() void OnRep_Fase();

private:
    UPROPERTY(Transient)
    TObjectPtr<class UAudioComponent> MusicaComp;
};
