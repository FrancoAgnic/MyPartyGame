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
class UPTEnterCodeWidget;
class UPTSettingsWidget;

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
    // Top-level: Play/Settings/Exit. Host/Find/EnterCode se revelan al tocar Play
    // (si el WBP todavía no tiene PlayButton, quedan visibles como antes — retrocompatible).
    UPROPERTY(meta = (BindWidgetOptional)) UButton* PlayButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* SettingsButton;
    UPROPERTY(meta = (BindWidget))         UButton* QuitButton;

    // Agrupa título+subtítulo de la pantalla principal (se oculta junto con Play/Settings/Exit).
    UPROPERTY(meta = (BindWidgetOptional)) UWidget* MainMenuHeaderPanel;

    UPROPERTY(meta = (BindWidget))         UButton* HostButton;
    UPROPERTY(meta = (BindWidget))         UButton* FindButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* EnterCodeButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* PlayBackButton;

    // Agrupa el título "PLAY" del submenú (se muestra junto con Host/Find/EnterCode/Back).
    UPROPERTY(meta = (BindWidgetOptional)) UWidget* PlaySubmenuHeaderPanel;

    // Sub-paneles opcionales incrustados en el WBP
    UPROPERTY(meta = (BindWidgetOptional)) UPTCreateSessionWidget* CreatePanel;
    UPROPERTY(meta = (BindWidgetOptional)) UPTFindSessionsWidget*  FindPanel;
    UPROPERTY(meta = (BindWidgetOptional)) UPTEnterCodeWidget*     EnterCodePanel;
    UPROPERTY(meta = (BindWidgetOptional)) UPTSettingsWidget*      SettingsPanel;

    // Fase 4 — Texto de error de conexión (ej: "Contraseña incorrecta")
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* ErrorText;

    // Fase 5 — Código de invitación recién generado (sesión privada). Se completa solo
    // cuando CreateSession se hizo con bPrivate=true; queda vacío/oculto en sesión pública.
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* GeneratedCodeText;

    // ------------------------------------------------------------------
    // Handlers de botones
    // ------------------------------------------------------------------
    UFUNCTION() void OnPlayClicked();
    UFUNCTION() void OnPlayBackClicked();
    UFUNCTION() void OnHostClicked();
    UFUNCTION() void OnFindClicked();
    UFUNCTION() void OnEnterCodeClicked();
    UFUNCTION() void OnQuitClicked();
    UFUNCTION() void OnSettingsClicked();

    void SetPlaySubmenuVisible(bool bVisible);

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
