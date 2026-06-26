// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyEscapeMenuWidget.h"
#include "PTSettingsWidget.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

bool UPTLobbyEscapeMenuWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (LeaveGameButton) LeaveGameButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnLeaveGameClicked);
    if (SettingsButton)  SettingsButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnSettingsClicked);
    if (ResumeButton)    ResumeButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnResumeClicked);

    SetVisibility(ESlateVisibility::Collapsed);
    return true;
}

bool UPTLobbyEscapeMenuWidget::IsMenuOpen() const
{
    return GetVisibility() != ESlateVisibility::Collapsed;
}

void UPTLobbyEscapeMenuWidget::ToggleMenu()
{
    const bool bOpen = !IsMenuOpen();
    SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (bOpen)
        {
            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputMode);
            PC->SetShowMouseCursor(true);
        }
        else
        {
            PC->SetInputMode(FInputModeGameOnly());
            PC->SetShowMouseCursor(false);
        }
    }
}

void UPTLobbyEscapeMenuWidget::OnLeaveGameClicked()
{
    // OpenLevel local desconecta del server (cierra la conexión); en el servidor esto
    // dispara APTLobbyGameMode::Logout, que ya limpia la sesión si era el último jugador.
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

void UPTLobbyEscapeMenuWidget::OnSettingsClicked()
{
    if (SettingsPanel) SettingsPanel->ShowPanel();
}

void UPTLobbyEscapeMenuWidget::OnResumeClicked()
{
    ToggleMenu();
}
