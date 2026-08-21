// Copyright Epic Games, Inc. All Rights Reserved.
// Contenedor del Locker estilo "outfits": personaje a un lado (3D), y a la derecha PESTAÑAS (Cabeza /
// Cuerpo) con sus slots. Se navega con flechas, se cambia de pestaña con TAB (o click en el nombre), y
// abajo a la derecha hay botones globales: Asignar (Enter), Editar (G), Back (Esc) — que operan sobre
// el slot SELECCIONADO.
//
// En el WBP derivado (nombres EXACTOS; casi todo opcional):
//   HeadSlotsBox / BodySlotsBox (Panel)     → contenedores de los slots (los llena el código)
//   HeadTabButton / BodyTabButton (Button)  → pestañas (click cambia de ventana)
//   AssignButton / EditActionButton / BackButton (Button) → barra de acciones (abajo a la derecha)
//   AssignLabel (TextBlock)                 → texto del botón Asignar ("Asignar" / "Crear")
// En Details (categoría Locker) asignar SlotWidgetClass = WBP del slot (deriva de PTLockerSlotWidget).

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "../UI/PTUserWidget.h"
#include "PTLockerWidget.generated.h"

class UPanelWidget;
class UButton;
class UTextBlock;
class UPTLockerSlotWidget;
class UPTLockerSubsystem;
class APTLobbyPlayerController;

UCLASS()
class MYPARTYGAME_API UPTLockerWidget : public UPTUserWidget
{
    GENERATED_BODY()
public:
    /** Reconstruye/actualiza el estado de todos los slots + resaltado + botones. */
    void RefreshSlots();
    /** Selecciona un slot (lo llama el tile al clickearlo). */
    void SelectSlot(int32 Index, bool bHead);

    /** Hover del mouse sobre un slot LLENO: lo selecciona y previsualiza esa skin en el personaje. */
    void HoverSlot(int32 Index, bool bHead);
    /** El mouse salió del slot (sin equipar): vuelve el personaje a lo equipado. */
    void EndHoverPreview();
    /** Click izquierdo en un slot LLENO: equipa esa skin directamente. */
    void EquipSlotNow(int32 Index, bool bHead);
    /** Click izquierdo en un slot VACÍO: entra directo a crearlo (sin botón Crear). */
    void CreateSlotNow(int32 Index, bool bHead);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    virtual bool NativeSupportsKeyboardFocus() const override { return true; }

    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* HeadSlotsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* BodySlotsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      HeadTabButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      BodyTabButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      AssignButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      EditActionButton;
    UPROPERTY(meta = (BindWidgetOptional)) UButton*      BackButton;
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*   AssignLabel;

    UPROPERTY(EditAnywhere, Category = "Locker") TSubclassOf<UPTLockerSlotWidget> SlotWidgetClass;
    // Columnas por fila cuando el contenedor es un Uniform Grid Panel (para acomodar los 6 slots en grilla).
    UPROPERTY(EditAnywhere, Category = "Locker") int32 SlotsPerRow = 3;
    // Color de la pestaña ACTIVA vs. inactiva (se aplica al fondo del botón de la pestaña).
    UPROPERTY(EditAnywhere, Category = "Locker") FLinearColor TabActiveColor   = FLinearColor(0.95f, 0.25f, 0.55f, 1.f); // rosa
    UPROPERTY(EditAnywhere, Category = "Locker") FLinearColor TabInactiveColor = FLinearColor(0.20f, 0.45f, 0.75f, 1.f); // azul

    UFUNCTION() void OnHeadTabClicked();
    UFUNCTION() void OnBodyTabClicked();
    UFUNCTION() void OnAssignClicked();
    UFUNCTION() void OnEditClicked();
    UFUNCTION() void OnBackClicked();

private:
    void BuildSlots();
    void SwitchTab(int32 Tab);          // 0 = Cabeza, 1 = Cuerpo
    void ApplyTabVisual();              // tiñe el botón de la pestaña activa
    void MoveSelection(int32 DX, int32 DY); // navegar en grilla (direccional, con wrap al borde opuesto)
    void ApplySelectionVisual();        // resaltar el slot seleccionado + refrescar botones
    void ActivateSelected();            // Asignar/Crear (Enter)
    void EditSelected();                // Editar el slot seleccionado (lo usa CreateSlotNow para slots vacíos)
    void EditEquipped();                // Editar SIEMPRE la skin equipada (botón Editar / G)

    UPTLockerSubsystem*      Locker()  const;
    APTLobbyPlayerController* LobbyPC() const;
    TArray<UPTLockerSlotWidget*>& ActiveList();
    int32 ActiveCount() const;

    UPROPERTY() TArray<UPTLockerSlotWidget*> HeadSlotWidgets;
    UPROPERTY() TArray<UPTLockerSlotWidget*> BodySlotWidgets;
    int32 ActiveTab     = 0; // 0 cabeza, 1 cuerpo
    int32 SelectedIndex = 0;
    bool  bBuilt = false;
    bool  bPreviewingHover = false; // true mientras un slot no-equipado está en preview por hover
};
