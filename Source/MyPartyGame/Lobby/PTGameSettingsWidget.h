// Copyright Epic Games, Inc. All Rights Reserved.
// Panel de configuración de partida (SOLO host), como widget aparte. Antes vivía dentro de
// WBP_LobbyHUD; ahora es su propio WBP y el HUD solo instancia una copia y la abre con un botón.
// Escribe directo en UPTGameInstance::PendingMatchSettings (que sobrevive el viaje a Lvl-01).
//
// En el WBP derivado (parent PTGameSettingsWidget) — nombres EXACTOS, casi todo opcional:
//   FriendsOnlyCheckbox (CheckBox) → "Private Room" (tildado = privada solo amigos)
//   TurnTimeText/TurnTimeMinus/TurnTimePlus
//   RoundsText/RoundsMinus/RoundsPlus
//   RevealText/RevealMinus/RevealPlus
//   DiffFacilButton/DiffMediaButton/DiffDificilButton  (dificultades; coloreadas si activas)
//   CategoriesBox (Panel)  → C++ lo llena con un check por categoría
//   LibraryButton (Button) → abre la Biblioteca (LibraryPanel)
//   LibraryPanel  (instancia de WBP_WordPack)  → arranca oculto
//   CloseButton   (Button) → cerrar (Back/Apply; los ajustes se aplican en vivo)
//   TitleText (TextBlock)

#pragma once
#include "CoreMinimal.h"
#include "../UI/PTUserWidget.h"
#include "../PTMatchSettings.h" // EPTWordDifficulty
#include "PTGameSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class UTextBlock;
class UPanelWidget;
class UPTWordPackWidget;
class UPTGameInstance;

UCLASS()
class MYPARTYGAME_API UPTGameSettingsWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Mostrar el panel: construye los checks de categoría (una vez), refresca valores y hace el blop. */
    UFUNCTION(BlueprintCallable, Category="GameSettings")
    void ShowPanel();

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidgetOptional)) UCheckBox*  FriendsOnlyCheckbox;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TurnTimeText;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    TurnTimeMinus;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    TurnTimePlus;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* RoundsText;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    RoundsMinus;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    RoundsPlus;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* RevealText;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    RevealMinus;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    RevealPlus;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    DiffFacilButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    DiffMediaButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    DiffDificilButton;
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* CategoriesBox;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    LibraryButton;
    UPROPERTY(meta = (BindWidgetOptional)) UPTWordPackWidget* LibraryPanel;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    CloseButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* TitleText;

    UFUNCTION() void OnTurnTimeMinus(); UFUNCTION() void OnTurnTimePlus();
    UFUNCTION() void OnRoundsMinus();   UFUNCTION() void OnRoundsPlus();
    UFUNCTION() void OnRevealMinus();   UFUNCTION() void OnRevealPlus();
    UFUNCTION() void OnDiffFacil();     UFUNCTION() void OnDiffMedia();  UFUNCTION() void OnDiffDificil();
    UFUNCTION() void OnCategoryChanged(bool bChecked);
    UFUNCTION() void OnFriendsOnlyChanged(bool bIsChecked);
    UFUNCTION() void OnLibraryClicked();
    UFUNCTION() void OnCloseClicked();

private:
    UPTGameInstance* GetGI() const;
    void BuildCategoryChecks();
    void RefreshUI();
    void ToggleDifficulty(EPTWordDifficulty Diff);

    UPROPERTY() TArray<UCheckBox*> CategoryChecks;
    TArray<FName>                  CategoryNames;
    bool bCategoryChecksBuilt = false;
};
