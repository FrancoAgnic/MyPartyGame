#include "PTColorRingWidget.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"

// ─── Widget Slate que dibuja la dona segmentada ──────────────────────────────
class SPTColorRing : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPTColorRing) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& /*InArgs*/) {}

    void SetColors(const TArray<FLinearColor>& In)
    {
        Colors = In;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    void SetParams(float InInner, float InGapDeg)
    {
        InnerRatio = InInner;
        GapDeg     = InGapDeg;
        Invalidate(EInvalidateWidgetReason::Paint);
    }
    void SetDesign(bool b) { bDesign = b; Invalidate(EInvalidateWidgetReason::Paint); }
    void SetHovered(int32 Seg) { if (Seg != Hovered) { Hovered = Seg; Invalidate(EInvalidateWidgetReason::Paint); } }
    void SetHoverStyle(const FLinearColor& C, float T) { HoverCol = C; HoverThick = T; }

    virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(220.f, 220.f); }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        // En el editor, si no hay colores guardados, mostrar un placeholder gris para
        // poder posicionar/dimensionar el anillo. En runtime vacío no se dibuja nada.
        TArray<FLinearColor> Draw = Colors;
        if (Draw.Num() == 0)
        {
            if (!bDesign) return LayerId;
            for (int32 i = 0; i < 8; ++i) Draw.Add(FLinearColor(0.35f, 0.35f, 0.38f, 0.6f));
        }
        const int32 N = Draw.Num();

        const FVector2D SizeD = AllottedGeometry.GetLocalSize();
        const FVector2f Size((float)SizeD.X, (float)SizeD.Y);
        const FVector2f Center = Size * 0.5f;
        const float Ro = FMath::Min(Size.X, Size.Y) * 0.5f;
        const float Ri = Ro * FMath::Clamp(InnerRatio, 0.f, 0.95f);
        if (Ro <= 1.f) return LayerId;

        const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
        if (!WhiteBrush) return LayerId;
        FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);

        const FSlateRenderTransform RT = AllottedGeometry.GetAccumulatedRenderTransform();
        const float TwoPi  = 2.f * PI;
        const float GapRad = FMath::DegreesToRadians(FMath::Clamp(GapDeg, 0.f, 20.f));

        TArray<FSlateVertex> Verts;
        TArray<SlateIndex>   Indices;

        for (int32 s = 0; s < N; ++s)
        {
            const float base0 = TwoPi * s / N;
            const float base1 = TwoPi * (s + 1) / N;
            // Separación entre segmentos (nada si es un solo color = anillo completo).
            const float a0 = (N > 1) ? base0 + GapRad * 0.5f : base0;
            const float a1 = (N > 1) ? base1 - GapRad * 0.5f : base1;
            const int32 Steps = FMath::Max(2, FMath::CeilToInt((a1 - a0) / (TwoPi / 96.f)));
            const FColor Col  = Draw[s].ToFColor(true);
            const int32 Base  = Verts.Num();

            for (int32 i = 0; i <= Steps; ++i)
            {
                const float a = a0 + (a1 - a0) * i / Steps;
                // Ángulo horario desde arriba: dir = (sin a, -cos a).
                const FVector2f Dir(FMath::Sin(a), -FMath::Cos(a));
                const FVector2f PInner = Center + Dir * Ri;
                const FVector2f POuter = Center + Dir * Ro;
                Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, PInner, FVector2f(0.f, 0.f), Col));
                Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, POuter, FVector2f(1.f, 1.f), Col));
            }
            for (int32 i = 0; i < Steps; ++i)
            {
                const SlateIndex b = (SlateIndex)(Base + i * 2);
                Indices.Add(b);     Indices.Add(b + 1); Indices.Add(b + 2);
                Indices.Add(b + 1); Indices.Add(b + 3); Indices.Add(b + 2);
            }
        }

        if (Verts.Num() >= 3)
            FSlateDrawElement::MakeCustomVerts(OutDrawElements, LayerId, Handle, Verts, Indices, nullptr, 0, 0);

        // Outline amarillo del segmento bajo el mouse (borde del sector anular).
        if (Hovered >= 0 && Hovered < N && Colors.Num() > 0)
        {
            const float base0 = TwoPi * Hovered / N;
            const float base1 = TwoPi * (Hovered + 1) / N;
            const float a0 = (N > 1) ? base0 + GapRad * 0.5f : base0;
            const float a1 = (N > 1) ? base1 - GapRad * 0.5f : base1;
            const int32 Steps = FMath::Max(2, FMath::CeilToInt((a1 - a0) / (TwoPi / 96.f)));

            TArray<FVector2D> Pts;
            for (int32 i = 0; i <= Steps; ++i) { const float a = a0 + (a1 - a0) * i / Steps; Pts.Add(FVector2D(Center.X + FMath::Sin(a) * Ro, Center.Y - FMath::Cos(a) * Ro)); }
            for (int32 i = Steps; i >= 0; --i) { const float a = a0 + (a1 - a0) * i / Steps; Pts.Add(FVector2D(Center.X + FMath::Sin(a) * Ri, Center.Y - FMath::Cos(a) * Ri)); }
            Pts.Add(Pts[0]); // cerrar el contorno

            FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(),
                                         Pts, ESlateDrawEffect::None, HoverCol, true, HoverThick);
        }

        return LayerId + 1;
    }

    TArray<FLinearColor> Colors;
    float InnerRatio = 0.72f;
    float GapDeg     = 2.f;
    bool  bDesign    = false;
    int32 Hovered    = -1;
    FLinearColor HoverCol = FLinearColor(1.f, 0.85f, 0.f, 1.f);
    float HoverThick = 3.f;
};

