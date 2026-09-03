// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTRadialMenu.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"

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
        // El box abarca el círculo + medio icono (con la escala mayor entre resaltado/glow, para no recortar).
        const float Half = FMath::Max(HighlightScale, GlowScale) * 0.5f;
        return FVector2D((Radius + IconSize.X * Half) * 2.f, (Radius + IconSize.Y * Half) * 2.f);
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
                        OutDrawElements, LayerId,
                        AllottedGeometry.ToPaintGeometry(HSize, FSlateLayoutTransform(Pos - HSize * 0.5f)),
                        &HighlightBrush, ESlateDrawEffect::None, HighlightBrush.TintColor.GetSpecifiedColor());
                }

                // Glow/outline: el MISMO icono agrandado y tintado detrás → contorno luminoso que
                // sigue la silueta del icono (para íconos con transparencia). Alpha 0 lo apaga.
                if (GlowColor.A > 0.f)
                {
                    const FVector2D GSize = IconSize * GlowScale;
                    FSlateDrawElement::MakeBox(
                        OutDrawElements, LayerId,
                        AllottedGeometry.ToPaintGeometry(GSize, FSlateLayoutTransform(Pos - GSize * 0.5f)),
                        &Brush, ESlateDrawEffect::None, GlowColor);
                }
            }

            // Icono del slot (por encima del glow).
            const FVector2D Size = IconSize * (bHot ? HighlightScale : 1.f);
            const FLinearColor Tint = (bHot ? HighlightTint : NormalTint) * Brush.TintColor.GetSpecifiedColor();
            FSlateDrawElement::MakeBox(
                OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos - Size * 0.5f)),
                &Brush, ESlateDrawEffect::None, Tint);
        }

        return LayerId + 2;
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
