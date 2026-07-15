// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — Datos replicados por jugador en el lobby.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "PTHeadSaveGame.h" // FPTHeadSection (geometría de la cabeza custom, replicada)
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

    // ── Cabeza custom (replicada) ───────────────────────────────────────────
    // Geometría final horneada de la cabeza (arcilla + pintura + ojos), serializada y COMPRIMIDA
    // (Zlib) a bytes: así entra en el límite de replicación (la malla cruda supera los ~64KB).
    // Se replica a todos y sobrevive el seamless travel (CopyProperties) → se ve en lobby y Lvl-01.
    UPROPERTY(ReplicatedUsing=OnRep_HeadBlob)
    TArray<uint8> HeadBlob;

    // Sube +1 cada vez que el jugador confirma una cabeza nueva. El pawn compara esta versión
    // con la que tiene aplicada y se re-aplica si difiere. Es la red de seguridad ante el orden
    // de llegada: si el blob replica ANTES de que el PlayerState tenga pawn, OnRep_HeadBlob no
    // encuentra a quién aplicársela y nunca vuelve a dispararse (el valor ya no cambia).
    UPROPERTY(Replicated)
    int32 HeadVersion = 0;

    UFUNCTION() void OnRep_HeadBlob();

    // Llamar solo desde el servidor (HasAuthority).
    void Server_SetDisplayName(const FString& InName);
    void Server_SetHost(bool bInHost);

    UFUNCTION() void OnRep_DisplayName();

    // El seamless travel Lobby→Lvl-01 puede recrear el PlayerState; sin copiar estos
    // campos a mano se perderían (nombre/host quedarían vacíos en el juego).
    virtual void CopyProperties(APlayerState* NewPlayerState) override;
};
