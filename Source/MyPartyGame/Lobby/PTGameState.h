// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — Estado compartido replicado del lobby.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "PTGameState.generated.h"

UENUM(BlueprintType)
enum class EPTLobbyState : uint8
{
    WaitingForPlayers UMETA(DisplayName="Waiting"),
    Starting          UMETA(DisplayName="Starting"),
    InGame            UMETA(DisplayName="InGame")
};

UCLASS()
class MYPARTYGAME_API APTGameState : public AGameState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    EPTLobbyState LobbyState = EPTLobbyState::WaitingForPlayers;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    FString SessionDisplayName;

    // Vacío si la sala es pública. Replicado para que cualquier jugador (no solo el host)
    // pueda verlo y compartirlo con quien todavía no se unió.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    FString SessionCode;

    // Tope de jugadores elegido al crear la sesión (para el "4/10" del HUD del lobby). Default 10.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    int32 MaxPlayers = 10;

    // Segundos restantes del countdown automático (todos listos → arranca). -1 = sin countdown.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Lobby")
    int32 CountdownSecondsRemaining = -1;

    // ── Config de partida elegida por el host, replicada para que los clientes la VEAN (read-only) ──
    // El host la edita en su GameSettings (vive en el GameInstance del servidor); el LobbyGameMode
    // la vuelca acá con SyncMatchSettingsToState(). Los defaults matchean FPTMatchSettings.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Match") float   MatchTurnDuration   = 90.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Match") int32   MatchNumRounds      = 3;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Match") float   MatchRevealFraction = 0.3f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Match") bool    bMatchFriendsOnly   = false;
    // Título del banco de palabras activo. Vacío = banco por defecto.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Match") FString MatchWordPackTitle;

    // true mientras el host tiene ABIERTO su panel de Game Settings: los clientes muestran su
    // panelcito read-only solo en ese lapso (lo ven cambiar en vivo mientras el host edita).
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Match") bool bHostSettingsPanelOpen = false;

    // La lista de jugadores vive en PlayerArray (heredado de AGameStateBase). No replicar aparte.
};
