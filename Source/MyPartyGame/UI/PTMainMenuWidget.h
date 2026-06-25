// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 2 — Widget principal del menú. Wiring entre UI y UMultiplayerSessionsSubsystem.
// INVARIANTE: este widget nunca llama a OSS directamente, solo al subsistema de la Fase 1.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "PTGameInstance.h"
#include "PTMainMenuWidget.generated.h"

class UTextBlock;

class UButton;
class UMultiplayerSessionsSubsystem;
class UPTCreateSessionWidget;
class UPTFindSessionsWidget;

UCLASS()
class MYPARTYGAME_API UPTMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /**
     * Inicializar y mostrar el menú. Llamar desde el PlayerController en BeginPlay.
     * @param InNumPublicConnections  Capacidad por defecto para la sesión a crear.
     * @param InLobbyPath             Path del mapa Lobby (sin "?listen", se agrega al viajar).
     */
    UFUNCTION(BlueprintCallable, Category = "Sessions")
    void MenuSetup(int32 InNumPublicConnections = 4,
                   FString InLobbyPath = FString(TEXT("/Game/Maps/Lobby")));

protected:
    virtual bool Initialize() override;
    virtual void NativeDestruct() override;

    // ------------------------------------------------------------------
    // Botones principales (nombres deben coincidir exactamente en el WBP)
    // ------------------------------------------------------------------
    UPROPERTY(meta = (BindWidget))         UButton* HostButton;
    UPROPERTY(meta = (BindWidget))         UButton* FindButton;
    UPROPERTY(meta = (BindWidget))         UButton* QuitButton;

    // Sub-paneles opcionales incrustados en el WBP
    UPROPERTY(meta = (BindWidgetOptional)) UPTCreateSessionWidget* CreatePanel;
    UPROPERTY(meta = (BindWidgetOptional)) UPTFindSessionsWidget*  FindPanel;

    // Fase 4 — Texto de error de conexión (ej: "Contraseña incorrecta")
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* ErrorText;

    // Fase 5 — Código de invitación recién generado (sesión privada). Se completa solo
    // cuando CreateSession se hizo con bPrivate=true; queda vacío/oculto en sesión pública.
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* GeneratedCodeText;

    // ------------------------------------------------------------------
    // Handlers de botones
    // ------------------------------------------------------------------
    UFUNCTION() void OnHostClicked();
    UFUNCTION() void OnFindClicked();
    UFUNCTION() void OnQuitClicked();

    // ------------------------------------------------------------------
    // Callbacks de los delegates del subsistema
    // ------------------------------------------------------------------
    void OnLogin(bool bWasSuccessful);
    void OnCreateSession(bool bWasSuccessful);
    void OnFindSessions(const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful);
    void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);

    void MenuTearDown();

private:
    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;

    int32   NumPublicConnections = 4;
    FString LobbyPath;
};
