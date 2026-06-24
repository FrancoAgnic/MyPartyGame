// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — GameMode del mapa Lobby. Registra clases, gestiona PostLogin/Logout y marca al host.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PTLobbyGameMode.generated.h"

UCLASS()
class MYPARTYGAME_API APTLobbyGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APTLobbyGameMode();

    virtual void PreLogin(const FString& Options, const FString& Address,
                          const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

private:
    int32 PlayersJoined = 0;
};
