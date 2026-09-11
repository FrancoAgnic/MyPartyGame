// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — GameMode del mapa Lobby. Registra clases, gestiona PostLogin/Logout y marca al host.
// Hereda de AGameMode (no AGameModeBase) para aprovechar su InactivePlayerArray/FindInactivePlayer
// nativo: si un jugador se desconecta y vuelve a entrar con el mismo Steam ID dentro de
// InactivePlayerStateLifeSpan, el motor le devuelve su PlayerState (Fase de reconexión).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "PTLobbyGameMode.generated.h"

UCLASS()
class MYPARTYGAME_API APTLobbyGameMode : public AGameMode
{
    GENERATED_BODY()

public:
    APTLobbyGameMode();

    virtual void BeginPlay() override;
    virtual void PreLogin(const FString& Options, const FString& Address,
                          const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

    // Llegada por SEAMLESS TRAVEL al lobby (vuelta desde la partida con "Back to Lobby", manteniendo
    // la sala). El seamless NO pasa por PostLogin, así que acá re-damos el pawn, reseteamos el estado
    // de sala (no-listo + puntaje 0) y mantenemos el contador PlayersJoined (si queda en 0 el primer
    // Logout destruye la sesión). APTSculptGameMode lo llama vía Super en el sentido lobby→juego.
    virtual void HandleSeamlessTravelPlayer(AController*& C) override;

    /** El ANFITRIÓN se va. Saca a los clientes primero y recién después destruye la sesión y se
     *  va él: si el host cierra el mundo de una, los clientes se quedan con la conexión muerta y
     *  el juego se les cae. Lo llama el botón "Salir" cuando corre en el listen server. */
    void HostLeaveGame();

private:
    /** Segundo paso de HostLeaveGame, una vez que los avisos ya salieron hacia los clientes. */
    void FinishHostLeave();

    FTimerHandle HostLeaveTimer;

public:

    /** Ruta del mapa de juego. Asignar en BP_LobbyGameMode si se quiere cambiar sin recompilar. */
    UPROPERTY(EditDefaultsOnly, Category="Travel")
    FString GameMapPath = TEXT("/Game/Template/levels/Lvl-01");

    /** GameMode de esculpido (BP), que se FUERZA por la URL al viajar a un mapa de MOD. Así el mapa del
     *  creador no necesita configurar su GameMode: solo PlayerStarts + un APTSculptVolume + escenografía. */
    UPROPERTY(EditDefaultsOnly, Category="Travel")
    FString SculptGameModePath = TEXT("/Game/Template/Character/BP_SculptGameMode.BP_SculptGameMode_C");

    void TravelToGame();
    /** (Host) Viaja a un mapa de MOD (ya montado su pak) forzando el GameMode de esculpido por la URL.
     *  MapPackage = ruta de paquete del mapa (el MapName del mod.json). */
    void TravelToModMap(const FString& MapPackage);

    // ── Handshake de montaje de mapa de MOD antes del ServerTravel (M4 3b) ──
    // El host, antes de viajar a un mapa de mod, le pide a cada cliente que MONTE (o baje+monte) el pak, y
    // espera sus "listo" (con timeout). Así los clientes pueden cargar el mapa en el seamless travel.
    void StartModMapTravel(const FString& ModId, const FString& MapPath);
    void OnClientMapReady(class APlayerController* PC); // lo llama el cliente por Server RPC
private:
    void DoModMapTravelNow(); // ejecuta el travel al mapa de mod (una sola vez)
    FString  PendingModId, PendingMapPath;
    int32    MapReadyExpected = 0;
    int32    MapReadyGot = 0;
    bool     bModTravelStarted = false;
    FTimerHandle MapTravelTimeout;
public:

    /** (Host) Vuelca la config de partida del host (GameInstance + sesión) al APTGameState replicado
     *  para que los clientes puedan verla read-only en el lobby. Lo llama el BeginPlay y el
     *  GameSettings del host tras cada cambio. No-op fuera del servidor. */
    void SyncMatchSettingsToState();

    /** (Host) Marca si el host tiene abierto su panel de Game Settings, para que los clientes
     *  muestren/oculten su vista read-only en vivo. No-op fuera del servidor. */
    void SetHostSettingsPanelOpen(bool bOpen);

    // El self-travel de "Crear sesión" (MainMenu → MainMenu con ?listen, ver
    // PTMainMenuWidget::OnCreateSession) es un travel duro: al desconectar el mundo viejo,
    // el propio host pasa por Logout() con PlayersJoined llegando a 0, lo que sin este flag
    // dispararía "se fue el último jugador, destruir sesión" — matando la sesión recién creada
    // antes de que un amigo pueda encontrarla. Poner en true justo antes de un travel voluntario.
    UPROPERTY()
    bool bTravelInProgress = false;

    // Jugadores mínimos para poder arrancar el countdown (lobby interactivo, sin botón manual).
    static constexpr int32 MinPlayersToStart = 2;
    static constexpr int32 ReadyCountdownSeconds = 5;

    // Revisa PlayerArray: si hay >= MinPlayersToStart y todos con bIsReady, arranca el countdown
    // de ReadyCountdownSeconds (o lo deja correr si ya estaba en curso). Si deja de cumplirse
    // (alguien se des-listó o el conteo bajó del mínimo) y había countdown, lo cancela.
    // Llamar tras cualquier cambio que pueda afectar el resultado: ready toggle, join, leave.
    void CheckReadyState();

private:
    int32 PlayersJoined = 0;

    void CountdownTick();
    FTimerHandle CountdownTimerHandle;
};
