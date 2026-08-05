// Copyright Epic Games, Inc. All Rights Reserved.
// Hotbar del modo "esculpir tu cabeza" (tecla G en el lobby). Parecida a la del gameplay, pero con
// los controles PROPIOS del modo G (WASD orbita la cámara en vez de volar, G aplica y sale).
//
// Reusa WBP_ToolSlot (el mismo cuadrito del gameplay). En el WBP derivado, todo es OPCIONAL: poné
// lo que quieras mostrar y nombralo EXACTO:
//   ToolsBox   (HorizontalBox) → 4 herramientas (1/2/3/4)
//   ShapesBox  (HorizontalBox) → 4 formas (TAB); se oculta con Ojos
//   ColorSlot  (WBP_ToolSlot)  → elegir color (click derecho)
//   ExitSlot   (WBP_ToolSlot)  → G: aplicar y salir
//   WasdUp/WasdDown/WasdLeft/WasdRight (WBP_ToolSlot) → cruz de cámara; brillan al mantener la tecla
//   OutOfBoundsIcon (Image)    → 🚫 cuando la brocha sale del área
//   ControlsText (TextBlock)   → leyenda de texto (alternativa a la cruz WASD)
// En Details (categoría HeadHUD) se asignan ToolSlotClass + los iconos (los mismos del gameplay).

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTHeadSculptHUDWidget.generated.h"

class UPanelWidget;
class UTextBlock;
class UImage;
class UProgressBar;
class UMaterialInterface;
class UTexture2D;
class UButton;
class UPTToolSlotWidget;
class APTLobbyPlayerController;

UCLASS()
class MYPARTYGAME_API UPTHeadSculptHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Refresca resaltados, cruz WASD e icono de fuera-de-área. La primera vez arma los cuadritos. */
    void Refresh(APTLobbyPlayerController* PC);
    /** Muestra/oculta el popup "¿Guardar los cambios?" (Sí = guardar, No = descartar). */
    void ShowDiscardPopup(bool bShow);

protected:
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* ToolsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* ShapesBox;

    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* ColorSlot;
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* ExitSlot;
    // SHIFT: alternar pintar cabeza ↔ cuerpo. Solo visible con la herramienta Paint.
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* PaintBodySlot;
    // BACKSPACE: toque = deshacer; mantener = borrar todo (muestra el círculo de progreso 3s).
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* ClearSlot;
    // Barra de presupuesto de pintura: solo visible con la herramienta Paint (cabeza o cuerpo). Se
    // llena al pintar; roja al llegar al límite. Nombrarla EXACTO "PaintBudgetBar" en el WBP.
    UPROPERTY(meta = (BindWidgetOptional)) UProgressBar* PaintBudgetBar;
    // Texto de la barra: dice "Pintura" y cambia a "¡Sin pintura!" (rojo) al llegar al límite.
    // Nombrarlo EXACTO "PaintBudgetText" en el WBP. Solo visible con la herramienta Paint.
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* PaintBudgetText;

    // Cruz de cámara (WASD): brillan mientras mantenés la tecla.
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* WasdUp;    // W
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* WasdDown;  // S
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* WasdLeft;  // A
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* WasdRight; // D

    /** 🚫 "no se puede esculpir acá": visible cuando la brocha sale del área de modelado. */
    UPROPERTY(meta = (BindWidgetOptional)) UImage* OutOfBoundsIcon;

    /** Leyenda de controles por texto (opcional; alternativa simple a la cruz WASD). */
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* ControlsText;

    // ── Confirmar / Back / popup de descartar — son WBP_ToolSlot (solo visual, se opera por TECLAS) ──
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* ConfirmSlot;  // Enter: guarda y equipa
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* BackSlot;     // Escape: abre el popup
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*           DiscardPopup; // panel del popup (oculto por defecto)
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* PopupYesSlot; // Enter = Sí (guardar) — visual
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* PopupNoSlot;  // Escape = No (descartar) — visual
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock*        PopupText;    // texto del popup
    // Botones CLICKEABLES del popup (opción con mouse; conviven con Enter/Esc). Nombralos EXACTO en
    // el WBP: "PopupApplyButton" / "PopupDiscardButton". Sus textos se setean desde acá (localizados),
    // pero si preferís poner el texto a mano en el WBP, dejá los TextBlock sin nombrar.
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    PopupApplyButton;   // "Aplicar y equipar" (guarda)
    UPROPERTY(meta = (BindWidgetOptional)) UButton*    PopupDiscardButton; // "No guardar" (descarta)
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* PopupApplyText;     // texto dentro del botón aplicar
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* PopupDiscardText;   // texto dentro del botón descartar

    UFUNCTION() void OnPopupApplyClicked();   // → ResolveHeadBack(true)  (guardar y equipar)
    UFUNCTION() void OnPopupDiscardClicked(); // → ResolveHeadBack(false) (descartar)
    // Iconos de las teclas (asignar en Details): Enter para confirmar/Sí, Esc para back/No.
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconEnter = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconEsc   = nullptr;

    /** WBP del cuadrito (el mismo WBP_ToolSlot del gameplay). */
    UPROPERTY(EditAnywhere, Category = "HeadHUD") TSubclassOf<UPTToolSlotWidget> ToolSlotClass;

    // Iconos de herramientas (asignar las MISMAS texturas del gameplay).
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconAdd      = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconErase    = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconPaint    = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconEyes     = nullptr;
    // Iconos de formas.
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconSphere   = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconCube     = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconCylinder = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconCone     = nullptr;
    // Iconos de acciones.
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconColor    = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconExit     = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconPaintBody = nullptr;
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UTexture2D* IconClear    = nullptr;
    // Material de "pulse" (MUI_Pulse): se aplica al texto "¡Sin pintura!" para que parpadee y se note.
    UPROPERTY(EditAnywhere, Category = "HeadHUD") UMaterialInterface* PulseMaterial = nullptr;

private:
    void BuildOnce();          // arma tools + shapes + slots contextuales (una sola vez)
    void RefreshControlsText();

    UPROPERTY() TArray<UPTToolSlotWidget*> ToolSlots;
    UPROPERTY() TArray<UPTToolSlotWidget*> ShapeSlots;
    bool bBuilt = false;
};
