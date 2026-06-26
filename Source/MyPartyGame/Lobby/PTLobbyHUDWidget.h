// Copyright Epic Games, Inc. All Rights Reserved.
// HUD del lobby: lista de jugadores, contador, código de sala (si es privada), Leave/Start Game.
// Refresca por timer en vez de enganchar delegates de replicación — la lista es chica
// y no es sensible a performance, así que no vale la pena la plomería extra.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTLobbyHUDWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;

UCLASS()
class MYPARTYGAME_API UPTLobbyHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Llamar desde el PlayerController del lobby en BeginPlay (mismo patrón que MenuSetup). */
    UFUNCTION(BlueprintCallable, Category = "Lobby")
    void ShowHUD();

protected:
    virtual bool Initialize() override;
    virtual void NativeDestruct() override;

    UPROPERTY(meta = (BindWidget))         UVerticalBox* PlayersBox;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   PlayersCountText;   // "4/8"
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   LobbyStatusText;    // "Waiting for players..."

    // Se ocultan juntos si la sala es pública (SessionCode vacío).
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*      PrivateRoomPanel;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   RoomCodeText;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      CopyCodeButton;

    UPROPERTY(meta = (BindWidgetOptional)) UButton*      LeaveGameButton;
    // Solo habilitado/visible para el jugador host (ver RefreshPlayerList).
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      StartGameButton;

    UFUNCTION() void OnCopyCodeClicked();
    UFUNCTION() void OnLeaveGameClicked();
    UFUNCTION() void OnStartGameClicked();

    void RefreshPlayerList();

private:
    FTimerHandle RefreshTimerHandle;
    FString CachedRoomCode;
};
