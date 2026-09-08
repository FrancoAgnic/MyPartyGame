// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSpectatorHUDWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

TSharedRef<SWidget> UPTSpectatorHUDWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("SpecRoot"));
        WidgetTree->RootWidget = RootCanvas;

        FlagImg = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("FlagBig"));
        if (UCanvasPanelSlot* S = RootCanvas->AddChildToCanvas(FlagImg))
        {
            // Anclado al centro-derecha: ancla en (1, 0.5), alineado por su borde derecho y centro vertical.
            S->SetAnchors(FAnchors(1.f, 0.5f));
            S->SetAlignment(FVector2D(1.f, 0.5f));
            S->SetSize(FlagSize);
            S->SetPosition(FVector2D(-RightMargin, 0.f));
        }
        if (FlagImg) FlagImg->SetVisibility(ESlateVisibility::Collapsed); // arranca oculta
    }
    return Super::RebuildWidget();
}

void UPTSpectatorHUDWidget::SetFlag(UTexture2D* Flag)
{
    if (!FlagImg) return;
    if (Flag)
    {
        FlagImg->SetBrushFromTexture(Flag);
        // Mantener el aspecto de la textura al tamaño pedido.
        FlagImg->SetDesiredSizeOverride(FlagSize);
        FlagImg->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        FlagImg->SetVisibility(ESlateVisibility::Collapsed);
    }
}
