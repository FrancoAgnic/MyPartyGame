// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTRadialMenu.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "PTRadialMenu"

// ─────────────────────────────────────────────────────────────────────────────
// Slate: dibuja los iconos en círculo y resalta el seleccionado. Sin widgets hijos.
// ─────────────────────────────────────────────────────────────────────────────
class SPTRadialMenu : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPTRadialMenu) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& /*InArgs*/) {}

    void SetData(const TArray<FPTRadialSlot>& InSlots, float InRadius, FVector2D InIconSize,
                 float InStartAngle, bool bInClockwise, float InDeadZone,
                 const FSlateBrush& InHighlight, float InHighlightScale,
                 FLinearColor InNormalTint, FLinearColor InHighlightTint,
                 FLinearColor InGlowColor, float InGlowScale)
    {
        Slots          = InSlots;
        Radius         = InRadius;
        IconSize       = InIconSize;
        StartAngle     = InStartAngle;
        bClockwise     = bInClockwise;
        DeadZone       = InDeadZone;
        HighlightBrush = InHighlight;
        HighlightScale = FMath::Max(0.1f, InHighlightScale);
        NormalTint     = InNormalTint;
        HighlightTint  = InHighlightTint;
        GlowColor      = InGlowColor;
        GlowScale      = FMath::Max(1.f, InGlowScale);
        Invalidate(EInvalidateWidgetReason::Layout | EInvalidateWidgetReason::Paint);
    }

    void SetSliceStyle(bool bInDraw, float InInner, float InOuter, float InGapDeg,
                       FLinearColor InColInner, FLinearColor InColOuter,
                       FLinearColor InHoverInner, FLinearColor InHoverOuter,
                       FLinearColor InOutline, float InOutlineThickness)
    {
        bDrawSlices    = bInDraw;
        InnerRadius    = FMath::Max(0.f, InInner);
        OuterRadius    = FMath::Max(InnerRadius + 1.f, InOuter);
        SliceGapDeg    = FMath::Clamp(InGapDeg, 0.f, 30.f);
        ColInner       = InColInner;
        ColOuter       = InColOuter;
        HoverInner     = InHoverInner;
        HoverOuter     = InHoverOuter;
        OutlineColor   = InOutline;
        OutlineThick   = FMath::Max(0.f, InOutlineThickness);
        Invalidate(EInvalidateWidgetReason::Paint);
    }

    void SetHighlighted(int32 Index)
    {
        if (Highlighted != Index) { Highlighted = Index; Invalidate(EInvalidateWidgetReason::Paint); }
    }

    /** Slot bajo una posición absoluta de pantalla (usa la última geometría pintada). */
    int32 HitTestAbsolute(FVector2D Abs) const
    {
        const int32 N = Slots.Num();
        if (N <= 0) return -1;

        const FVector2D Center = CachedGeometry.GetLocalSize() * 0.5f;
        const FVector2D Local  = CachedGeometry.AbsoluteToLocal(Abs);
        const FVector2D Delta  = Local - Center;
        if (Delta.Size() < DeadZone) return -1;

        const float Step = 360.f / static_cast<float>(N);
        float Ang = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
        float Rel = (Ang - StartAngle) * (bClockwise ? 1.f : -1.f);
        Rel = FMath::Fmod(Rel + 3600.f, 360.f);
        return FMath::RoundToInt(Rel / Step) % N;
    }

    virtual FVector2D ComputeDesiredSize(float) const override
    {
        // El box abarca lo más grande entre las porciones (radio exterior) y el círculo de iconos + glow.
        const float Half = FMath::Max(HighlightScale, GlowScale) * 0.5f;
        const float IconExtent = Radius + FMath::Max(IconSize.X, IconSize.Y) * Half;
        const float Extent = FMath::Max(bDrawSlices ? OuterRadius : 0.f, IconExtent) + OutlineThick;
        return FVector2D(Extent * 2.f, Extent * 2.f);
    }

    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
                          const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements,
                          int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
    {
        CachedGeometry = AllottedGeometry; // para HitTestAbsolute

        const int32 N = Slots.Num();
        if (N <= 0) return LayerId;

        const FVector2D Center = AllottedGeometry.GetLocalSize() * 0.5f;
        const float     Step   = 360.f / static_cast<float>(N);

        int32 SliceLayer = LayerId; // porciones abajo; iconos arriba

        // ── Porciones tipo pizza (sectores anulares con degradado del centro hacia afuera) ──
        if (bDrawSlices && OuterRadius > InnerRadius)
        {
            const FSlateRenderTransform RT = AllottedGeometry.GetAccumulatedRenderTransform();
            const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");
            FSlateResourceHandle Handle;
            if (WhiteBrush && FSlateApplication::IsInitialized() && FSlateApplication::Get().GetRenderer())
                Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*WhiteBrush);

            const float GapHalf = SliceGapDeg * 0.5f;
            const float DirSign = (bClockwise ? 1.f : -1.f);

            TArray<FSlateVertex> Verts;
            TArray<SlateIndex>   Indices;

            for (int32 i = 0; i < N; ++i)
            {
                const bool bHot = (i == Highlighted);
                const FColor CI = (bHot ? HoverInner : ColInner).ToFColor(true);
                const FColor CO = (bHot ? HoverOuter : ColOuter).ToFColor(true);

                const float CenterDeg = StartAngle + DirSign * i * Step;
                const float A0 = FMath::DegreesToRadians(CenterDeg - Step * 0.5f + GapHalf);
                const float A1 = FMath::DegreesToRadians(CenterDeg + Step * 0.5f - GapHalf);
                const float SpanDeg = FMath::Abs((A1 - A0)) * (180.f / PI);
                const int32 Seg = FMath::Clamp(FMath::CeilToInt(SpanDeg / 6.f), 2, 64);

                const int32 Base = Verts.Num();
                for (int32 t = 0; t <= Seg; ++t)
                {
                    const float Ang = FMath::Lerp(A0, A1, static_cast<float>(t) / static_cast<float>(Seg));
                    const FVector2f Dir(FMath::Cos(Ang), FMath::Sin(Ang));
                    const FVector2f Inner = FVector2f(Center) + Dir * InnerRadius;
                    const FVector2f Outer = FVector2f(Center) + Dir * OuterRadius;
                    Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, Inner, FVector2f(0.5f, 0.5f), CI));
                    Verts.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(RT, Outer, FVector2f(0.5f, 0.5f), CO));
                }
                for (int32 s = 0; s < Seg; ++s)
                {
                    const SlateIndex i0 = Base + 2 * s;       // inner s
                    const SlateIndex o0 = Base + 2 * s + 1;   // outer s
                    const SlateIndex i1 = Base + 2 * (s + 1); // inner s+1
                    const SlateIndex o1 = Base + 2 * (s + 1) + 1; // outer s+1
                    Indices.Add(i0); Indices.Add(o0); Indices.Add(o1);
                    Indices.Add(i0); Indices.Add(o1); Indices.Add(i1);
                }
            }

            if (Verts.Num() > 0 && Handle.IsValid())
            {
                FSlateDrawElement::MakeCustomVerts(OutDrawElements, SliceLayer, Handle, Verts, Indices, nullptr, 0, 0);
            }

            // Outline de la porción bajo el cursor (contorno del sector).
            if (Slots.IsValidIndex(Highlighted) && OutlineColor.A > 0.f && OutlineThick > 0.f)
            {
                const float CenterDeg = StartAngle + DirSign * Highlighted * Step;
                const float A0 = FMath::DegreesToRadians(CenterDeg - Step * 0.5f + GapHalf);
                const float A1 = FMath::DegreesToRadians(CenterDeg + Step * 0.5f - GapHalf);
                const float SpanDeg = FMath::Abs((A1 - A0)) * (180.f / PI);
                const int32 Seg = FMath::Clamp(FMath::CeilToInt(SpanDeg / 6.f), 2, 64);

                TArray<FVector2D> Pts;
                for (int32 t = 0; t <= Seg; ++t) // arco exterior A0→A1
                {
                    const float Ang = FMath::Lerp(A0, A1, static_cast<float>(t) / static_cast<float>(Seg));
                    Pts.Add(Center + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * OuterRadius);
                }
                for (int32 t = Seg; t >= 0; --t) // arco interior A1→A0
                {
                    const float Ang = FMath::Lerp(A0, A1, static_cast<float>(t) / static_cast<float>(Seg));
                    Pts.Add(Center + FVector2D(FMath::Cos(Ang), FMath::Sin(Ang)) * InnerRadius);
                }
                const FVector2D FirstPt = Pts[0]; // copia: pasar Pts[0] directo a Add() aliasea el array
                Pts.Add(FirstPt);                 // cerrar el contorno
                FSlateDrawElement::MakeLines(OutDrawElements, SliceLayer + 1, AllottedGeometry.ToPaintGeometry(),
                                             Pts, ESlateDrawEffect::None, OutlineColor, true, OutlineThick);
            }
        }

        const int32 IconLayer = SliceLayer + 2; // iconos SIEMPRE por encima de las porciones y el outline

        for (int32 i = 0; i < N; ++i)
        {
            const bool   bHot = (i == Highlighted);
            const float  Deg  = StartAngle + (bClockwise ? 1.f : -1.f) * i * Step;
            const float  Rad  = FMath::DegreesToRadians(Deg);
            const FVector2D Pos = Center + FVector2D(FMath::Cos(Rad), FMath::Sin(Rad)) * Radius;
            const FSlateBrush& Brush = Slots[i].Icon;

            if (bHot)
            {
                // Aro/glow por brush asignado (opcional): se dibuja bien atrás.
                if (HighlightBrush.GetResourceObject() != nullptr)
                {
                    const FVector2D HSize = IconSize * (HighlightScale + 0.25f);
                    FSlateDrawElement::MakeBox(
                        OutDrawElements, IconLayer,
                        AllottedGeometry.ToPaintGeometry(HSize, FSlateLayoutTransform(Pos - HSize * 0.5f)),
                        &HighlightBrush, ESlateDrawEffect::None, HighlightBrush.TintColor.GetSpecifiedColor());
                }

                // Glow/outline: el MISMO icono agrandado y tintado detrás → contorno luminoso que
                // sigue la silueta del icono (para íconos con transparencia). Alpha 0 lo apaga.
                if (GlowColor.A > 0.f)
                {
                    const FVector2D GSize = IconSize * GlowScale;
                    FSlateDrawElement::MakeBox(
                        OutDrawElements, IconLayer,
                        AllottedGeometry.ToPaintGeometry(GSize, FSlateLayoutTransform(Pos - GSize * 0.5f)),
                        &Brush, ESlateDrawEffect::None, GlowColor);
                }
            }

            // Icono del slot (por encima del glow).
            const FVector2D Size = IconSize * (bHot ? HighlightScale : 1.f);
            const FLinearColor Tint = (bHot ? HighlightTint : NormalTint) * Brush.TintColor.GetSpecifiedColor();
            FSlateDrawElement::MakeBox(
                OutDrawElements, IconLayer + 1,
                AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos - Size * 0.5f)),
                &Brush, ESlateDrawEffect::None, Tint);
        }

        return IconLayer + 2;
    }

