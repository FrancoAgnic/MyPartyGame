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
class UTexture2D;
class UPTToolSlotWidget;
class APTLobbyPlayerController;

UCLASS()
class MYPARTYGAME_API UPTHeadSculptHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Refresca resaltados, cruz WASD e icono de fuera-de-área. La primera vez arma los cuadritos. */
    void Refresh(APTLobbyPlayerController* PC);

protected:
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* ToolsBox;
    UPROPERTY(meta = (BindWidgetOptional)) UPanelWidget* ShapesBox;

    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* ColorSlot;
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* ExitSlot;
    // SHIFT: alternar pintar cabeza ↔ cuerpo. Solo visible con la herramienta Paint.
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* PaintBodySlot;
    // BACKSPACE: toque = deshacer; mantener = borrar todo (muestra el círculo de progreso 3s).
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* ClearSlot;

    // Cruz de cámara (WASD): brillan mientras mantenés la tecla.
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* WasdUp;    // W
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* WasdDown;  // S
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* WasdLeft;  // A
    UPROPERTY(meta = (BindWidgetOptional)) UPTToolSlotWidget* WasdRight; // D

    /** 🚫 "no se puede esculpir acá": visible cuando la brocha sale del área de modelado. */
    UPROPERTY(meta = (BindWidgetOptional)) UImage* OutOfBoundsIcon;

    /** Leyenda de controles por texto (opcional; alternativa simple a la cruz WASD). */
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* ControlsText;

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

private:
    void BuildOnce();          // arma tools + shapes + slots contextuales (una sola vez)
    void RefreshControlsText();

    UPROPERTY() TArray<UPTToolSlotWidget*> ToolSlots;
    UPROPERTY() TArray<UPTToolSlotWidget*> ShapeSlots;
    bool bBuilt = false;
};