// ─── UWidget wrapper ─────────────────────────────────────────────────────────
TSharedRef<SWidget> UPTColorRingWidget::RebuildWidget()
{
    MyRing = SNew(SPTColorRing);
    MyRing->SetHoverStyle(HoverColor, HoverThickness);
    MyRing->SetParams(InnerRadiusRatio, GapDegrees);
    MyRing->SetColors(Colors);
    return MyRing.ToSharedRef();
}

void UPTColorRingWidget::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    MyRing.Reset();
}

void UPTColorRingWidget::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    if (MyRing.IsValid())
    {
        MyRing->SetHoverStyle(HoverColor, HoverThickness);
        MyRing->SetParams(InnerRadiusRatio, GapDegrees);
        MyRing->SetDesign(IsDesignTime()); // placeholder gris solo en el editor
        MyRing->SetColors(Colors);
    }
}

void UPTColorRingWidget::SetColors(const TArray<FLinearColor>& InColors)
{
    Colors = InColors;
    if (MyRing.IsValid())
    {
        MyRing->SetParams(InnerRadiusRatio, GapDegrees);
        MyRing->SetColors(Colors);
    }
}

void UPTColorRingWidget::SetHovered(int32 SegmentIndex)
{
    if (MyRing.IsValid()) MyRing->SetHovered(SegmentIndex);
}

int32 UPTColorRingWidget::SegmentAt(const FVector2D& AbsPos) const
{
    const int32 N = Colors.Num();
    if (N <= 0) return -1;

    const FGeometry& G = GetCachedGeometry();
    const FVector2D SizeD = G.GetLocalSize();
    if (SizeD.X <= 0.f || SizeD.Y <= 0.f) return -1;

    const FVector2D Local  = G.AbsoluteToLocal(AbsPos);
    const FVector2D Center = SizeD * 0.5;
    const FVector2D D      = Local - Center;
    const float Ro = FMath::Min(SizeD.X, SizeD.Y) * 0.5f;
    const float Ri = Ro * FMath::Clamp(InnerRadiusRatio, 0.f, 0.95f);
    const float r  = (float)D.Size();
    if (r < Ri || r > Ro) return -1;

    // Ángulo horario desde arriba: a = atan2(dx, -dy).
    float a = FMath::Atan2((float)D.X, (float)-D.Y);
    if (a < 0.f) a += 2.f * PI;
    const int32 Seg = FMath::FloorToInt(a / (2.f * PI / N));
    return FMath::Clamp(Seg, 0, N - 1);
}

bool UPTColorRingWidget::GetColorAt(const FVector2D& AbsPos, FLinearColor& OutColor) const
{
    const int32 s = SegmentAt(AbsPos);
    if (s < 0 || !Colors.IsValidIndex(s)) return false;
    OutColor = Colors[s];
    return true;
}
