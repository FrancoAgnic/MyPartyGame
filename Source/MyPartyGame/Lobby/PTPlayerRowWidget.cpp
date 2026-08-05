// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTPlayerRowWidget.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"

void UPTPlayerRowWidget::SetRow(const FString& Name, bool bHost, bool bReady,
                                const FLinearColor& ReadyColor, const FLinearColor& NotReadyColor, int32 MaxChars)
{
    if (NameText)
    {
        FString N = Name;
        if (MaxChars > 0 && N.Len() > MaxChars) N = N.Left(MaxChars - 1) + TEXT("…"); // recortar para no salirse
        NameText->SetText(FText::FromString(N));
    }

    // Corona SOLO para el anfitrión.
    if (HostCrown)
        HostCrown->SetVisibility(bHost ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

    // Check: textura según estado + tinte verde(listo)/rojo(no listo), igual que el botón Listo.
    if (ReadyCheck)
    {
        if (UTexture2D* Tex = bReady ? ReadyTex : NotReadyTex) ReadyCheck->SetBrushFromTexture(Tex);
        ReadyCheck->SetColorAndOpacity(bReady ? ReadyColor : NotReadyColor);
    }
}