private:
    TArray<FPTRadialSlot> Slots;
    float        Radius         = 160.f;
    FVector2D    IconSize       = FVector2D(72.f, 72.f);
    float        StartAngle     = -90.f;
    bool         bClockwise     = true;
    float        DeadZone       = 45.f;
    FSlateBrush  HighlightBrush;
    float        HighlightScale = 1.2f;
    FLinearColor NormalTint     = FLinearColor::White;
    FLinearColor HighlightTint  = FLinearColor::White;
    FLinearColor GlowColor      = FLinearColor(1.f, 0.9f, 0.2f, 0.9f);
    float        GlowScale      = 1.55f;
    int32        Highlighted    = -1;

    // Porciones tipo pizza (fondo de cada slot).
    bool         bDrawSlices    = true;
    float        InnerRadius    = 70.f;
    float        OuterRadius    = 240.f;
    float        SliceGapDeg    = 4.f;
    FLinearColor ColInner       = FLinearColor(0.05f, 0.05f, 0.08f, 0.55f);
    FLinearColor ColOuter       = FLinearColor(0.15f, 0.15f, 0.22f, 0.85f);
    FLinearColor HoverInner     = FLinearColor(0.20f, 0.35f, 0.55f, 0.70f);
    FLinearColor HoverOuter     = FLinearColor(0.35f, 0.55f, 0.85f, 0.95f);
    FLinearColor OutlineColor   = FLinearColor(1.f, 0.9f, 0.3f, 1.f);
    float        OutlineThick   = 2.5f;

    mutable FGeometry CachedGeometry;
};

