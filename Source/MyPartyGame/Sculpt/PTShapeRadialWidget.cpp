// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTShapeRadialWidget.h"
#include "../UI/PTRadialMenu.h"
#include "Components/Image.h"
#include "Components/CanvasPanelSlot.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"

int32 UPTShapeRadialWidget::NumPages() const
{
    return FMath::Max(1, FMath::DivideAndRoundUp(ShapeSlots.Num(), SlotsPerPage));
}

void UPTShapeRadialWidget::BeginRadial(int32 StartPage)
{
    CurrentPage   = FMath::Clamp(StartPage, 0, NumPages() - 1);
    SelectedIndex = -1;
    BuildPage();

    // Cursor DOT: si el WBP tiene un CursorDot, lo preparamos (blanco, sin tinte) y OCULTAMOS el cursor
    // del SO (EMouseCursor::None) para que se vea solo el dot — igual que el gotero del color picker.
    if (CursorDot && CursorDotTexture)
    {
        CursorDot->SetBrushFromTexture(CursorDotTexture);
        CursorDot->SetBrushSize(CursorDotSize);
        CursorDot->SetColorAndOpacity(FLinearColor::White); // sin tinte
        // Anchor/alineación top-left para que la posición sea en coords locales absolutas.
        if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(CursorDot->Slot))
        {
            CS->SetAnchors(FAnchors(0.f, 0.f));
            CS->SetAlignment(FVector2D(0.f, 0.f));
            CS->SetSize(CursorDotSize);
        }
        // Oculto hasta que UpdateSelection lo posicione (evita el flash abajo-derecha del primer frame).
        CursorDot->SetVisibility(ESlateVisibility::Collapsed);
        if (APlayerController* PC = GetOwningPlayer())
        {
            SetCursor(EMouseCursor::None);
            PC->CurrentMouseCursor = EMouseCursor::None; // solo se ve el dot
        }
    }
    else if (CursorDot)
    {
        CursorDot->SetVisibility(ESlateVisibility::Collapsed); // sin textura → no mostrar caja vacía
    }
}

void UPTShapeRadialWidget::NativeDestruct()
{
    // Restaurar el cursor del SO al cerrar el radial.
    if (CursorDot)
        if (APlayerController* PC = GetOwningPlayer())
            PC->CurrentMouseCursor = EMouseCursor::Default;
    Super::NativeDestruct();
}

void UPTShapeRadialWidget::BuildPage()
{
    if (!Radial) return;

    // Cargar en el radial los (hasta 4) iconos de la página actual, en orden cardinal.
    Radial->Slots.Reset();
    const int32 Base = CurrentPage * SlotsPerPage;
    for (int32 i = 0; i < SlotsPerPage; ++i)
    {
        const int32 G = Base + i;
        if (!ShapeSlots.IsValidIndex(G)) break;
        FPTRadialSlot S;
        S.Icon = ShapeSlots[G].Icon;
        S.Tag  = FName(*ShapeSlots[G].Name.ToString());
        Radial->Slots.Add(S);
    }
    // Layout cardinal: slot 0 arriba, horario (0=arriba,1=der,2=abajo,3=izq).
    Radial->StartAngleDegrees = -90.f;
    Radial->bClockwise        = true;
    Radial->SetHighlightedIndex(-1);
    Radial->SynchronizeProperties(); // reconstruye los visuales con los nuevos slots
    SelectedIndex = -1;
    OnSelectionChanged(-1);
}

void UPTShapeRadialWidget::NextPage()
{
    if (NumPages() <= 1) return;
    CurrentPage = (CurrentPage + 1) % NumPages();
    BuildPage();
}

void UPTShapeRadialWidget::PrevPage()
{
    if (NumPages() <= 1) return;
    CurrentPage = (CurrentPage - 1 + NumPages()) % NumPages();
    BuildPage();
}

void UPTShapeRadialWidget::UpdateSelection()
{
    // Cuántos slots tiene la página actual (la última puede tener menos de 4).
    const int32 Base       = CurrentPage * SlotsPerPage;
    const int32 SlotsHere  = FMath::Clamp(ShapeSlots.Num() - Base, 0, SlotsPerPage);
    if (SlotsHere <= 0) return;

    // Centro y cursor en espacio LOCAL del widget (sin DPI) para un deadzone consistente.
    const FGeometry& Geo = GetCachedGeometry();
    const FVector2D LocalSize = Geo.GetLocalSize();
    if (LocalSize.X <= 0.f || LocalSize.Y <= 0.f) return; // sin geometría aún (primer frame)

    const FVector2D Center      = LocalSize * 0.5f;
    const FVector2D CursorLocal = Geo.AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());
    const FVector2D Delta       = CursorLocal - Center;

    // Mover el dot custom a la posición del cursor (centrado). Forzamos anchor/alineación top-left para
    // que SetPosition sea en coords locales absolutas (si el slot está anclado al centro, se iría abajo-der).
    if (CursorDot && CursorDotTexture)
        if (UCanvasPanelSlot* CS = Cast<UCanvasPanelSlot>(CursorDot->Slot))
        {
            CS->SetAnchors(FAnchors(0.f, 0.f));
            CS->SetAlignment(FVector2D(0.f, 0.f));
            CS->SetSize(CursorDotSize);
            CS->SetPosition(CursorLocal - CursorDotSize * 0.5f);
            CursorDot->SetVisibility(ESlateVisibility::HitTestInvisible); // ya posicionado → mostrar
        }

    // Selección CARDINAL por eje dominante (nada de diagonales): rapidísima y sin ambigüedad.
    //   0 = arriba, 1 = derecha, 2 = abajo, 3 = izquierda (matchea StartAngle -90 + horario).
    int32 NewIndex = -1;
    if (Delta.Size() >= DeadZonePixels)
    {
        if (FMath::Abs(Delta.X) > FMath::Abs(Delta.Y))
            NewIndex = (Delta.X > 0.f) ? 1 : 3;   // derecha / izquierda
        else
            NewIndex = (Delta.Y < 0.f) ? 0 : 2;   // arriba / abajo
    }
    // Si esa dirección no tiene slot en esta página (página incompleta), no seleccionar.
    if (NewIndex >= SlotsHere) NewIndex = -1;

    if (Radial) Radial->SetHighlightedIndex(NewIndex);
    if (NewIndex != SelectedIndex)
    {
        SelectedIndex = NewIndex;
        OnSelectionChanged(SelectedIndex);
    }
}

bool UPTShapeRadialWidget::GetSelectedShape(EPTStampShape& OutShape) const
{
    if (SelectedIndex < 0) return false; // zona muerta → mantener la forma actual
    const int32 G = CurrentPage * SlotsPerPage + SelectedIndex;
    if (ShapeSlots.IsValidIndex(G))
    {
        OutShape = ShapeSlots[G].Shape;
        return true;
    }
    return false;
}
