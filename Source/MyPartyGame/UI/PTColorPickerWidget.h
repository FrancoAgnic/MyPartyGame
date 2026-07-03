#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTColorPickerWidget.generated.h"

class UImage;
class USlider;
class UTextBlock;
class UBorder;
class UButton;
class UPanelWidget;

/**
 * Color picker estilo rueda HSV.
 *  - Wheel (Image): rueda HSV. Click/drag elige matiz (ángulo) y saturación (radio).
 *  - ValueSlider (Slider vertical): brillo (V).
 *  - HexText (TextBlock): muestra el hex del color actual.
 *  - PreviewSwatch (Border): muestra el color actual.
 *  - ConfirmButton (Button): confirma y cierra (pasa a modo Paint).
 *
 * Reparentar el WBP a esta clase y nombrar los widgets igual (meta=BindWidget).
 * Los que son BindWidgetOptional pueden faltar.
 */
UCLASS()
class MYPARTYGAME_API UPTColorPickerWidget : public UUserWidget
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

protected:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseMove     (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
    virtual FReply NativeOnMouseButtonUp (const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    UPROPERTY(meta=(BindWidget))          UImage*      Wheel         = nullptr;
    UPROPERTY(meta=(BindWidgetOptional))  USlider*     ValueSlider   = nullptr;
    UPROPERTY(meta=(BindWidgetOptional))  UTextBlock*  HexText       = nullptr;
    UPROPERTY(meta=(BindWidgetOptional))  UBorder*     PreviewSwatch = nullptr;
    UPROPERTY(meta=(BindWidgetOptional))  UButton*     ConfirmButton = nullptr;
    /** Contenedor (Horizontal/Wrap Box) donde se agregan los swatches guardados. */
    UPROPERTY(meta=(BindWidgetOptional))  UPanelWidget* SwatchBox    = nullptr;
    /** Botón "+" para guardar el color actual como swatch. */
    UPROPERTY(meta=(BindWidgetOptional))  UButton*      AddButton    = nullptr;

private:
    float Hue = 0.f, Sat = 0.f; // guardados para recomputar al mover el value slider
    bool  bDragging = false;

    // Paleta de colores guardados (persiste en disco entre sesiones).
    static constexpr int32 MaxSwatches = 11; // buffer rotativo: al pasarse cae el más viejo
    TArray<FLinearColor> Palette;
    UPROPERTY() TArray<UButton*> SwatchButtons;
    UPROPERTY() TArray<UWidget*> SwatchWrappers; // los SizeBox contenedores (para limpiar)

    UFUNCTION() void OnValueChanged(float V);
    UFUNCTION() void OnAddClicked();
    UFUNCTION() void OnSwatchClicked();

    void LoadPalette();
    void SavePalette() const;
    void BuildSwatches();
    void ClearSwatches();
    void AddSwatchButton(const FLinearColor& C);

    // Devuelve true si el click cae dentro de la rueda; actualiza Hue/Sat.
    bool UpdateFromMouse(const FPointerEvent& InMouseEvent);
    void RecomputeColor();
    void RefreshUI();
};
