// Copyright Epic Games, Inc. All Rights Reserved.
// Selector de color simple y reusable: una RUEDA de matiz (elegís el tono girando/clickeando) + 2 sliders
// verticales (Saturación y Valor). Emite OnColorChanged en vivo. Pensado para el panel de ambiente.
//
// En el WBP derivado (parent = PTColorWheelWidget), nombres EXACTOS:
//   Wheel        (Image, obligatorio)  → textura de rueda de matiz; clic/arrastre elige el HUE por ángulo
//   SatSlider    (Slider, opcional)    → saturación (0..1), vertical
//   ValueSlider  (Slider, opcional)    → valor/brillo (0..1), vertical
//   Preview      (Image, opcional)     → se tiñe con el color actual
//   Dot          (Image, opcional)     → puntito que marca DÓNDE quedó el color en la rueda; sigue al
//                                        mouse al arrastrar. IMPORTANTE: poné Wheel y Dot en un MISMO
//                                        Canvas Panel, con el Wheel anclado a llenar (offsets 0,0).

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTColorWheelWidget.generated.h"

class UImage;
class USlider;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FPTOnColorWheelChanged, FLinearColor, Color);

UCLASS()
class MYPARTYGAME_API UPTColorWheelWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Se dispara en vivo cada vez que cambia el color (rueda o sliders). */
    UPROPERTY(BlueprintAssignable, Category="Color") FPTOnColorWheelChanged OnColorChanged;

    /** Abre/inicializa el selector con este color (descompone en HSV y setea la rueda + sliders). */
    void OpenWith(const FLinearColor& Color);

    FLinearColor GetColor() const;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& G, float Dt) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override;
    virtual FReply NativeOnMouseMove(const FGeometry& G, const FPointerEvent& E) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override;

    UPROPERTY(meta=(BindWidget))         UImage*  Wheel       = nullptr;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* SatSlider   = nullptr;
    UPROPERTY(meta=(BindWidgetOptional)) USlider* ValueSlider = nullptr;
    UPROPERTY(meta=(BindWidgetOptional)) UImage*  Preview     = nullptr;
    UPROPERTY(meta=(BindWidgetOptional)) UImage*  Dot         = nullptr; // marca dónde quedó el color

    UFUNCTION() void OnSatChanged(float V);
    UFUNCTION() void OnValueChanged(float V);

private:
    float Hue = 0.f, Sat = 1.f, Val = 1.f; // 0..1
    float DotRadiusFrac = 0.8f;            // radio (0..1) del dot en la rueda (se actualiza al clickear)
    bool  bDragging = false;
    bool  bLoading  = false;
    bool  PickHueFromCursor(const FPointerEvent& E); // ángulo del cursor sobre la rueda → Hue + dot
    void  Recompute();                               // arma el color y avisa + tiñe el preview
    void  PlaceDot(const FVector2D& Local);          // posiciona el dot en coords del canvas
    void  UpdateDotFromHue();                        // recoloca el dot según Hue + DotRadiusFrac
};
