// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMapDownloadPromptWidget.h"
#include "../Lobby/PTLobbyPlayerController.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UPTMapDownloadPromptWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (DownloadButton) DownloadButton->OnClicked.AddDynamic(this, &UPTMapDownloadPromptWidget::OnDownloadClicked);
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTMapDownloadPromptWidget::ShowFor(const FString& MapTitle)
{
    if (MapNameText) MapNameText->SetText(FText::FromString(MapTitle));
    SetVisibility(ESlateVisibility::Visible);
    PlayPopIn();
}

void UPTMapDownloadPromptWidget::HidePanel()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

void UPTMapDownloadPromptWidget::OnDownloadClicked()
{
    // Descargar por Steam (lo maneja el PlayerController) y cerrar el popup. Mientras baja, la fila del
    // jugador muestra el logo "descargando".
    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
        PC->ConfirmMapDownload();
    HidePanel();
}
