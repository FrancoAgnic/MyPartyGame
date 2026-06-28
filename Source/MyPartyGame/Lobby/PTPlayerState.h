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

    // Llamar solo desde el servidor (HasAuthority).
    void Server_SetDisplayName(const FString& InName);
    void Server_SetHost(bool bInHost);

    UFUNCTION() void OnRep_DisplayName();
};
