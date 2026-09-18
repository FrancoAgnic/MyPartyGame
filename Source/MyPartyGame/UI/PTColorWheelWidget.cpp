// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTColorWheelWidget.h"
#include "Components/Image.h"
#include "Components/Slider.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/PanelWidget.h"

void UPTColorWheelWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (SatSlider)   SatSlider->OnValueChanged.AddDynamic(this, &UPTColorWheelWidget::OnSatChanged);
    if (ValueSlider) ValueSlider->OnValueChanged.AddDynamic(this, &UPTColorWheelWidget::OnValueChanged);
    // La Image por defecto NO es hit-testeable → el click la atraviesa y no llega el evento. Forzarla a
    // Visible (y el widget también) para poder mantener-arrastrar-soltar sobre la rueda.
    SetVisibility(ESlateVisibility::Visible);
    if (Wheel) Wheel->SetVisibility(ESlateVisibility::Visible);
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
    const FVector2D C = Size * 0.5f;
    const FVector2D Local = G.AbsoluteToLocal(E.GetScreenSpacePosition());
    FVector2D D = Local - C;
    if (D.SizeSquared() < 1.f) return false; // muy al centro: ignorar
    const float Radius = FMath::Min(Size.X, Size.Y) * 0.5f;
    // Clampear al borde de la rueda para que el dot no se salga.
    if (D.Size() > Radius) D = D.GetSafeNormal() * Radius;
    Hue = AngleToHue(FMath::Atan2(D.Y, D.X));
    DotRadiusFrac = Radius > 0.f ? FMath::Clamp(D.Size() / Radius, 0.1f, 1.f) : 0.8f;
    PlaceDot(C + D); // punto en espacio LOCAL de la rueda (clampeado al borde)
    Recompute();
    return true;
}

float UPTColorWheelWidget::AngleToHue(float AngRad) const
{
    float H = AngRad / (2.f * PI);           // 0..1 (sentido de pantalla, Y hacia abajo)
    if (bReverseHue) H = -H;
    H += HueOffsetDeg / 360.f;
    H = H - FMath::FloorToFloat(H);          // wrap a [0,1)
    return H;
}

float UPTColorWheelWidget::HueToAngleRad(float H) const
{
    H -= HueOffsetDeg / 360.f;               // inverso de AngleToHue
    if (bReverseHue) H = -H;
    return H * 2.f * PI;
}

void UPTColorWheelWidget::PlaceDot(const FVector2D& WheelLocal)
{
    if (!Dot || !Wheel) return;
    UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(Dot->Slot);
    if (!CS) return; // el Dot tiene que estar en un Canvas Panel
    // Convertir el punto de la RUEDA a espacio del CANVAS del dot (por si no comparten origen/escala), así
    // el CENTRO del dot cae EXACTO donde apunta el cursor (la punta de la flecha), sin corrimiento.
    const FVector2D Abs = Wheel->GetCachedGeometry().LocalToAbsolute(WheelLocal);
    FVector2D CanvasLocal = Abs;
    if (UWidget* Parent = Dot->GetParent())
        CanvasLocal = Parent->GetCachedGeometry().AbsoluteToLocal(Abs);
    CS->SetAnchors(FAnchors(0.f, 0.f));
    CS->SetAlignment(FVector2D(0.5f, 0.5f)); // el centro del dot en la posición
    CS->SetPosition(CanvasLocal);
}

void UPTColorWheelWidget::UpdateDotFromHue()
{
    if (!Wheel || !Dot) return;
    const FVector2D Size = Wheel->GetCachedGeometry().GetLocalSize();
    if (Size.X <= 0.f || Size.Y <= 0.f) return; // aún sin layout
    const FVector2D C = Size * 0.5f;
    const float Radius = FMath::Min(Size.X, Size.Y) * 0.5f * DotRadiusFrac;
    const float A = HueToAngleRad(Hue);
    PlaceDot(C + FVector2D(FMath::Cos(A), FMath::Sin(A)) * Radius);
}

void UPTColorWheelWidget::NativeTick(const FGeometry& G, float Dt)
{
    Super::NativeTick(G, Dt);
    // Mientras NO arrastrás, mantener el dot en su lugar según el Hue (cubre el init y el timing de layout).
    if (!bDragging) UpdateDotFromHue();
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
