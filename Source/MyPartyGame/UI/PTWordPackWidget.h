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
//   WorkshopButton(Button)           → abre el Workshop Browser (buscar/suscribir/publicar) (opcional)
//   TitleText / SelectedText / EmptyText / StatusText (TextBlock) → título / elegido / vacío / estado (opcionales)
// En Details (categoría WordPack) asignar WorkshopBrowserClass = WBP_WorkshopBrowser (deriva de PTWorkshopBrowserWidget).
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
class UPTWorkshopBrowserWidget;

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
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      RefreshButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      DefaultButton;

    // ── Lista de MAPAS (bloqueada por ahora) ──
    // MapsBox queda vacío hasta que estén los mapas custom; MapsLockedPanel muestra "Próximamente".
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* MapsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*      MapsLockedPanel;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   MapsLockedText; // "Mapas — Próximamente"

    // ── Pestañas (como el Locker: Cabeza/Cuerpo → acá Palabras/Mapas) ──
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      WordsTabButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      MapsTabButton;
    UPROPERTY(EditAnywhere, Category="WordPack") FLinearColor TabActiveColor   = FLinearColor(0.95f, 0.25f, 0.55f, 1.f);
    UPROPERTY(EditAnywhere, Category="WordPack") FLinearColor TabInactiveColor = FLinearColor(0.20f, 0.45f, 0.75f, 1.f);

    UPROPERTY(meta = (BindWidgetOptional)) UButton*      BackButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   TitleText;

    // Abre el Workshop Browser (buscar/suscribir/publicar items) desde la Biblioteca.
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      WorkshopButton;
    // WBP del Browser (deriva de PTWorkshopBrowserWidget). Asignar en Details (categoría WordPack).
    UPROPERTY(EditAnywhere, Category="WordPack") TSubclassOf<UPTWorkshopBrowserWidget> WorkshopBrowserClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="WordPack")
    TSubclassOf<UPTWordPackRowWidget> RowWidgetClass;

    UFUNCTION() void OnRefreshClicked();
    UFUNCTION() void OnDefaultClicked();
    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnWordsTabClicked();
    UFUNCTION() void OnMapsTabClicked();
    UFUNCTION() void OnWorkshopClicked();

private:
    void Rebuild();
    void OnPacksUpdated();
    void SwitchTab(int32 Tab);   // 0 = Palabras (funcional), 1 = Mapas (bloqueado)
    void ApplyTabVisual();
    UPTWordPackSubsystem* Packs() const;
    UPTGameInstance*      GI() const;

    int32 ActiveTab = 0;
    bool bBound = false;

    UPROPERTY(Transient) UPTWorkshopBrowserWidget* WorkshopBrowser = nullptr; // creado una vez y reusado
};
