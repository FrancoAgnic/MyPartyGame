// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTEnterCodeWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"

bool UPTEnterCodeWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (JoinButton) JoinButton->OnClicked.AddDynamic(this, &UPTEnterCodeWidget::OnJoinClicked);
    if (BackButton) BackButton->OnClicked.AddDynamic(this, &UPTEnterCodeWidget::OnBackClicked);

    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    return true;
}

void UPTEnterCodeWidget::ShowPanel()
{
    if (CodeInput) CodeInput->SetText(FText::GetEmpty());
    SetVisibility(ESlateVisibility::Visible);
}

void UPTEnterCodeWidget::OnJoinClicked()
{
    // OBSOLETO: el sistema de sesiones por código se eliminó (ahora hay Públicas y Privadas-solo-amigos).
    // Este widget queda inerte; borrar el WBP_EnterCode y su botón "Unirse con código" del menú en el editor.
    UE_LOG(LogTemp, Warning, TEXT("[PTEnterCode] Sesiones por código eliminadas; este panel ya no hace nada."));
}

void UPTEnterCodeWidget::OnBackClicked()
{
    SetVisibility(ESlateVisibility::Collapsed);
}
