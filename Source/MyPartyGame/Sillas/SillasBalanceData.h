// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 1 — TODOS los números de balance del juego viven acá (convención 2 del
// plan: si un número de gameplay aparece hardcodeado en una PR, es un bug).
// El playtest vive de tocar estos valores en el asset DA_SillasBalance sin
// recompilar. Si el asset no existe, el GameMode crea uno con estos defaults.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SillasBalanceData.generated.h"

UCLASS(BlueprintType)
class MYPARTYGAME_API USillasBalanceData : public UDataAsset
{
    GENERATED_BODY()

public:
    // ---------- Fases de ronda (D3, D11: música corta, silencio largo) ----------

    // Espera tras llegar del lobby antes de arrancar la primera ronda (que todos
    // terminen el seamless travel y vean la arena).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fases", meta=(ClampMin="0.0"))
    float EsperaInicialSeg = 2.0f;

    // Cuenta regresiva antes de la primera fase de música de cada ronda.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fases", meta=(ClampMin="0.0"))
    float IntroRondaSeg = 3.0f;

    // Pantalla de resultado entre rondas (ganador, scoreboard en Fase 5).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fases", meta=(ClampMin="0.0"))
    float FinRondaSeg = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fases", meta=(ClampMin="1.0"))
    float MusicaSegBase = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fases", meta=(ClampMin="1.0"))
    float SilencioSegBase = 20.0f;

    // ---------- Intensificación (D12: al caer sillas, menos música y más silencio) ----------
    // Factor lineal por fracción de sillas eliminadas: con f = eliminadas/total,
    // Musica = Base*(1 - f*Factor) y Silencio = Base*(1 + f*Factor), con topes.
    // (Si el playtest pide una curva no lineal, se cambia por UCurveFloat en Fase 4.)

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intensificacion", meta=(ClampMin="0.0", ClampMax="1.0"))
    float FactorIntensificacion = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intensificacion", meta=(ClampMin="1.0"))
    float MusicaSegMin = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Intensificacion", meta=(ClampMin="1.0"))
    float SilencioSegMax = 30.0f;

    // ---------- Captura: caminata de cola (D5/D6) — los consume la Fase 2 ----------
    // "La velocidad de la caminata de cola y la distancia de sentado válido son
    // los dos números de balance más importantes del juego" (diseño, D5).

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Captura", meta=(ClampMin="0.0"))
    float VelocidadCaminataCola = 150.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Captura", meta=(ClampMin="0.0"))
    float DistanciaSentadoValido = 120.0f;

    // Medio cono frente al asiento objetivo dentro del cual el sentado cuenta.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Captura", meta=(ClampMin="0.0", ClampMax="180.0"))
    float AnguloSentadoValidoGrados = 45.0f;

    // D6: castigo por sentarse en un señuelo.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Captura", meta=(ClampMin="0.0"))
    float DuracionDolorSeg = 1.8f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Captura", meta=(ClampMin="0.0", ClampMax="1.0"))
    float MultiplicadorVelocidadDolor = 0.35f;

    // ---------- Movilidad de sillas (D10) — los consume la Fase 3 ----------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sillas", meta=(ClampMin="0.0"))
    float VelocidadCaminataSilla = 300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sillas", meta=(ClampMin="0.0"))
    float VelocidadSprintSilla = 550.0f;

    // Stamina en segundos de sprint continuo y su regeneración por segundo quieto.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sillas", meta=(ClampMin="0.1"))
    float StaminaSprintSeg = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sillas", meta=(ClampMin="0.0"))
    float StaminaRegenPorSeg = 0.5f;

    // Empujón silla→silla (la traición física de D2/D10).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Sillas", meta=(ClampMin="0.0"))
    float ImpulsoEmpujon = 600.0f;

    // ---------- Cazador (D17: caza con oído; sin sprint propio por ahora) ----------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Cazador", meta=(ClampMin="0.0"))
    float VelocidadCazador = 400.0f;

    // ---------- Señuelos (D9: densidad media) ----------

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senuelos", meta=(ClampMin="0"))
    int32 SenuelosMin = 15;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senuelos", meta=(ClampMin="0"))
    int32 SenuelosMax = 25;

    // Radio de jitter alrededor de cada TargetPoint "SillaSpawn" del mapa.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Senuelos", meta=(ClampMin="0.0"))
    float JitterSpawnSenuelo = 150.0f;

    // ---------- Roles y match (D7, D8) ----------

    // D8: default de cazadores iniciales — 1 para 2–5 jugadores, 2 desde este umbral.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match", meta=(ClampMin="2"))
    int32 UmbralJugadoresParaDosCazadores = 6;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Match", meta=(ClampMin="1"))
    int32 RondasPorMatch = 5;

    // ---------- Puntos (D7b) — los consume la Fase 5 ----------
    // Regla de referencia: ganar la ronda vivo ≥ mejor puntaje posible como
    // cazador de esa ronda (anti dejarse-atrapar).

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Puntos", meta=(ClampMin="0"))
    int32 PuntosPorFaseSobrevivida = 10;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Puntos", meta=(ClampMin="0"))
    int32 PuntosPorCaptura = 25;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Puntos", meta=(ClampMin="0"))
    int32 BonusUltimoVivo = 100;
};
