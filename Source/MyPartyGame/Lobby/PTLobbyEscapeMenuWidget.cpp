// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyEscapeMenuWidget.h"
#include "PTSettingsWidget.h"
#include "PTLobbyGameMode.h"
#include "../PTGameInstance.h"
#include "../Mods/PTMapAuthorGameMode.h"
#include "../Sculpt/PTSculptVolume.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Kismet/GameplayStatics.h"

bool UPTLobbyEscapeMenuWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (LeaveGameButton) LeaveGameButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnLeaveGameClicked);
    if (SettingsButton)  SettingsButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnSettingsClicked);
    if (ResumeButton)    ResumeButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnResumeClicked);
    if (SaveMapButton)   SaveMapButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnSaveMapClicked);

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

    // "Guardar mapa" solo tiene sentido en el modo autoría de mapa.
    if (SaveMapButton)
        SaveMapButton->SetVisibility(IsMapAuthorMode() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

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

void UPTLobbyEscapeMenuWidget::HandleEscape()
{
    // Si el settings está abierto → volver atrás (cerrar settings, seguir en el menú).
    if (SettingsPanel && SettingsPanel->GetVisibility() != ESlateVisibility::Collapsed)
    {
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    // Si no, alternar el menú completo. Al abrir, asegurar el settings oculto.
    ToggleMenu();
    if (IsMenuOpen() && SettingsPanel)
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTLobbyEscapeMenuWidget::OnLeaveGameClicked()
{
    UWorld* World = GetWorld();

    // Si el que se va es el ANFITRIÓN, no se puede cerrar el mundo de una: eso deja a los clientes
    // con la conexión muerta y se les cae el juego. El GameMode los saca primero y después cierra.
    if (World && World->GetNetMode() == NM_ListenServer)
    {
        if (APTLobbyGameMode* GM = World->GetAuthGameMode<APTLobbyGameMode>())
        {
            GM->HostLeaveGame();
            return;
        }
    }

    // Cliente (o partida local): OpenLevel desconecta del server; en el servidor eso dispara
    // APTLobbyGameMode::Logout, que ya limpia la sesión si era el último jugador.
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

bool UPTLobbyEscapeMenuWidget::IsMapAuthorMode() const
{
    return GetWorld() && Cast<APTMapAuthorGameMode>(GetWorld()->GetAuthGameMode()) != nullptr;
}

void UPTLobbyEscapeMenuWidget::OnSaveMapClicked()
{
    // Guarda el escenario esculpido (geometría + pintura) al archivo de trabajo local. Igual que el
    // botón Guardar del HUD; acá vive en el menú de pausa (ESC) para el modo autoría.
    UWorld* W = GetWorld();
    UPTGameInstance* GI = W ? Cast<UPTGameInstance>(W->GetGameInstance()) : nullptr;
    APTSculptVolume* Vol = W ? Cast<APTSculptVolume>(
        UGameplayStatics::GetActorOfClass(W, APTSculptVolume::StaticClass())) : nullptr;
    if (!GI || !Vol) return;
    TArray<uint8> Blob;
    Vol->SaveSnapshot(Blob);
    GI->SaveAuthoredMap(Blob);
}
