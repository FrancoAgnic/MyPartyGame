// Copyright Epic Games, Inc. All Rights Reserved.
// HUD del lobby: lista de jugadores conectados y el código de sala (si es privada).
// Refresca por timer en vez de enganchar delegates de replicación — la lista es chica
// y no es sensible a performance, así que no vale la pena la plomería extra.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTLobbyHUDWidget.generated.h"

class UVerticalBox;
class UTextBlock;

UCLASS()
class MYPARTYGAME_API UPTLobbyHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Llamar desde el PlayerController del lobby en BeginPlay (mismo patrón que MenuSetup). */
    UFUNCTION(BlueprintCallable, Category = "Lobby")
    void ShowHUD();

protected:
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))         UVerticalBox* PlayersBox;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   RoomCodeText;

    void RefreshPlayerList();

private:
    FTimerHandle RefreshTimerHandle;
};
