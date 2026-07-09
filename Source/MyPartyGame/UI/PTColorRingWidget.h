// Anillo de colores guardados (paleta) dibujado como una "dona" segmentada alrededor
// de la rueda: 1 color ocupa todo el anillo, 2 se parten a la mitad, 3 en tercios, etc.
// Es un widget nativo (dibujo custom en Slate), así que se coloca en el WBP concéntrico
// con la rueda y un poco más grande. Todo el dibujo/hit-test vive en C++.

#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "PTColorRingWidget.generated.h"

class SPTColorRing;

UCLASS()
class MYPARTYGAME_API UPTColorRingWidget : public UWidget
{
    GENERATED_BODY()

public:
    /** Colores a mostrar (la paleta). Reparte el anillo en partes iguales. */
    void SetColors(const TArray<FLinearColor>& InColors);

    /** Índice del segmento bajo una posición ABSOLUTA de pantalla, o -1 si cae fuera del anillo. */
    int32 SegmentAt(const FVector2D& AbsPos) const;

    /** Devuelve true + el color si la posición absoluta cae sobre un segmento. */
    bool GetColorAt(const FVector2D& AbsPos, FLinearColor& OutColor) const;

    /** Radio interno como fracción del radio del anillo (el agujero donde va la rueda). */
    UPROPERTY(EditAnywhere, Category="Ring", meta=(ClampMin="0.0", ClampMax="0.95"))
    float InnerRadiusRatio = 0.72f;

    /** Separación entre segmentos, en grados (0 = pegados). */
    UPROPERTY(EditAnywhere, Category="Ring", meta=(ClampMin="0.0", ClampMax="20.0"))
    float GapDegrees = 2.f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
    UPROPERTY() TArray<FLinearColor> Colors;
    TSharedPtr<SPTColorRing> MyRing;
};
