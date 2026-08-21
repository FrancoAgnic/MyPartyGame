// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 2 — Panel de búsqueda de sesiones con lista de resultados.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSessionSettings.h"
#include "PTFindSessionsWidget.generated.h"

class UScrollBox;
class UButton;
class UTextBlock;
class UPTSessionRowWidget;
class UMultiplayerSessionsSubsystem;

UCLASS()
class MYPARTYGAME_API UPTFindSessionsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Mostrar el panel de búsqueda. */
    UFUNCTION(BlueprintCallable, Category = "Sessions")
    void ShowPanel();

    /**
     * Poblar la lista con los resultados del subsistema. Llamar desde UPTMainMenuWidget::OnFindSessions.
     * Fase 5 — solo pinta sesiones públicas (sin código); las privadas se unen por OnJoinByCodeClicked.
     */
    void PopulateResults(const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful);

protected:
    virtual bool Initialize() override;

    // Lista de la pestaña PÚBLICAS (partidas públicas de cualquiera).
    UPROPERTY(meta = (BindWidget))         UScrollBox* ResultsBox;
    // Lista de la pestaña AMIGOS (partidas privadas-solo-amigos creadas por tus amigos). Opcional:
    // si no existe en el WBP, solo funciona la pestaña públicas.
    UPROPERTY(meta = (BindWidgetOptional)) UScrollBox* FriendsBox;

    UPROPERTY(meta = (BindWidget))         UButton*    RefreshButton;
    UPROPERTY(meta = (BindWidget))         UButton*    BackButton;
    // Pestañas (opcionales): alternan entre la lista pública y la de amigos.
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    PublicTabButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    FriendsTabButton;
    // Textos "no hay partidas" por lista (opcionales).
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* EmptyPublicText;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* EmptyFriendsText;
    // Botón "Reconectar a la partida anterior": aparece solo si hay una última partida.
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    ReconnectButton;

    // Colores de pestaña activa/inactiva.
    UPROPERTY(EditAnywhere, Category = "Sessions") FLinearColor TabActiveColor   = FLinearColor(0.95f, 0.25f, 0.55f, 1.f);
    UPROPERTY(EditAnywhere, Category = "Sessions") FLinearColor TabInactiveColor = FLinearColor(0.20f, 0.45f, 0.75f, 1.f);

    /** Clase del widget de fila. Asignar en el WBP derivado. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sessions")
    TSubclassOf<UPTSessionRowWidget> RowWidgetClass;

    UFUNCTION() void OnRefreshClicked();
    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnReconnectClicked();
    UFUNCTION() void OnPublicTabClicked();
    UFUNCTION() void OnFriendsTabClicked();

private:
    void RefreshList();          // ReadFriends + FindSessions
    void SwitchTab(int32 Tab);
    void ApplyTabVisual();
    void UpdateEmptyLabels();    // muestra el cartel "no hay partidas" de la pestaña activa si está vacía

    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
    int32 ActiveTab       = 0; // 0 = públicas, 1 = amigos
    int32 LastPublicCount  = 0;
    int32 LastFriendsCount = 0;
};
