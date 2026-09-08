// Copyright Epic Games, Inc. All Rights Reserved.
// "Grito" de chat estilo onomatopeya de cómic: aparece sobre la boca del jugador cuando escribe algo,
// con una animación (escala 0→1, se sostiene, desaparece). El NOMBRE del jugador se mantiene siempre
// aparte (esto NO lo reemplaza). Vive en un WidgetComponent atado al personaje → sigue al jugador y lo
// ven todas las máquinas.
//
// En el WBP derivado (parent = UPTChatShoutWidget):
//   ShoutText (TextBlock)          → el texto gritado (lo pone el código).
//   PopAnim   (Widget Animation)   → OPCIONAL: tu animación (escala 0→1, sostener, fade out). Si la
//                                    ponés, define TODO el timing/look; si no, hay un fallback simple.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTChatShoutWidget.generated.h"

class UTextBlock;
class UWidgetAnimation;

UCLASS()
class MYPARTYGAME_API UPTChatShoutWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Muestra el texto y reproduce la animación. Devuelve CUÁNTOS SEGUNDOS dura (para ocultarlo luego).
     *  bGuess = acierto (tiñe el texto con GuessColor). */
    float ShowShout(const FString& Text, bool bGuess);

protected:
    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* ShoutText;
    // Tu animación (nombrala EXACTO "PopAnim" en el WBP). Si existe, define el timing completo.
    UPROPERTY(Transient, meta = (BindWidgetAnimOptional)) UWidgetAnimation* PopAnim = nullptr;

    // Si NO hay PopAnim: cuántos segundos se muestra (y color del acierto). Editables en el WBP.
    UPROPERTY(EditAnywhere, Category = "Shout") float FallbackSeconds = 2.5f;
    UPROPERTY(EditAnywhere, Category = "Shout") FLinearColor GuessColor = FLinearColor(0.f, 1.f, 0.f, 1.f);

    // Color de diseño del texto (del WBP), para restaurarlo cuando NO es acierto.
    FLinearColor DefaultTextColor = FLinearColor::White;
    virtual bool Initialize() override;
};
