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

    /** Poblar la lista con los resultados del subsistema. Llamar desde UPTMainMenuWidget::OnFindSessions. */
    void PopulateResults(const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidget)) UScrollBox* ResultsBox;
    UPROPERTY(meta = (BindWidget)) UButton*    RefreshButton;
    UPROPERTY(meta = (BindWidget)) UButton*    BackButton;

    /** Clase del widget de fila. Asignar en el WBP derivado. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sessions")
    TSubclassOf<UPTSessionRowWidget> RowWidgetClass;

    UFUNCTION() void OnRefreshClicked();
    UFUNCTION() void OnBackClicked();

private:
    UPROPERTY() UMultiplayerSessionsSubsystem* Sessions = nullptr;
};
