// Copyright Epic Games, Inc. All Rights Reserved.
// HUD del lobby: lista de jugadores, contador, código de sala (si es privada), Leave/Start Game.
// Refresca por timer en vez de enganchar delegates de replicación — la lista es chica
// y no es sensible a performance, así que no vale la pena la plomería extra.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../PTMatchSettings.h"
#include "../UI/PTUserWidget.h"
#include "PTLobbyHUDWidget.generated.h"

class UVerticalBox;
class UTextBlock;
class UButton;
class UPanelWidget;
class UCheckBox;
class UPTGameInstance;

UCLASS()
class MYPARTYGAME_API UPTLobbyHUDWidget : public UPTUserWidget
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

    // Toggle de "listo". El arranque es automático (todos listos → countdown), no hay
    // botón de Start manual en el flujo normal (StartGameButton queda como override de debug).
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      ReadyButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   ReadyButtonText;
    // Casillero: abre el Locker desde el lobby en sesión (al lado de Listo / Game Settings).
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      LockerButton;

    // ── Invitaciones / visibilidad (SOLO host) ──────────────────────────────────
    // Botón "Invitar": abre el panel de amigos (FriendsPanel) o, si no hay, el overlay de Steam.
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      InviteButton;
    // Panel de amigos embebido (deriva de PTFriendsWidget). Arranca oculto; lo muestra InviteButton.
    UPROPERTY(meta = (BindWidgetOptional)) class UPTFriendsWidget* FriendsPanel;
    // Visible solo mientras APTGameState::CountdownSecondsRemaining >= 0.
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   CountdownText;

    UFUNCTION() void OnCopyCodeClicked();
    UFUNCTION() void OnLeaveGameClicked();
    UFUNCTION() void OnStartGameClicked();
    UFUNCTION() void OnReadyClicked();
    UFUNCTION() void OnLockerClicked();
    UFUNCTION() void OnInviteClicked();

    // ── Lista de jugadores (fila = widget propio) + colores de listo/no-listo ──
    UPROPERTY(EditAnywhere, Category = "Lobby") TSubclassOf<class UPTPlayerRowWidget> PlayerRowClass;
    UPROPERTY(EditAnywhere, Category = "Lobby") int32 MaxNameChars = 14;
    UPROPERTY(EditAnywhere, Category = "Lobby") FLinearColor ReadyColor    = FLinearColor(0.10f, 0.80f, 0.12f, 1.f); // verde intenso
    UPROPERTY(EditAnywhere, Category = "Lobby") FLinearColor NotReadyColor = FLinearColor(0.90f, 0.10f, 0.10f, 1.f); // rojo intenso
    // Shadow del texto de ReadyButtonText (configurable en el class default del widget).
    UPROPERTY(EditAnywhere, Category = "Lobby") FLinearColor ReadyTextShadowColor    = FLinearColor(0.f, 0.f, 0.f, 1.f);
    UPROPERTY(EditAnywhere, Category = "Lobby") FLinearColor NotReadyTextShadowColor = FLinearColor(0.f, 0.f, 0.f, 1.f);

    void RefreshPlayerList();

    // ── Config de partida (SOLO host) ──────────────────────────────────────────
    // Botón "Game Settings" (host-only) que ABRE el panel de ajustes.
    UPROPERTY(meta = (BindWidgetOptional)) UButton* GameSettingsButton;
    // Panel de ajustes = su propio widget (deriva de PTGameSettingsWidget). Arranca oculto; lo abre
    // GameSettingsButton. Toda la lógica (steppers/dificultad/categorías/privada) vive ahí.
    UPROPERTY(meta = (BindWidgetOptional)) class UPTGameSettingsWidget* GameSettingsPanel;
    // Biblioteca de bancos de palabras (deriva de PTWordPackWidget). Vive acá (no dentro del settings)
    // para poder acomodarla libre en el canvas. Arranca oculta; la abre el botón "Library Mods" del
    // GameSettings vía su delegate OnRequestLibrary.
    UPROPERTY(meta = (BindWidgetOptional)) class UPTWordPackWidget* LibraryPanel;

    UFUNCTION() void OnGameSettingsClicked(); // abre el panel
    void OnLibraryRequested();                // abre LibraryPanel (lo pide el GameSettings)

    // ── Vista read-only de la config del host (SOLO clientes, y SOLO mientras el host la edita) ──
    // Contenedor del panelcito. Arranca colapsado; se muestra únicamente cuando el host tiene su
    // panel de Game Settings abierto (APTGameState::bHostSettingsPanelOpen) y este jugador NO es host.
    UPROPERTY(meta = (BindWidgetOptional)) UWidget* GameSettingsClientsPanel;
    // Los TextBlock son opcionales: poné en el WBP solo los que quieras mostrar. Se rellenan desde el
    // APTGameState replicado en cada refresco del timer.
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* SV_PrivateText;  // "Sala privada: Sí/No"
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* SV_TurnTimeText; // "Tiempo de turno: 90 s"
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* SV_RoundsText;   // "Rondas: 3"
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* SV_RevealText;   // "Revelado: 30%"
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* SV_WordPackText; // "Biblioteca: X / Default"

    void RefreshSettingsView(); // rellena los SV_* desde APTGameState

private:
    FTimerHandle RefreshTimerHandle;
    FString CachedRoomCode;
};
