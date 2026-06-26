// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — PlayerController del lobby. Agrega el Input Mapping Context de lobby.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PTLobbyPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UPTLobbyEscapeMenuWidget;
struct FInputActionValue;

UCLASS()
class MYPARTYGAME_API APTLobbyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    /**
     * Hook de arranque del template: solo flippea APTGameState::LobbyState a Starting si quien
     * llama es el host. El template no decide qué pasa después de eso — eso es trabajo de cada
     * juego (ver bUseSeamlessTravel en PTLobbyGameMode).
     */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby")
    void Server_RequestStartGame();

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;

    /** Asignar IMC_Lobby en el Blueprint derivado BP_LobbyPlayerController. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
    UInputMappingContext* LobbyMappingContext;

    /** Acción de Escape (crear el Input Action en el editor y asignarlo acá y en LobbyMappingContext). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
    UInputAction* EscapeMenuAction;

    /** Asignar WBP_LobbyEscapeMenu (o derivado) en el Blueprint derivado. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    TSubclassOf<UPTLobbyEscapeMenuWidget> EscapeMenuWidgetClass;

    void ToggleEscapeMenu(const FInputActionValue& Value);

private:
    UPROPERTY() UPTLobbyEscapeMenuWidget* EscapeMenuWidget = nullptr;
};
