// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMainMenuWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "PTCreateSessionWidget.h"
#include "PTFindSessionsWidget.h"
#include "PTGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"

// ==========================================================================
// Inicialización
// ==========================================================================

bool UPTMainMenuWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (HostButton) HostButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnHostClicked);
    if (FindButton) FindButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnFindClicked);
    if (QuitButton) QuitButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnQuitClicked);

    return true;
}

void UPTMainMenuWidget::MenuSetup(int32 InNumPublicConnections, FString InLobbyPath)
{
    NumPublicConnections = InNumPublicConnections;
    LobbyPath            = InLobbyPath;

    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);
    SetIsFocusable(true);

    // Capturar mouse y bloquear input de juego mientras el menú está abierto.
    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            FInputModeUIOnly InputMode;
            InputMode.SetWidgetToFocus(TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputMode);
            PC->SetShowMouseCursor(true);
        }
    }

    // Fase 4 — Mostrar error de conexión previo si lo hay (ej: contraseña incorrecta).
    if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
    {
        const FString Err = GI->ConsumePendingConnectError();
        if (!Err.IsEmpty())
        {
            if (ErrorText) ErrorText->SetText(FText::FromString(Err));
            UE_LOG(LogTemp, Warning, TEXT("[Menu] Error de conexión: %s"), *Err);
        }
    }

    // Obtener el subsistema y suscribir delegates.
    if (UGameInstance* GI = GetGameInstance())
        Sessions = GI->GetSubsystem<UMultiplayerSessionsSubsystem>();

    if (Sessions)
    {
        Sessions->OnLoginComplete.AddUObject(this, &UPTMainMenuWidget::OnLogin);
        Sessions->OnCreateSessionComplete.AddUObject(this, &UPTMainMenuWidget::OnCreateSession);
        Sessions->OnFindSessionsComplete.AddUObject(this, &UPTMainMenuWidget::OnFindSessions);
        Sessions->OnJoinSessionComplete.AddUObject(this, &UPTMainMenuWidget::OnJoinSession);

        // Botones deshabilitados hasta recibir OnLogin(true).
        if (HostButton) HostButton->SetIsEnabled(false);
        if (FindButton) FindButton->SetIsEnabled(false);

        Sessions->Login();
    }
}

void UPTMainMenuWidget::NativeDestruct()
{
    MenuTearDown();
    Super::NativeDestruct();
}

// ==========================================================================
// Handlers de botones
// ==========================================================================

void UPTMainMenuWidget::OnHostClicked()
{
    if (CreatePanel)
    {
        // Mostrar sub-panel con campo de nombre/password/jugadores.
        CreatePanel->ShowPanel(NumPublicConnections);
    }
    else if (Sessions)
    {
        // Fallback si no hay CreatePanel en el WBP: crear con valores por defecto.
        Sessions->CreateSession(NumPublicConnections, TEXT("TestRoom"), TEXT(""));
    }
}

void UPTMainMenuWidget::OnFindClicked()
{
    if (FindPanel) FindPanel->ShowPanel();
    if (Sessions)  Sessions->FindSessions(20);
}

void UPTMainMenuWidget::OnQuitClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
        UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
}

// ==========================================================================
// Callbacks del subsistema
// ==========================================================================

void UPTMainMenuWidget::OnLogin(bool bWasSuccessful)
{
    if (HostButton) HostButton->SetIsEnabled(bWasSuccessful);
    if (FindButton) FindButton->SetIsEnabled(bWasSuccessful);
    // TODO Fase UI: si !bWasSuccessful mostrar mensaje de error.
}

void UPTMainMenuWidget::OnCreateSession(bool bWasSuccessful)
{
    if (!bWasSuccessful)
    {
        // TODO Fase UI: mostrar error al usuario.
        return;
    }

    if (UWorld* World = GetWorld())
    {
        MenuTearDown();

        // Host viaja al lobby como listen server. Su propia conexión local también
        // pasa por PreLogin, así que si la sesión tiene contraseña hay que incluirla
        // o PTLobbyGameMode::PreLogin rechaza al propio host.
        FString TravelURL = LobbyPath + TEXT("?listen");
        if (Sessions)
        {
            const FString HostPassword = Sessions->GetPendingHostPassword();
            if (!HostPassword.IsEmpty())
            {
                TravelURL += TEXT("?Password=") + HostPassword;
            }
        }
        World->ServerTravel(TravelURL);
    }
}

void UPTMainMenuWidget::OnFindSessions(
    const TArray<FOnlineSessionSearchResult>& Results, bool bWasSuccessful)
{
    // Reenviar resultados al panel de búsqueda para que pinte las filas.
    if (FindPanel) FindPanel->PopulateResults(Results, bWasSuccessful);
}

void UPTMainMenuWidget::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        // TODO Fase UI: mostrar "No se pudo unir a la sesión".
        return;
    }

    if (!Sessions) return;

    FString ConnectString;
    if (Sessions->GetResolvedConnectString(ConnectString))
    {
        const FString TravelURL = ConnectString
            + TEXT("?Password=")
            + Sessions->GetPendingJoinPassword();

        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            MenuTearDown();
            // Cliente viaja al servidor con la contraseña en la URL (validación real en Fase 4).
            PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
        }
    }
}

// ==========================================================================
// Limpieza
// ==========================================================================

void UPTMainMenuWidget::MenuTearDown()
{
    RemoveFromParent();

    if (UWorld* World = GetWorld())
    {
        if (APlayerController* PC = World->GetFirstPlayerController())
        {
            FInputModeGameOnly InputMode;
            PC->SetInputMode(InputMode);
            PC->SetShowMouseCursor(false);
        }
    }

    // Desuscribir delegates para evitar callbacks huérfanos.
    if (Sessions)
    {
        Sessions->OnLoginComplete.RemoveAll(this);
        Sessions->OnCreateSessionComplete.RemoveAll(this);
        Sessions->OnFindSessionsComplete.RemoveAll(this);
        Sessions->OnJoinSessionComplete.RemoveAll(this);
    }
}
