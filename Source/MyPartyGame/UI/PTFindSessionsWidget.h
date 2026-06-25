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
class UEditableTextBox;
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

    UPROPERTY(meta = (BindWidget)) UScrollBox* ResultsBox;
    UPROPERTY(meta = (BindWidget)) UButton*    RefreshButton;
    UPROPERTY(meta = (BindWidget)) UButton*    BackButton;

    // Fase 5 — pestaña "unirse con código": pegar el código y unirse directo, sin lista.
    UPROPERTY(meta = (BindWidget)) UEditableTextBox* CodeInput;
    UPROPERTY(meta = (BindWidget)) UButton*          JoinByCodeButton;

    /** Clase del widget de fila. Asignar en el WBP derivado. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sessions")
    TSubclassOf<UPTSessionRowWidget> RowWidgetClass;

    UFUNCTION() void OnRefreshClicked();
    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnJoinByCodeClicked();

private:
    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
};
