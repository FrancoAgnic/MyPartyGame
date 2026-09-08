#include "PTNameTagWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Border.h"

bool UPTNameTagWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (NameText) DefaultColor = NameText->GetColorAndOpacity().GetSpecifiedColor();
    if (HostCrown) HostCrown->SetVisibility(ESlateVisibility::Collapsed); // arranca oculta
    // Guardar el marco original del WBP para poder restaurarlo cuando el jugador deja de estar listo.
    if (BorderNameTag) { DefaultBrush = BorderNameTag->Background; bDefaultBrushCaptured = true; }
    return true;
}

void UPTNameTagWidget::SetReadyState(bool bReady)
{
    if (!BorderNameTag) return;
    // Listo → tu marco/material verde (ReadyBrush, asignado en el WBP). No listo → marco original.
    if (bReady)                    BorderNameTag->SetBrush(ReadyBrush);
    else if (bDefaultBrushCaptured) BorderNameTag->SetBrush(DefaultBrush);
}

void UPTNameTagWidget::SetHost(bool bIsHost)
{
    if (HostCrown)
        HostCrown->SetVisibility(bIsHost ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UPTNameTagWidget::SetPlayerName(const FString& InName)
{
    if (!NameText) return;
    NameText->SetText(FText::FromString(InName.Left(10))); // máximo 10 caracteres
    NameText->SetColorAndOpacity(FSlateColor(DefaultColor));
}

void UPTNameTagWidget::ShowMessage(const FString& Msg)
{
    if (!NameText) return;
    NameText->SetText(FText::FromString(Msg.Left(40))); // globo de chat (tope 40)
    NameText->SetColorAndOpacity(FSlateColor(DefaultColor));
}

void UPTNameTagWidget::ShowGuessMessage(const FString& Msg)
{
    if (!NameText) return;
    NameText->SetText(FText::FromString(Msg.Left(40)));
    NameText->SetColorAndOpacity(FSlateColor(FLinearColor::Green));
}
