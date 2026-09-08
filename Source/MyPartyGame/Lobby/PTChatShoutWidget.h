// Copyright Epic Games, Inc. All Rights Reserved.
// "Grito" de chat estilo onomatopeya de cómic: aparece sobre la boca del jugador cuando escribe algo,
// crece de 0→1 con un rebote, se acomoda ARRIBA (en una posición/rotación al azar para que varios no se
// pisen), se desacelera y se desvanece. El NOMBRE del jugador se mantiene siempre aparte.
//
// La animación está hecha POR CÓDIGO (NativeTick) para poder variar posición/rotación en cada grito.
//
// En el WBP derivado (parent = UPTChatShoutWidget):
//   ShoutText  (TextBlock)  → el texto gritado (lo pone el código).
//   ShoutRoot  (opcional)   → contenedor a animar (si lo ponés, se anima ESTE; si no, se anima ShoutText).
//                             Poné acá el fondo + texto para que se muevan juntos.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTChatShoutWidget.generated.h"

class UTextBlock;
class UWidget;

UCLASS()
class MYPARTYGAME_API UPTChatShoutWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Muestra el texto y arranca la animación procedural. Devuelve cuántos segundos dura (para ocultarlo). */
    float ShowShout(const FString& Text, bool bGuess);

protected:
    virtual bool Initialize() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* ShoutText;
    UPROPERTY(meta = (BindWidgetOptional)) UWidget*    ShoutRoot; // contenedor a animar (si existe)

    // ── Tuneables (editables en el WBP → categoría "Shout") ──
    UPROPERTY(EditAnywhere, Category="Shout") float GrowTime  = 0.26f;  // 0→1 (con rebote) rápido
    UPROPERTY(EditAnywhere, Category="Shout") float HoldTime  = 1.7f;   // se sostiene arriba
    UPROPERTY(EditAnywhere, Category="Shout") float FadeTime  = 0.55f;  // se desvanece
    UPROPERTY(EditAnywhere, Category="Shout") float StartY    = 40.f;   // desde la boca (abajo, +Y)
    UPROPERTY(EditAnywhere, Category="Shout") float RiseY     = 70.f;   // hasta ARRIBA del nombre (-Y)
    UPROPERTY(EditAnywhere, Category="Shout") float RiseYVar  = 25.f;   // variación vertical del destino
    UPROPERTY(EditAnywhere, Category="Shout") float SpreadX   = 85.f;   // cuánto se corre a los lados
    UPROPERTY(EditAnywhere, Category="Shout") float MaxTiltDeg = 12.f;  // inclinación máxima (leve, legible)
    UPROPERTY(EditAnywhere, Category="Shout") float DriftUp   = 26.f;   // cuánto sigue subiendo al desvanecerse
    UPROPERTY(EditAnywhere, Category="Shout") FLinearColor GuessColor = FLinearColor(0.f, 1.f, 0.f, 1.f);

private:
    UWidget* AnimTarget() const; // ShoutRoot si existe, si no ShoutText
    void ApplyTransform(float Time);

    FLinearColor DefaultTextColor = FLinearColor::White;
    bool     bAnimating   = false;
    float    AnimTime     = 0.f;
    FVector2D StartPos    = FVector2D::ZeroVector;
    FVector2D TargetPos   = FVector2D::ZeroVector;
    float    TargetAngle  = 0.f;
};
