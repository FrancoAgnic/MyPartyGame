// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 0/1 — GameMode de la arena (L_TestArena). Solo existe en el servidor.
//
// Integración con el flujo multiplayer del template (Fase 0):
//  - Llega gente por seamless travel desde el lobby (ASillasLobbyGameMode dispara
//    el viaje): HandleSeamlessTravelPlayer restaura DisplayName/bIsHost, que se
//    pierden al cambiar la clase de PlayerState en el viaje.
//  - Join-in-progress (conexión directa a mitad de partida): PreLogin valida la
//    contraseña igual que el lobby y PostLogin asigna nombre/host igual que el lobby.
//  - Se vacía el server → se destruye la sesión Steam (sin salas fantasma).
//
// Máquina de rondas (Fase 1, D3/D11/D12): timers server-authoritative que
// alternan Musica ↔ Silencio y escriben el estado replicado en ASillasGameState.
// Los números salen SIEMPRE de USillasBalanceData (convención 2 del plan).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "SillasPlayerState.h"
#include "SillasGameMode.generated.h"

class USillasBalanceData;
class ASillasGameState;
class ASillasSenuelo;
enum class ESillasFase : uint8;

UCLASS()
class MYPARTYGAME_API ASillasGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    ASillasGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void BeginPlay() override;
    virtual void PreLogin(const FString& Options, const FString& Address,
                          const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void HandleSeamlessTravelPlayer(AController*& C) override;
    virtual void Logout(AController* Exiting) override;

    // Integración con el flujo nativo de spawn (RestartPlayer): la clase de pawn
    // sale del rol y las sillas nacen en los puntos "SillaSpawn" del mapa,
    // mezcladas entre los señuelos.
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
    virtual AActor* FindPlayerStart_Implementation(AController* Player, const FString& IncomingName) override;

    // API para la Fase 2 (captura): el server reporta una silla-jugador rota.
    // Descuenta SillasVivas, convierte al eliminado en cazador (D1) y cierra la
    // ronda si queda una sola silla (D2).
    void NotificarSillaEliminada(ASillasPlayerState* Eliminado);

    // FASE 2 — Resolución server-authoritative del sentado de la caminata de
    // cola (D5/D6): señuelo → dolor 1.8s · silla-jugador → rotura + infección.
    void ResolverSentado(class ASillasPawnCazador* Cazador, AActor* Objetivo);

protected:
    // Config de balance. Se intenta cargar el asset DA_SillasBalance; si no
    // existe todavía, se usa un objeto con los defaults de C++.
    UPROPERTY(EditDefaultsOnly, Category="Sillas")
    TSoftObjectPtr<USillasBalanceData> BalanceAsset;

    UPROPERTY(Transient, BlueprintReadOnly, Category="Sillas")
    TObjectPtr<USillasBalanceData> Balance;

    // Pawn de la silla-jugador (v0: ASillasPawnSilla).
    UPROPERTY(EditDefaultsOnly, Category="Sillas")
    TSubclassOf<APawn> PawnSillaClass;

    // Pawn del cazador (v0: el mannequin BP_ThirdPersonCharacter del template;
    // el cazador C++ con baile y caminata de cola llega en Fase 2/3).
    UPROPERTY(EditDefaultsOnly, Category="Sillas")
    TSubclassOf<APawn> PawnCazadorClass;

    // Clase del señuelo (los BPs pueden pisarla — debe verse IDÉNTICA al pawn silla).
    UPROPERTY(EditDefaultsOnly, Category="Sillas")
    TSubclassOf<class ASillasSenuelo> SenueloClass;

private:
    // --- Flujo de ronda (todo servidor) ---
    void IniciarRonda();
    void EmpezarMusica();
    void EmpezarSilencio();
    void TerminarRonda();
    void TerminarMatch();
    void VolverAlLobby();

    // D7b: puntos por fase COMPLETA sobrevivida (se otorga en cada transición
    // de fase a las sillas que siguen vivas).
    void OtorgarPuntosSupervivencia();

    void AsignarRoles();
    void SetFase(ESillasFase NuevaFase, float DuracionSeg);

    // Limpia los señuelos viejos, reparte puntos de spawn entre sillas-jugador y
    // señuelos (D9), y respawnea el pawn de cada jugador según su rol.
    void RepartirPawnsYSenuelos();

    // Rompe una silla-jugador: escombros, eliminación y re-posesión como cazador
    // en el lugar de la rotura (D1: la infección no corta el flujo de la ronda).
    void RomperSilla(class ASillasPawnSilla* Silla, class ASillasPawnCazador* Cazador);

    UPROPERTY(Transient)
    TArray<TObjectPtr<ASillasSenuelo>> Senuelos;

    // Punto de spawn reservado por controller para la ronda (lo consume FindPlayerStart).
    UPROPERTY(Transient)
    TMap<TObjectPtr<AController>, TObjectPtr<AActor>> PuntoAsignado;

    // D12: duraciones intensificadas según sillas eliminadas en la ronda.
    float DuracionMusicaActual() const;
    float DuracionSilencioActual() const;

    ASillasGameState* SillasGS() const;

    FTimerHandle FaseTimer;
    int32 PlayersJoined = 0;

    // FASE 7 — métricas de playtest (solo server; CSV en Saved/SillasMetrics/).
    // Los tres números que el plan pide medir: duración de ronda real,
    // % de capturas erradas y supervivencia por fase.
    struct FMetricasRonda
    {
        int32 Ronda = 0;
        int32 JugadoresAlInicio = 0;
        float DuracionSeg = 0.f;
        int32 Capturas = 0;
        int32 SentadasErradas = 0;
        int32 FasesCompletadas = 0;
    };
    TArray<FMetricasRonda> MetricasMatch;
    FMetricasRonda MetricasRonda;
    double InicioRondaSeg = 0.0;

    void VolcarMetricasCSV();

    // D7: cazadores iniciales sin repetir hasta agotar la lista.
    TSet<TWeakObjectPtr<ASillasPlayerState>> YaFueronCazadorInicial;

    // D8 — llegan como opciones de la URL de travel desde el lobby
    // (?Rondas= y ?Cazadores=; 0 = automático). Fallback: BalanceData.
    int32 RondasOverride = 0;
    int32 CazadoresOverride = 0;

protected:
    // Mapa del lobby al que se vuelve al terminar el match (seamless).
    UPROPERTY(EditDefaultsOnly, Category="Sillas")
    FString LobbyMapPath = TEXT("/Game/Template/levels/Lobby");
};
