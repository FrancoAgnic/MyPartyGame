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

private:
    int32 PlayersJoined = 0;
};
