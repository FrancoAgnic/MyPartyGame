// Copyright Epic Games, Inc. All Rights Reserved.
// Pantalla de carga con 3 animaciones (IN → LOOP → OUT) para cubrir la entrada a un nivel sin pantalla negra.
// Reparentá tu WBP_LoadingScreen a esta clase y nombrá las animaciones EXACTO: AnimIn, AnimLoop, AnimOut.
//   · AnimIn   : transición de entrada (se toca 1 vez al aparecer).
//   · AnimLoop : queda en loop desde donde terminó AnimIn hasta que el nivel carga.
//   · AnimOut  : transición de salida (revela el nivel); al terminar, el widget se auto-remueve.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTLoadingScreenWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPTOnLoadingCovered); // se dispara cuando el AnimIn terminó (pantalla tapada)

UCLASS()
class MYPARTYGAME_API UPTLoadingScreenWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Se dispara una vez cuando el AnimIn terminó y la pantalla ya tapa (arrancó el loop). Úsalo para hacer
     *  el travel/carga RECIÉN cuando la transición de entrada ya cubrió la pantalla. */
    UPROPERTY(BlueprintAssignable) FPTOnLoadingCovered OnCovered;
    /** Arranca la secuencia: AnimIn y, al terminar, AnimLoop en loop. Si bStartAtLoop (o no hay AnimIn),
     *  arranca directo en el loop (para cuando el IN ya se mostró antes del travel). */
    void PlayIntro(bool bStartAtLoop = false);
    /** Reproduce AnimOut y, al terminar, se remueve solo. Idempotente. */
    void PlayOutro();
    bool IsOutroPlaying() const { return bOutroPlaying; }
    /** true cuando el AnimIn ya terminó y la pantalla está TAPADA (en loop). Antes de esto no conviene cargar
     *  el nivel, porque durante la transición de entrada todavía se ve la escena detrás. */
    bool IsCovering() const { return bCovering; }

protected:
    UPROPERTY(Transient, meta=(BindWidgetAnimOptional)) UWidgetAnimation* AnimIn   = nullptr;
    UPROPERTY(Transient, meta=(BindWidgetAnimOptional)) UWidgetAnimation* AnimLoop = nullptr;
    UPROPERTY(Transient, meta=(BindWidgetAnimOptional)) UWidgetAnimation* AnimOut  = nullptr;

    UFUNCTION() void OnInFinished();
    UFUNCTION() void OnOutFinished();

    FWidgetAnimationDynamicEvent InEndDelegate;
    FWidgetAnimationDynamicEvent OutEndDelegate;
    bool bOutroPlaying = false;
    bool bCovering = false; // true una vez que arrancó el loop (AnimIn terminó)

    void StartLoop();
};
