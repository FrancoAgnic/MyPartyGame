// Copyright Epic Games, Inc. All Rights Reserved.
// Clase base de TODOS los widgets de UI del juego. Trabajo extra:
//  1) al construirse, aplica los sonidos de UI (hover/click) del GameInstance a TODOS los botones.
//  2) provee PlayPopIn(): animación "blop" (escala 0→1 con un pequeño rebote) para popups.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTUserWidget.generated.h"

UCLASS()
class MYPARTYGAME_API UPTUserWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Reproduce la animación "blop": escala el widget de 0 a 1 con un pequeño rebote (overshoot).
     *  Los popups la llaman al mostrarse (en ShowPanel / Setup / NativeConstruct). Va por TIMER (no
     *  depende de NativeTick, que en modo Auto puede no correr si el WBP no implementa Tick). */
    UFUNCTION(BlueprintCallable, Category="PopIn")
    void PlayPopIn();
    /** Igual que PlayPopIn pero animando un WIDGET HIJO (p.ej. un panel interno como Game Settings),
     *  no el widget entero. El timer lo maneja este widget. */
    UFUNCTION(BlueprintCallable, Category="PopIn")
    void PlayPopInOn(class UWidget* Target);

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // Duración del blop (segundos). Editable por si se quiere más rápido/lento.
    UPROPERTY(EditAnywhere, Category="PopIn") float PopInDuration = 0.5f;
    // Si es true, el blop se reproduce solo al construirse (para popups que se crean ya visibles).
    UPROPERTY(EditAnywhere, Category="PopIn") bool  bAutoPopIn = false;

private:
    void PopInStep();

    FTimerHandle PopInTimer;
    float PopInElapsed = 0.f;
    TWeakObjectPtr<class UWidget> PopInTarget; // qué se anima (this por defecto, o un hijo)
};