// ─────────────────────────────────────────────────────────────────────────────
// UWidget: expone las propiedades al panel de detalles y sincroniza con el Slate.
// ─────────────────────────────────────────────────────────────────────────────
TSharedRef<SWidget> UPTRadialMenu::RebuildWidget()
{
    MyRadial = SNew(SPTRadialMenu);
    return MyRadial.ToSharedRef();
}

void UPTRadialMenu::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    if (MyRadial.IsValid())
    {
        MyRadial->SetData(Slots, Radius, IconSize, StartAngleDegrees, bClockwise, DeadZoneRadius,
                          HighlightBrush, HighlightScale, NormalTint, HighlightTint, GlowColor, GlowScale);
        MyRadial->SetSliceStyle(bDrawSlices, SliceInnerRadius, SliceOuterRadius, SliceGapDegrees,
                                SliceColorInner, SliceColorOuter, SliceHoverColorInner, SliceHoverColorOuter,
                                OutlineColor, OutlineThickness);
        MyRadial->SetHighlighted(HighlightedIndex);
    }
}

void UPTRadialMenu::ReleaseSlateResources(bool bReleaseChildren)
{
    Super::ReleaseSlateResources(bReleaseChildren);
    MyRadial.Reset();
}

void UPTRadialMenu::SetHighlightedIndex(int32 Index)
{
    HighlightedIndex = Index;
    if (MyRadial.IsValid()) MyRadial->SetHighlighted(Index);
}

int32 UPTRadialMenu::HitTestAbsolute(FVector2D AbsScreenPos) const
{
    return MyRadial.IsValid() ? MyRadial->HitTestAbsolute(AbsScreenPos) : -1;
}

#if WITH_EDITOR
const FText UPTRadialMenu::GetPaletteCategory()
{
    return LOCTEXT("PartyGameCategory", "Party Game");
}
#endif

#undef LOCTEXT_NAMESPACE
