// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLoadingScreenWidget.h"
#include "Animation/WidgetAnimation.h"

void UPTLoadingScreenWidget::PlayIntro(bool bStartAtLoop)
{
    if (bStartAtLoop || !AnimIn)
    {
        StartLoop();
        return;
    }
    // AnimIn una vez → al terminar, entra al loop.
    InEndDelegate.BindDynamic(this, &UPTLoadingScreenWidget::OnInFinished);
    BindToAnimationFinished(AnimIn, InEndDelegate);
    PlayAnimation(AnimIn);
}

void UPTLoadingScreenWidget::OnInFinished()
{
    StartLoop();
}

void UPTLoadingScreenWidget::StartLoop()
{
    const bool bWasCovering = bCovering;
    bCovering = true; // el AnimIn terminó → la pantalla ya tapa el nivel; ya se puede cargar
    if (AnimLoop) PlayAnimation(AnimLoop, 0.f, /*NumLoops=*/0); // 0 = loop infinito
    if (!bWasCovering) OnCovered.Broadcast(); // avisar (una vez) que ya tapa → el que llama puede viajar/cargar
}

void UPTLoadingScreenWidget::PlayOutro()
{
    if (bOutroPlaying) return;
    bOutroPlaying = true;
    if (AnimLoop) StopAnimation(AnimLoop);
    if (!AnimOut) { RemoveFromParent(); return; }
    OutEndDelegate.BindDynamic(this, &UPTLoadingScreenWidget::OnOutFinished);
    BindToAnimationFinished(AnimOut, OutEndDelegate);
    PlayAnimation(AnimOut);
}

void UPTLoadingScreenWidget::OnOutFinished()
{
    RemoveFromParent();
}
