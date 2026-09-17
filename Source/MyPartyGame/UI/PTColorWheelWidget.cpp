// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTColorWheelWidget.h"
#include "Components/Image.h"
#include "Components/Slider.h"

void UPTColorWheelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (SatSlider)   SatSlider->OnValueChanged.AddDynamic(this, &UPTColorWheelWidget::OnSatChanged);
    if (ValueSlider) ValueSlider->OnValueChanged.AddDynamic(this, &UPTColorWheelWidget::OnValueChanged);
}

FLinearColor UPTColorWheelWidget::GetColor() const
{
    return FLinearColor(Hue * 360.f, Sat, Val, 1.f).HSVToLinearRGB();
}

void UPTColorWheelWidget::OpenWith(const FLinearColor& Color)
{
    const FLinearColor HSV = Color.LinearRGBToHSV(); // R=Hue(0..360) G=Sat B=Value
    Hue = HSV.R / 360.f; Sat = HSV.G; Val = HSV.B;

    bLoading = true;
    if (SatSlider)   SatSlider->SetValue(Sat);
    if (ValueSlider) ValueSlider->SetValue(Val);
    bLoading = false;

    if (Preview) Preview->SetColorAndOpacity(GetColor());
}

bool UPTColorWheelWidget::PickHueFromCursor(const FPointerEvent& E)
{
    if (!Wheel) return false;
    const FGeometry& G = Wheel->GetCachedGeometry();
    const FVector2D Size = G.GetLocalSize();
    if (Size.X <= 0.f || Size.Y <= 0.f) return false;
    const FVector2D Local = G.AbsoluteToLocal(E.GetScreenSpacePosition());
    const FVector2D D = Local - Size * 0.5f;
    if (D.SizeSquared() < 1.f) return false; // muy al centro: ignorar
    float Ang = FMath::Atan2(D.Y, D.X) / (2.f * PI); // -0.5..0.5
    if (Ang < 0.f) Ang += 1.f;
    Hue = Ang;
    Recompute();
    return true;
}

void UPTColorWheelWidget::Recompute()
{
    const FLinearColor C = GetColor();
    if (Preview) Preview->SetColorAndOpacity(C);
    OnColorChanged.Broadcast(C);
}

void UPTColorWheelWidget::OnSatChanged(float V)   { if (bLoading) return; Sat = V; Recompute(); }
void UPTColorWheelWidget::OnValueChanged(float V) { if (bLoading) return; Val = V; Recompute(); }

FReply UPTColorWheelWidget::NativeOnMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    if (E.GetEffectingButton() == EKeys::LeftMouseButton && PickHueFromCursor(E))
    {
        bDragging = true;
        return FReply::Handled().CaptureMouse(TakeWidget());
    }
    return Super::NativeOnMouseButtonDown(G, E);
}

FReply UPTColorWheelWidget::NativeOnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
    if (bDragging) { PickHueFromCursor(E); return FReply::Handled(); }
    return Super::NativeOnMouseMove(G, E);
}

FReply UPTColorWheelWidget::NativeOnMouseButtonUp(const FGeometry& G, const FPointerEvent& E)
{
    if (bDragging && E.GetEffectingButton() == EKeys::LeftMouseButton)
    {
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(G, E);
}
