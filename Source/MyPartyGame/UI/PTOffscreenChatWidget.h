// Copyright Epic Games, Inc. All Rights Reserved.
// Globito de chat en el BORDE del HUD: aparece cuando el jugador que habló está FUERA de pantalla o a tu
// espalda, apuntando en su dirección. Lo crea y posiciona UPTGameplayHUDWidget en un CanvasPanel.
//
// En el WBP derivado (nombres EXACTOS, todo opcional):
//   TxtMessage (TextBlock) → el texto del mensaje.
//   Arrow (Image/Widget)   → flechita que apunta hacia el jugador (se la rota por el ángulo). Opcional.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTOffscreenChatWidget.generated.h"

class UTextBlock;
class UWidget;

UCLASS()
class MYPARTYGAME_API UPTOffscreenChatWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    /** Setea el texto, si es acierto (para teñir distinto en BP vía OnBubbleSet), y el ángulo (grados) de
     *  la flecha hacia el jugador (0 = derecha, horario). */
    void SetBubble(const FString& Text, bool bGuess, float ArrowAngleDeg);

    // Estado de seguimiento (lo maneja el HUD): a qué jugador sigue este globito, su posición suavizada
    // actual (para el lag) y si ya fue inicializada (para snap al aparecer en vez de deslizar desde 0).
    TWeakObjectPtr<AActor> OwnerActor;
    FVector2D SmoothPos = FVector2D::ZeroVector;
    bool      bSmoothInit = false;

protected:
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtMessage;
    UPROPERTY(meta=(BindWidgetOptional)) UWidget*    Arrow;

    /** Gancho para el BP: tiñe/anima según si es acierto. Lo llama SetBubble. */
    UFUNCTION(BlueprintImplementableEvent, Category="Chat")
    void OnBubbleSet(bool bGuess);
};
