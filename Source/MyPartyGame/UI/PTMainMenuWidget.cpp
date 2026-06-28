// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMainMenuWidget.h"
#include "MultiplayerSessionsSubsystem.h"
#include "PTCreateSessionWidget.h"
#include "PTFindSessionsWidget.h"
#include "PTEnterCodeWidget.h"
#include "PTSettingsWidget.h"
#include "PTGameUserSettings.h"
#include "PTGameInstance.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformApplicationMisc.h"
#include "GenericPlatform/GenericPlatformHttp.h"

// ==========================================================================
// Inicialización
// ==========================================================================

bool UPTMainMenuWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (PlayButton)      PlayButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnPlayClicked);
    if (PlayBackButton)  PlayBackButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnPlayBackClicked);
    if (HostButton)      HostButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnHostClicked);
    if (FindButton)      FindButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnFindClicked);
    if (EnterCodeButton) EnterCodeButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnEnterCodeClicked);
    if (QuitButton)      QuitButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnQuitClicked);
    if (SettingsButton)  SettingsButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnSettingsClicked);

    // Si hay un PlayButton, arrancar en la pantalla principal (submenú Host/Find/EnterCode oculto).
    // Si el WBP todavía no tiene PlayButton, no se toca nada (comportamiento previo, todo visible).
    if (PlayButton) SetPlaySubmenuVisible(false);

    return true;
}

void UPTMainMenuWidget::MenuSetup(int32 InNumPublicConnections, FString InLobbyPath)
{
    NumPublicConnections = InNumPublicConnections;
    LobbyPath            = InLobbyPath;

    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);
    SetIsFocusable(true);

    // Settings persistentes (volumen/idioma): aplicar lo guardado al volver al menú.
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->ApplyAudioAndLanguage(GetWorld());
    }

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

void UPTMainMenuWidget::OnPlayClicked()
{
    SetPlaySubmenuVisible(true);
}

void UPTMainMenuWidget::OnPlayBackClicked()
{
    SetPlaySubmenuVisible(false);
}

void UPTMainMenuWidget::SetPlaySubmenuVisible(bool bVisible)
{
    const ESlateVisibility Shown  = ESlateVisibility::Visible;
    const ESlateVisibility Hidden = ESlateVisibility::Collapsed;

    // Pantalla "PLAY": Host/Find/EnterCode + su título + Back.
    if (HostButton)             HostButton->SetVisibility(bVisible ? Shown : Hidden);
    if (FindButton)             FindButton->SetVisibility(bVisible ? Shown : Hidden);
    if (EnterCodeButton)        EnterCodeButton->SetVisibility(bVisible ? Shown : Hidden);
    if (PlayBackButton)         PlayBackButton->SetVisibility(bVisible ? Shown : Hidden);
    if (PlaySubmenuHeaderPanel) PlaySubmenuHeaderPanel->SetVisibility(bVisible ? Shown : Hidden);

    // Pantalla principal: Play/Settings/Exit + título — se ocultan mientras está abierto "PLAY".
    if (PlayButton)          PlayButton->SetVisibility(bVisible ? Hidden : Shown);
    if (SettingsButton)      SettingsButton->SetVisibility(bVisible ? Hidden : Shown);
    if (QuitButton)          QuitButton->SetVisibility(bVisible ? Hidden : Shown);
    if (MainMenuHeaderPanel) MainMenuHeaderPanel->SetVisibility(bVisible ? Hidden : Shown);
}

void UPTMainMenuWidget::OnHostClicked()
{
    if (CreatePanel)
    {
        // Mostrar sub-panel con campo de nombre/privada/jugadores.
        CreatePanel->ShowPanel(NumPublicConnections);
    }
    else if (Sessions)
    {
        // Fallback si no hay CreatePanel en el WBP: crear pública con valores por defecto.
        Sessions->CreateSession(NumPublicConnections, false);
    }
}

void UPTMainMenuWidget::OnFindClicked()
{
    if (FindPanel) FindPanel->ShowPanel();
    if (Sessions)  Sessions->FindSessions(20);
}

void UPTMainMenuWidget::OnEnterCodeClicked()
{
    if (EnterCodePanel) EnterCodePanel->ShowPanel();
}

void UPTMainMenuWidget::OnQuitClicked()
{
    if (APlayerController* PC = GetOwningPlayer())
        UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
}

void UPTMainMenuWidget::OnSettingsClicked()
{
    if (SettingsPanel) SettingsPanel->ShowPanel();
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
        if (ErrorText) ErrorText->SetText(FText::FromString(TEXT("No se pudo crear la sesión")));
        return;
    }

    // Fase 5 — si la sesión es privada, copiar el código al portapapeles para compartirlo.
    if (Sessions)
    {
        const FString Code = Sessions->GetGeneratedSessionCode();
        if (!Code.IsEmpty())
        {
            FPlatformApplicationMisc::ClipboardCopy(*Code);
            UE_LOG(LogTemp, Log, TEXT("[Menu] Sesión privada — código (copiado al portapapeles): %s"), *Code);
            if (GeneratedCodeText)
            {
                GeneratedCodeText->SetText(FText::FromString(
                    FString::Printf(TEXT("Código: %s (copiado al portapapeles)"), *Code)));
            }
        }
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
            // "Name" lo reconoce el motor nativamente (AGameModeBase::InitNewPlayer) y deja el
            // nombre real de Steam en PlayerState->GetPlayerName() antes de PostLogin.
            TravelURL += TEXT("&Name=") + FGenericPlatformHttp::UrlEncode(Sessions->GetLocalPlayerDisplayName());
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
        if (ErrorText)
        {
            const FString Msg = (Result == EOnJoinSessionCompleteResult::SessionDoesNotExist)
                ? TEXT("Invalid Code")
                : TEXT("No se pudo unir a la sesión");
            ErrorText->SetText(FText::FromString(Msg));

            if (UWorld* World = GetWorld())
            {
                World->GetTimerManager().SetTimer(ErrorTextTimerHandle, this, &UPTMainMenuWidget::HideErrorText, 2.f, false);
            }
        }
        return;
    }

    if (!Sessions) return;

    FString ConnectString;
    if (Sessions->GetResolvedConnectString(ConnectString))
    {
        const FString TravelURL = ConnectString
            + TEXT("?Password=")
            + Sessions->GetPendingJoinPassword()
            + TEXT("&Name=") + FGenericPlatformHttp::UrlEncode(Sessions->GetLocalPlayerDisplayName());

        if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
        {
            // Guardar la URL para que el GameInstance pueda reintentar si la conexión se cae.
            if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
            {
                GI->NotifyJoinedServer(TravelURL);
            }

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

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(ErrorTextTimerHandle);
    }
}

void UPTMainMenuWidget::HideErrorText()
{
    if (ErrorText) ErrorText->SetText(FText::GetEmpty());
}
