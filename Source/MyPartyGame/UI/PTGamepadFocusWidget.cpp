#include "PTGamepadFocusWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"

TSharedRef<SWidget> UPTGamepadFocusWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        UCanvasPanel* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("Canvas"));
        WidgetTree->RootWidget = Canvas;
        Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Frame"));
        Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(1.f, 1.f, 1.f, 0.06f), 12.f, OutlineColor, OutlineWidth));
        Frame->SetVisibility(ESlateVisibility::Collapsed);
        Canvas->AddChildToCanvas(Frame);
        SetVisibility(ESlateVisibility::HitTestInvisible); // nunca intercepta el mouse
    }
    return Super::RebuildWidget();
}

void UPTGamepadFocusWidget::SetTarget(const FVector2D& ViewportPos, const FVector2D& ViewportSize, float Time)
{
    if (!Frame) return;
    if (UCanvasPanelSlot* S = Cast<UCanvasPanelSlot>(Frame->Slot))
    {
        S->SetPosition(ViewportPos - FVector2D(Margin));
        S->SetSize(ViewportSize + FVector2D(Margin * 2.f));
    }
    // Pulso suave para que se note dónde está el foco.
    const float A = 0.75f + 0.25f * FMath::Sin(Time * 6.f);
    Frame->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, A));
    Frame->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPTGamepadFocusWidget::HideTarget()
{
    if (Frame) Frame->SetVisibility(ESlateVisibility::Collapsed);
}
