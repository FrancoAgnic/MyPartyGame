// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTChatShoutWidget.h"
#include "Components/TextBlock.h"

namespace
{
    // Ease-out cúbico: arranca rápido y DESACELERA hasta frenar (para el movimiento hacia arriba).
    FORCEINLINE float EaseOutCubic(float t) { t = FMath::Clamp(t, 0.f, 1.f); const float u = 1.f - t; return 1.f - u * u * u; }
    // Ease-out "back": llega a 1 pasándose un poco y vuelve → REBOTE al final del crecimiento.
    FORCEINLINE float EaseOutBack(float t)
    {
        t = FMath::Clamp(t, 0.f, 1.f);
        const float c1 = 1.9f;       // cuánto rebota (más alto = rebote más marcado)
        const float c3 = c1 + 1.f;
        const float u = t - 1.f;
        return 1.f + c3 * u * u * u + c1 * u * u;
    }
}

bool UPTChatShoutWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (ShoutText) DefaultTextColor = ShoutText->GetColorAndOpacity().GetSpecifiedColor();
    return true;
}

UWidget* UPTChatShoutWidget::AnimTarget() const
{
    if (ShoutRoot) return ShoutRoot;
    return ShoutText;
}

float UPTChatShoutWidget::ShowShout(const FString& Text, bool bGuess)
{
    if (ShoutText)
    {
        // Tope de caracteres: nada de mega-textos, solo palabras cortas.
        FString Shown = Text;
        if (MaxChars > 0 && Shown.Len() > MaxChars) Shown = Shown.Left(MaxChars - 1) + TEXT("…");
        ShoutText->SetText(FText::FromString(Shown));
        ShoutText->SetColorAndOpacity(FSlateColor(bGuess ? GuessColor : DefaultTextColor));
    }

    // Origen SIEMPRE el mismo (la boca, un poco abajo del centro). Destino ARRIBA pero variado:
    // a un lado u otro y con leve inclinación → si mandás varias, no salen todas iguales ni encimadas.
    StartPos    = FVector2D(0.f, StartY);
    const float Side = FMath::FRandRange(-1.f, 1.f);
    TargetPos   = FVector2D(Side * SpreadX, -(RiseY + FMath::FRandRange(-RiseYVar, RiseYVar)));
    TargetAngle = FMath::FRandRange(-MaxTiltDeg, MaxTiltDeg); // leve; nunca dado vuelta

    AnimTime   = 0.f;
    bAnimating = true;
    if (UWidget* T = AnimTarget()) T->SetRenderTransformPivot(FVector2D(0.5f, 0.5f)); // escalar/rotar desde el centro
    ApplyTransform(0.f);

    return GrowTime + HoldTime + FadeTime;
}

void UPTChatShoutWidget::ApplyTransform(float Time)
{
    UWidget* T = AnimTarget();
    if (!T) return;

    const float Total = GrowTime + HoldTime + FadeTime;
    Time = FMath::Clamp(Time, 0.f, Total);

    // ── Escala: 0→1 rápido CON REBOTE al final del crecimiento; se mantiene en 1 mientras dura. ──
    float Scale = 1.f;
    if (Time < GrowTime) Scale = EaseOutBack(Time / GrowTime);

    // ── Posición: de la boca (abajo) hacia el destino de arriba, DESACELERANDO. Se mueve un poco más
    //    de tiempo que el crecimiento para que se sienta el "frenado". ──
    const float MoveT = EaseOutCubic(Time / (GrowTime * 1.7f));
    FVector2D Pos = FMath::Lerp(StartPos, TargetPos, MoveT);

    // ── Inclinación: 0→ángulo destino desacelerando. ──
    const float Angle = FMath::Lerp(0.f, TargetAngle, EaseOutCubic(Time / GrowTime));

    // ── Opacidad: fade-in muy rápido; sostener; fade-out al final (subiendo un toque más = flotar). ──
    float Opacity = 1.f;
    const float FadeInT = 0.07f;
    if (Time < FadeInT) Opacity = Time / FadeInT;
    const float FadeStart = GrowTime + HoldTime;
    if (Time > FadeStart)
    {
        const float f = FMath::Clamp((Time - FadeStart) / FMath::Max(0.01f, FadeTime), 0.f, 1.f);
        Opacity = 1.f - f;
        Pos.Y  -= DriftUp * f;          // sigue flotando hacia arriba al desvanecerse
        Scale   = FMath::Lerp(1.f, 1.12f, f); // y se agranda un pelín
    }

    FWidgetTransform WT;
    WT.Translation = Pos;
    WT.Scale       = FVector2D(Scale, Scale);
    WT.Angle       = Angle;
    T->SetRenderTransform(WT);
    T->SetRenderOpacity(Opacity);
}

void UPTChatShoutWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!bAnimating) return;

    AnimTime += InDeltaTime;
    ApplyTransform(AnimTime);

    if (AnimTime >= GrowTime + HoldTime + FadeTime)
    {
        bAnimating = false;
        if (UWidget* T = AnimTarget()) T->SetRenderOpacity(0.f); // queda invisible hasta el próximo grito
    }
}
