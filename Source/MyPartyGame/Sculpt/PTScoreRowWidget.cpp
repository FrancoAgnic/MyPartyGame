#include "PTScoreRowWidget.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

void UPTScoreRowWidget::ApplyScoreText(int32 Value)
{
    if (TxtScore)
    {
        // Puntaje separado: TxtName = solo el nombre, el número va en TxtScore (siempre visible).
        const FText Num = FText::AsNumber(Value);
        TxtScore->SetText(Num);
        if (TxtScorePop) TxtScorePop->SetText(Num); // el fantasma muestra el mismo número
        if (TxtName) TxtName->SetText(FText::FromString(RowName));
    }
    else if (TxtName)
    {
        // Compatibilidad: sin TxtScore, todo junto "Nombre: puntaje".
        TxtName->SetText(FText::FromString(FString::Printf(TEXT("%s: %d"), *RowName, Value)));
    }
}

void UPTScoreRowWidget::SetRow(const FString& Name, int32 Score, bool bSculptor)
{
    RowName    = Name.Left(10);
    FinalScore = Score;
    if (TxtName)
    {
        if (!bAnimating) ApplyScoreText(Score); // si está animando el conteo, no pisar el número en curso

        // Contorno amarillo solo para el escultor (0 = sin contorno para el resto).
        FSlateFontInfo Font = TxtName->GetFont();
        Font.OutlineSettings.OutlineSize  = bSculptor ? SculptorOutlineSize : 0;
        Font.OutlineSettings.OutlineColor = SculptorOutlineColor;
        TxtName->SetFont(Font);
    }

    // La imagen del pincel solo se ve en la fila del que esculpe.
    if (ImgBrush)
        ImgBrush->SetVisibility(bSculptor ? ESlateVisibility::HitTestInvisible
                                          : ESlateVisibility::Collapsed);

    // El "+N" arranca oculto (el HUD lo prende al reconstruir si este jugador acaba de adivinar).
    if (TxtGuessPlus) TxtGuessPlus->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTScoreRowWidget::SetGuessPlus(int32 Points, bool bShow)
{
    if (!TxtGuessPlus) return;
    if (bShow)
    {
        TxtGuessPlus->SetText(FText::FromString(FString::Printf(TEXT("+%d"), Points)));
        TxtGuessPlus->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    else
    {
        TxtGuessPlus->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UPTScoreRowWidget::AnimateScore(int32 From, int32 To, float Duration)
{
    AnimFrom     = From;
    AnimTo       = To;
    AnimDuration = FMath::Max(0.05f, Duration);
    AnimElapsed  = 0.f;
    LastShown    = From;
    bAnimating   = true;
    LastPopAt    = -1.f;
    ApplyScoreText(From);
    if (ScorePopAnim) PlayAnimation(ScorePopAnim); // primer rebote (tu animación) …
    else              NumScale = 1.35f;            // … o el fallback por código
}

void UPTScoreRowWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    if (bAnimating)
    {
        AnimElapsed += InDeltaTime;
        const float a = FMath::Clamp(AnimElapsed / AnimDuration, 0.f, 1.f);
        const float e = 1.f - (1.f - a) * (1.f - a); // easeOut: rápido al principio, frena al final
        const int32 Cur = FMath::RoundToInt(FMath::Lerp((float)AnimFrom, (float)AnimTo, e));
        if (Cur != LastShown)
        {
            LastShown = Cur;
            ApplyScoreText(Cur);
            if (SndPointTick) UGameplayStatics::PlaySound2D(this, SndPointTick); // 1 tick por punto que sube
            // Rebote en cada subida: tu animación UMG (si la asignaste) o el fallback por código.
            if (ScorePopAnim)
            {
                if (AnimElapsed - LastPopAt > 0.06f) { PlayAnimation(ScorePopAnim); LastPopAt = AnimElapsed; }
            }
            else
            {
                NumScale = 1.35f;
            }
        }
        if (a >= 1.f) { bAnimating = false; ApplyScoreText(AnimTo); }
    }

    // Fallback por código: el número vuelve suavemente a escala 1 (solo si NO usás tu animación UMG).
    if (!ScorePopAnim && !FMath::IsNearlyEqual(NumScale, 1.f, 0.01f))
    {
        NumScale = FMath::FInterpTo(NumScale, 1.f, InDeltaTime, 14.f);
        UWidget* Target = TxtScore ? (UWidget*)TxtScore : (UWidget*)TxtName;
        if (Target)
        {
            Target->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
            Target->SetRenderScale(FVector2D(NumScale, NumScale));
        }
    }
}
