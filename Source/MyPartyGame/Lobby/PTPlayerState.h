// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — Datos replicados por jugador en el lobby.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PTPlayerState.generated.h"

UCLASS()
class MYPARTYGAME_API APTPlayerState : public APlayerState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing=OnRep_DisplayName, BlueprintReadOnly, Category="Lobby")
    FString DisplayName;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    bool bIsHost = false;

    // Listo para arrancar (toggle en el HUD del lobby). El GameMode revisa esto en
    // APTLobbyGameMode::CheckReadyState para el countdown automatico.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    bool bIsReady = false;

    // ── Partida (Sculpturillo) ──────────────────────────────────────────────
    // Si este jugador ya adivinó la palabra del turno actual. El servidor lo resetea
    // al empezar cada turno. Se usa para el HUD ("quién adivinó") y para saber cuándo
    // terminó el turno (todos adivinaron).
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    bool bHasGuessedThisTurn = false;

    // Puntaje acumulado de la partida (estilo Skribbl). El servidor lo resetea a 0 al
    // empezar cada partida y suma al adivinar / cuando alguien adivina tu escultura.
    // (Se llama GameScore y no Score porque APlayerState ya tiene un Score propio.)
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    int32 GameScore = 0;

    // Llamar solo desde el servidor (HasAuthority).
    void Server_SetDisplayName(const FString& InName);
    void Server_SetHost(bool bInHost);

    UFUNCTION() void OnRep_DisplayName();

    // El seamless travel Lobby→Lvl-01 puede recrear el PlayerState; sin copiar estos
    // campos a mano se perderían (nombre/host quedarían vacíos en el juego).
    virtual void CopyProperties(APlayerState* NewPlayerState) override;
};
