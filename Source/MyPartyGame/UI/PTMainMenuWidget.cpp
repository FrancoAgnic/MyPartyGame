// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMainMenuWidget.h"
#include "../Lobby/PTLobbyPlayerController.h"
#include "MultiplayerSessionsSubsystem.h"
#include "../PTTextTable.h"
#include "PTCreateSessionWidget.h"
#include "PTFindSessionsWidget.h"
#include "PTEnterCodeWidget.h"
#include "PTSettingsWidget.h"
#include "PTGameUserSettings.h"
#include "PTGameInstance.h"
#include "PTLobbyGameMode.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"

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
    if (LockerButton)    LockerButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnLockerClicked);

    // Si hay un PlayButton, arrancar en la pantalla principal (submenú Host/Find/EnterCode oculto).
    // Si el WBP todavía no tiene PlayButton, no se toca nada (comportamiento previo, todo visible).
    if (PlayButton) SetPlaySubmenuVisible(false);

    // Número de versión (desde ProjectVersion de DefaultGame.ini, que se sube en cada build).
    if (VersionText)
    {
        FString Ver;
        GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),
                           TEXT("ProjectVersion"), Ver, GGameIni);
        if (Ver.IsEmpty()) Ver = TEXT("0.0.0");
        VersionText->SetText(FText::FromString(FString::Printf(TEXT("v%s"), *Ver)));
    }

    return true;
}

void UPTMainMenuWidget::MenuSetup(int32 InNumPublicConnections, FString InLobbyPath)
{
    NumPublicConnections = InNumPublicConnections;
    LobbyPath            = InLobbyPath;

    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);
    // No SetIsFocusable(true) acá: el teclado tiene que quedar para el juego (WASD/Space), no
    // para este widget. Los botones se clickean con mouse únicamente (ver nota de arriba).

    // Settings persistentes (volumen/idioma): aplicar lo guardado al volver al menú.
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->ApplyAudioAndLanguage(GetWorld());
    }

    // NOTA (lobby interactivo): no tocar el input mode ni el cursor acá. El PlayerController
    // (APTLobbyPlayerController::BeginPlay) ya deja GameAndUI + cursor visible para toda la
    // sesión diegética (podés moverte por el diorama mientras el overlay está abierto); pisarlo
    // con UIOnly bloquearía el WASD. El menú clásico (no diegético) no usa este widget para
    // este flujo, así que no hace falta una rama condicional.

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
        // Estar en el menú principal significa NO estar en una sala (este overlay solo se muestra
        // en standalone). Si quedó una sesión registrada de una partida anterior, limpiarla: si no,
        // tu sala vieja sigue anunciada (fantasma en Find) y no podés unirte a ninguna otra.
        Sessions->CleanupStaleSession();

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

void UPTMainMenuWidget::OnLockerClicked()
{
    // El Locker vive en el PlayerController del lobby (donde está el modo esculpir/pintar).
    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
        PC->OpenLocker();
}

// ==========================================================================
// Callbacks del subsistema
// ==========================================================================

void UPTMainMenuWidget::OnLogin(bool bWasSuccessful)
{
    if (HostButton) HostButton->SetIsEnabled(bWasSuccessful);
    if (FindButton) FindButton->SetIsEnabled(bWasSuccessful);

    if (!bWasSuccessful && ErrorText)
    {
        // Causa típica: el cliente de Steam no está corriendo en esta PC.
        ErrorText->SetText(PTText::Get(TEXT("ERR_STEAM_CONNECT")));

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(ErrorTextTimerHandle, this, &UPTMainMenuWidget::HideErrorText, 2.f, false);
        }
    }
}

void UPTMainMenuWidget::OnCreateSession(bool bWasSuccessful)
{
    if (!bWasSuccessful)
    {
        if (ErrorText) ErrorText->SetText(PTText::Get(TEXT("ERR_CREATE_SESSION")));
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
                FFormatOrderedArguments Args;
                Args.Add(FText::FromString(Code));
                GeneratedCodeText->SetText(PTText::Format(TEXT("MENU_CODE_COPIED"), Args));
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
            // No usar UrlEncode: CanServerTravel rechaza cualquier '%' en la URL (tildes/emojis
            // codificados la rompen entera) — sanitizar en su lugar (ver SanitizeNameForTravelURL).
            // IMPORTANTE: las opciones de FURL se separan con '?', no con '&' (eso es para URLs
            // web normales). Usar '&' acá hace que "Name=..." quede pegado como parte del VALOR
            // de la opción anterior (p.ej. Password="XXXX&Name=Yyyy"), rompiendo la validación de
            // contraseña en PreLogin para sesiones privadas.
            TravelURL += TEXT("?Name=") + UMultiplayerSessionsSubsystem::SanitizeNameForTravelURL(Sessions->GetLocalPlayerDisplayName());
        }

        // Este self-travel tiene que ser un travel DURO (con el "flash" que el diseño acepta),
        // no seamless: en seamless travel el PlayerController del host sobrevive al viaje sin
        // pasar de nuevo por BeginPlay (así que ShowLobbyOverlay nunca se re-ejecuta) y sin pasar
        // por PostLogin (así que el RestartPlayer manual tampoco corre) — el host queda sin pawn
        // y sin overlay, como espectador. bUseSeamlessTravel=true es para el viaje final a Lvl-01
        // (PTLobbyGameMode::TravelToGame lo vuelve a poner en true antes de usarlo).
        if (APTLobbyGameMode* GM = World->GetAuthGameMode<APTLobbyGameMode>())
        {
            GM->bUseSeamlessTravel = false;
            // Evita que el propio host dispare "último jugador se fue, destruir sesión" cuando
            // su conexión local se recicla durante este travel (ver comentario en el header).
            GM->bTravelInProgress = true;
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
            ErrorText->SetText(PTText::Get(Result == EOnJoinSessionCompleteResult::SessionDoesNotExist
                ? TEXT("ERR_INVALID_CODE")
                : TEXT("ERR_JOIN_SESSION")));

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
        // Las opciones de FURL se separan con '?', no con '&' (ver mismo comentario en
        // OnCreateSession) — si no, "Name=..." queda pegado al valor de "Password=...".
        const FString TravelURL = ConnectString
            + TEXT("?Password=")
            + Sessions->GetPendingJoinPassword()
            + TEXT("?Name=") + UMultiplayerSessionsSubsystem::SanitizeNameForTravelURL(Sessions->GetLocalPlayerDisplayName());

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

    // No restaurar GameOnly/cursor acá — ver nota en MenuSetup, lo mantiene el PlayerController
    // durante toda la sesión diegética (el overlay de Ready que sigue también necesita el mouse).

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
