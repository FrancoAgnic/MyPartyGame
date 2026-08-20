#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTUserWidget.h"
#include "PTColorPickerWidget.generated.h"

class UImage;
class USlider;
class UTextBlock;
class UBorder;
class UButton;
class UPTColorRingWidget;

/**
 * Color picker estilo rueda HSV (se abre manteniendo el click derecho).
 *  - Wheel (Image, obligatorio): rueda HSV. Arrastrar elige matiz (ángulo) y saturación (radio).
 *  - ValueSlider (Slider, opcional): brillo (V); también se ajusta con la rueda del mouse.
 *  - PreviewSwatch (Border, opcional): muestra el color actual.
 *  - SwatchRing (UPTColorRingWidget, opcional): dona segmentada con los colores guardados,
 *    concéntrica con la rueda. Guardar = E; elegir = soltar el RMB sobre un segmento.
 *  - SaveHintText (TextBlock, opcional): C++ le pone "E — Save Color".
 *
 * Reparentar el WBP a esta clase y nombrar los widgets igual (meta=BindWidget).
 * Los que son BindWidgetOptional pueden faltar.
 */
UCLASS()
class MYPARTYGAME_API UPTColorPickerWidget : public UPTUserWidget
{
    GENERATED_BODY()
public:
    /** Color actualmente seleccionado. */
    UPROPERTY(BlueprintReadOnly, Category="ColorPicker")
    FLinearColor CurrentColor = FLinearColor::White;

    /** Setea el color (para swatches de la paleta) y refresca la UI. */
    UFUNCTION(BlueprintCallable, Category="ColorPicker")
    void SetColor(FLinearColor NewColor);

    /** Confirma el color actual: avisa al PlayerController y cierra. */
    UFUNCTION(BlueprintCallable, Category="ColorPicker")
    void Confirm();

    // ── Selección rápida (mantener RMB): el PlayerController abre/cierra y va tickeando ──
    /** Actualiza matiz/saturación desde la posición actual del cursor (llamar por tick). */
    void QuickPickTick();
    /** Sube/baja el brillo (V) del color (rueda del mouse con RMB apretado). */
    void QuickAdjustValue(float Delta);

    /** Al soltar el RMB: si el cursor está sobre un swatch guardado usa ese color; si no,
     *  confirma el color de la rueda. Luego cierra. */
    void ConfirmQuickPick();

    /** Guarda el color actual en la paleta persistente (lo llama el PlayerController con E). */
    void SaveCurrentColor();

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove     (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    UPROPERTY(meta=(BindWidget))          UImage*      Wheel         = nullptr;
    UPROPERTY(meta=(BindWidgetOptional))  USlider*     ValueSlider   = nullptr;
    UPROPERTY(meta=(BindWidgetOptional))  UBorder*     PreviewSwatch = nullptr;
    UPROPERTY(meta=(BindWidgetOptional))  UButton*     ConfirmButton = nullptr;
    /** Anillo de colores guardados (dona segmentada). Colocar concéntrico con la rueda. */
    UPROPERTY(meta=(BindWidgetOptional))  UPTColorRingWidget* SwatchRing = nullptr;
    /** Texto de ayuda "E — Save Color" (lo setea C++). */
    UPROPERTY(meta=(BindWidgetOptional))  UTextBlock*   SaveHintText = nullptr;

private:
    float Hue = 0.f, Sat = 0.f, Val = 1.f; // HSV actual (Val = brillo; el slider es opcional)
    bool  bDragging = false;

    // Edición del BRILLO (Value) de un color guardado con la rueda del mouse: mientras el cursor
    // está sobre un swatch, la rueda le sube/baja el value (y el ValueSlider lo muestra en vivo);
    // al soltar el RMB se guarda como un color NUEVO (distinto al original).
    int32 EditingSeg   = -1;    // índice del swatch que se está editando (-1 = ninguno)
    bool  bSwatchEdited = false; // true si ya se modificó su value (para no resetearlo en el tick)

    // Actualiza matiz/saturación desde una posición ABSOLUTA de pantalla (Slate).
    bool UpdateFromAbsolute(FVector2D AbsPos);

    // Paleta de colores guardados (persiste en disco entre sesiones).
    static constexpr int32 MaxSwatches = 11; // buffer rotativo: al pasarse cae el más viejo
    TArray<FLinearColor> Palette;

    UFUNCTION() void OnValueChanged(float V);

    void PushLiveColorToPC(); // aplica el color actual a la brocha en vivo (mientras arrastrás)

    void PlayPickSound();          // sonido al elegir/guardar color (dedupe interno)
    float LastPickSoundTime = -1.f;

    void LoadPalette();
    void SavePalette() const;
    void RefreshRing();       // vuelca la paleta al anillo (SwatchRing)

    // Devuelve true si el click cae dentro de la rueda; actualiza Hue/Sat.
    bool UpdateFromMouse(const FPointerEvent& InMouseEvent);
    void RecomputeColor();
    void RefreshUI();
};
