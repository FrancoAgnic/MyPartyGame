// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTShapeRadialWidget.h"
#include "../UI/PTRadialMenu.h"
#include "Framework/Application/SlateApplication.h"

void UPTShapeRadialWidget::UpdateSelection()
{
    const int32 N = SlotShapes.Num();
    if (N <= 0) return;

    // Si hay un UPTRadialMenu nativo dentro del WBP, delegamos en él (usa su propia geometría,
    // deadzone y ángulos) y le manejamos el resaltado. El orden de slots debe matchear SlotShapes.
    if (Radial)
    {
        const int32 NewIndex = Radial->HitTestAbsolute(FSlateApplication::Get().GetCursorPos());
        Radial->SetHighlightedIndex(NewIndex);
        if (NewIndex != SelectedIndex)
        {
            SelectedIndex = NewIndex;
            OnSelectionChanged(SelectedIndex);
        }
        return;
    }

    // Cursor y centro del widget en el MISMO espacio (local del widget, sin escala DPI): así el
    // DeadZonePixels es consistente sin importar la resolución. Mismo enfoque que la rueda de color.
    const FGeometry& Geo = GetCachedGeometry();
    const FVector2D LocalSize = Geo.GetLocalSize();
    if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f) return; // todavía sin geometría (primer frame)

    const FVector2D Center      = LocalSize * 0.5f;
    const FVector2D CursorAbs   = FSlateApplication::Get().GetCursorPos();
    const FVector2D CursorLocal = Geo.AbsoluteToLocal(CursorAbs);
    const FVector2D Delta        = CursorLocal - Center;

    int32 NewIndex = -1;
    if (Delta.Size() >= DeadZonePixels)
    {
        // Ángulo con "arriba" = 0 y sentido HORARIO positivo (Y crece hacia abajo en pantalla).
        float A = FMath::Atan2(Delta.Y, Delta.X) + HALF_PI;
        A = FMath::Fmod(A + 2.f * PI, 2.f * PI);          // normalizar a [0, 2π)
        const float Sector = 2.f * PI / static_cast<float>(N);
        NewIndex = FMath::RoundToInt(A / Sector) % N;     // slot centrado en cada sector
    }

    if (NewIndex != SelectedIndex)
    {
        SelectedIndex = NewIndex;
        OnSelectionChanged(SelectedIndex); // el BP resalta el slot
    }
}

bool UPTShapeRadialWidget::GetSelectedShape(EPTStampShape& OutShape) const
{
    if (SlotShapes.IsValidIndex(SelectedIndex))
    {
        OutShape = SlotShapes[SelectedIndex];
        return true;
    }
    return false; // zona muerta → mantener la forma actual
}
