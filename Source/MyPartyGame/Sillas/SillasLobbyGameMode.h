// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 0 — GameMode del lobby PARA el juego de sillas.
// El template deja el arranque a medio cablear a propósito: el botón Start del
// host (PTLobbyHUDWidget → Server_RequestStartGame) solo flippea
// APTGameState::LobbyState a Starting y nadie escucha ese cambio. Esta clase es
// la que escucha: cuando lo ve, viaja (seamless) a la arena. BP_LobbyGameMode se
// reparenta a esta clase para no tocar C++ ni assets del template en el resto.

#pragma once
#include "CoreMinimal.h"
#include "PTLobbyGameMode.h"
#include "SillasLobbyGameMode.generated.h"

UCLASS()
class MYPARTYGAME_API ASillasLobbyGameMode : public APTLobbyGameMode
{
    GENERATED_BODY()

public:
    virtual void BeginPlay() override;

protected:
    // Mapa al que viaja el Start. Editable en BP_LobbyGameMode por si se quiere
    // apuntar a otra arena sin recompilar.
    UPROPERTY(EditDefaultsOnly, Category="Sillas")
    FString ArenaMapPath = TEXT("/Game/Sillas/Maps/L_TestArena");

    // D8 — config del host que viaja a la arena como opciones de URL.
    // (La UI de lobby para tocarlos en runtime queda pendiente; por ahora se
    // editan en BP_LobbyGameMode.)
    UPROPERTY(EditDefaultsOnly, Category="Sillas", meta=(ClampMin="1", ClampMax="20"))
    int32 RondasPorMatch = 5;

    // 0 = automático (1 cazador para 2-5 jugadores, 2 para 6+, según BalanceData).
    UPROPERTY(EditDefaultsOnly, Category="Sillas", meta=(ClampMin="0", ClampMax="4"))
    int32 CazadoresIniciales = 0;

private:
    // El template no expone evento/virtual al flippear LobbyState (RPC no-virtual
    // en PTLobbyPlayerController), así que se sondea con un timer corto en el
    // servidor. 4 Hz: imperceptible para el arranque, gratis para el frame.
    void CheckStartRequested();
    FTimerHandle StartCheckHandle;
    bool bTravelStarted = false;
};
