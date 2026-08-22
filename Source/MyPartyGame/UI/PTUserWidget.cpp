// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTUserWidget.h"
#include "../PTGameInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace { constexpr float PT_POPIN_STEP = 1.f / 60.f; } // paso del timer (~60 fps)

void UPTUserWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Aplicar los sonidos de UI (definidos en el GameInstance) a todos los botones de este widget.
    // Cada widget que deriva de esta base lo hace solo → no hay que tocar botón por botón, y los
    // widgets creados dinámicamente (filas de sesión, slots, etc.) también quedan cubiertos.
    if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
        GI->ApplyUIButtonSounds(this);

    if (bAutoPopIn) PlayPopIn();
}

void UPTUserWidget::NativeDestruct()
{
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(PopInTimer);
    Super::NativeDestruct();
}

void UPTUserWidget::PlayPopIn()
{
    UWorld* W = GetWorld();
    if (!W) return;

    // Pivote en el centro para que escale desde el medio; arranca en 0.
    SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    SetRenderScale(FVector2D(0.f, 0.f));
    PopInElapsed = 0.f;

    W->GetTimerManager().SetTimer(PopInTimer, this, &UPTUserWidget::PopInStep, PT_POPIN_STEP, /*loop=*/true);
}

void UPTUserWidget::PopInStep()
{
    PopInElapsed += PT_POPIN_STEP;
    const float Dur = FMath::Max(0.05f, PopInDuration);
    const float T   = FMath::Clamp(PopInElapsed / Dur, 0.f, 1.f);

    // Ease "back out": sobrepasa un poco 1.0 y vuelve → efecto de rebote/blop.
    const float C1 = 1.70158f;
    const float C3 = C1 + 1.f;
    const float U  = T - 1.f;
    const float Scale = 1.f + C3 * U * U * U + C1 * U * U;

    SetRenderScale(FVector2D(Scale, Scale));

    if (T >= 1.f)
    {
        SetRenderScale(FVector2D(1.f, 1.f)); // asentar exacto
        if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(PopInTimer);
    }
}
