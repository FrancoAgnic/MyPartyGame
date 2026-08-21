// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTPlayerRowWidget.h"
#include "PTPlayerState.h"
#include "PTLobbyPlayerController.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/Button.h"

bool UPTPlayerRowWidget::Initialize()
{
    if (!Super::Initialize()) return false;
    if (KickButton) KickButton->OnClicked.AddDynamic(this, &UPTPlayerRowWidget::OnKickClicked);
    return true;
}

void UPTPlayerRowWidget::SetRow(const FString& Name, bool bHost, bool bReady,
                                const FLinearColor& ReadyColor, const FLinearColor& NotReadyColor, int32 MaxChars,
                                APTPlayerState* Target, bool bCanKick)
{
    TargetPS = Target;

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

    // Kick: solo visible en las filas de OTROS jugadores cuando el local es el host.
    if (KickButton)
        KickButton->SetVisibility(bCanKick ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UPTPlayerRowWidget::OnKickClicked()
{
    if (!TargetPS.IsValid()) return;
    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
        PC->Server_KickPlayer(TargetPS.Get());
}
