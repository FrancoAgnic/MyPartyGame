// Copyright Epic Games, Inc. All Rights Reserved.
// Panel del lobby (SOLO host) para elegir un banco de palabras de la comunidad y publicar el propio.
// Lista los bancos que trae UPTWordPackSubsystem (locales + Workshop suscrito).
//
// En el WBP derivado (nombres EXACTOS; casi todo opcional):
//   PacksBox      (ScrollBox/Panel)  → contenedor de filas (lo llena el código)
//   RefreshButton (Button)           → re-escanea bancos (opcional)
//   PublishButton (Button)           → elegir un CSV y subirlo al Workshop (opcional)
//   DefaultButton (Button)           → volver al banco por defecto del juego (opcional)
//   BackButton    (Button)           → cerrar el panel (opcional)
//   TitleText / SelectedText / EmptyText / StatusText (TextBlock) → título / elegido / vacío / estado (opcionales)
// En Details (categoría WordPack) asignar RowWidgetClass = WBP de la fila (deriva de PTWordPackRowWidget).

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTWordPackWidget.generated.h"

class UPanelWidget;
class UButton;
class UTextBlock;
class UPTWordPackRowWidget;
class UPTWordPackSubsystem;
class UPTGameInstance;

UCLASS()
class MYPARTYGAME_API UPTWordPackWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Mostrar el panel y re-escanear bancos. */
    UFUNCTION(BlueprintCallable, Category="WordPack") void ShowPanel();
    /** Elegir un banco (lo llama la fila). */
    void UsePack(const FString& PackId);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // ── Lista de BANCOS DE PALABRAS (funcional) ──
    UPROPERTY(meta = (BindWidget))         UPanelWidget* PacksBox;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   SelectedText;   // banco elegido ("En uso: X")
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   EmptyText;      // "no hay bancos"
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   StatusText;     // resultado de publicar
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      RefreshButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      PublishButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      DefaultButton;

    // ── Lista de MAPAS (bloqueada por ahora) ──
    // MapsBox queda vacío hasta que estén los mapas custom; MapsLockedPanel muestra "Próximamente".
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* MapsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*      MapsLockedPanel;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   MapsLockedText; // "Mapas — Próximamente"

    UPROPERTY(meta = (BindWidgetOptional)) UButton*      BackButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   TitleText;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WordPack")
    TSubclassOf<UPTWordPackRowWidget> RowWidgetClass;

    UFUNCTION() void OnRefreshClicked();
    UFUNCTION() void OnPublishClicked();
    UFUNCTION() void OnDefaultClicked();
    UFUNCTION() void OnBackClicked();

private:
    void Rebuild();
    void OnPacksUpdated();
    void OnPublished(bool bOk, const FString& Info);
    UPTWordPackSubsystem* Packs() const;
    UPTGameInstance*      GI() const;

    bool bBound = false;
};
