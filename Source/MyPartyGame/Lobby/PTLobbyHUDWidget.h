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
    // Visible solo mientras APTGameState::CountdownSecondsRemaining >= 0.
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   CountdownText;

    UFUNCTION() void OnCopyCodeClicked();
    UFUNCTION() void OnLeaveGameClicked();
    UFUNCTION() void OnStartGameClicked();
    UFUNCTION() void OnReadyClicked();
    UFUNCTION() void OnLockerClicked();

    // ── Lista de jugadores (fila = widget propio) + colores de listo/no-listo ──
    UPROPERTY(EditAnywhere, Category = "Lobby") TSubclassOf<class UPTPlayerRowWidget> PlayerRowClass;
    UPROPERTY(EditAnywhere, Category = "Lobby") int32 MaxNameChars = 14;
    UPROPERTY(EditAnywhere, Category = "Lobby") FLinearColor ReadyColor    = FLinearColor(0.10f, 0.80f, 0.12f, 1.f); // verde intenso
    UPROPERTY(EditAnywhere, Category = "Lobby") FLinearColor NotReadyColor = FLinearColor(0.90f, 0.10f, 0.10f, 1.f); // rojo intenso

    void RefreshPlayerList();

    // ── Config de partida (SOLO host) ──────────────────────────────────────────
    // Todo BindWidgetOptional: el WBP se arma aparte. Se muestran solo si el jugador es el host.
    // Botón "Game Settings" (al lado del Listo, host-only) que ABRE el panel.
    UPROPERTY(meta = (BindWidgetOptional)) UButton* GameSettingsButton;
    // Botón "X" dentro del panel que lo CIERRA (los ajustes ya se aplican en vivo).
    UPROPERTY(meta = (BindWidgetOptional)) UButton* CloseSettingsButton;
    // Panel raíz que agrupa toda la config. Arranca oculto; se abre con GameSettingsButton.
    UPROPERTY(meta = (BindWidgetOptional)) UWidget* HostSettingsPanel;

    // Steppers numéricos: cada uno un texto + botón menos/más.
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TurnTimeText;   // "90 s"
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    TurnTimeMinus;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    TurnTimePlus;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* RoundsText;     // "3 rondas"
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    RoundsMinus;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    RoundsPlus;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* RevealText;     // "70%"
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    RevealMinus;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    RevealPlus;

    // Dificultad: 3 toggles multi-selección (activa = coloreado). Vacío = todas.
    UPROPERTY(meta = (BindWidgetOptional)) UButton* DiffFacilButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* DiffMediaButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton* DiffDificilButton;

    // Categorías: contenedor (VerticalBox/ScrollBox) que C++ llena con un checkbox por categoría.
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* CategoriesBox;

    // CSV propio del host.
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    LoadCSVButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    ClearCSVButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* CSVStatusText;  // "Banco default (100)" / "CSV: 42 palabras"

    UFUNCTION() void OnTurnTimeMinus(); UFUNCTION() void OnTurnTimePlus();
    UFUNCTION() void OnRoundsMinus();   UFUNCTION() void OnRoundsPlus();
    UFUNCTION() void OnRevealMinus();   UFUNCTION() void OnRevealPlus();
    UFUNCTION() void OnDiffFacil();     UFUNCTION() void OnDiffMedia();  UFUNCTION() void OnDiffDificil();
    UFUNCTION() void OnLoadCSV();       UFUNCTION() void OnClearCSV();
    UFUNCTION() void OnCategoryChanged(bool bChecked); // rebuild desde el estado de todos los checks
    UFUNCTION() void OnGameSettingsClicked(); // abre el panel
    UFUNCTION() void OnCloseSettingsClicked(); // aplica (ya está en vivo) y cierra el panel

    UPTGameInstance* GetGI() const;
    void BuildCategoryChecks();     // crea los checkboxes una vez (según PTDefaultWordCategories)
    void RefreshHostSettingsUI();   // vuelca PendingMatchSettings a los textos/colores
    void ToggleDifficulty(EPTWordDifficulty Diff);

private:
    FTimerHandle RefreshTimerHandle;
    FString CachedRoomCode;

    // Checkbox ↔ categoría (para reconstruir ActiveCategories al cambiar cualquiera).
    UPROPERTY() TArray<UCheckBox*> CategoryChecks;
    TArray<FName>                  CategoryNames;
    bool bCategoryChecksBuilt = false;
    bool bSettingsOpen        = false; // panel de config abierto/cerrado (host)
};
