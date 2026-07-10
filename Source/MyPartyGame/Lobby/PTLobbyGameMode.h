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

    /** Ruta del mapa de juego. Asignar en BP_LobbyGameMode si se quiere cambiar sin recompilar. */
    UPROPERTY(EditDefaultsOnly, Category="Travel")
    FString GameMapPath = TEXT("/Game/Template/levels/Lvl-01");

    void TravelToGame();

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
