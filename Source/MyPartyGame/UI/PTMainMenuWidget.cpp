// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMainMenuWidget.h"
#include "../Lobby/PTLobbyPlayerController.h"
#include "MultiplayerSessionsSubsystem.h"
#include "../PTTextTable.h"
#include "PTCreateSessionWidget.h"
#include "PTFindSessionsWidget.h"
#include "PTEnterCodeWidget.h"
#include "PTSettingsWidget.h"
#include "PTLanguageSelectWidget.h"
#include "PTWorkshopBrowserWidget.h"
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
    if (WorkshopButton)  WorkshopButton->OnClicked.AddDynamic(this, &UPTMainMenuWidget::OnWorkshopClicked);

    // Si hay un PlayButton, arrancar en la pantalla principal (submenú Host/Find/EnterCode oculto).
    // Si el WBP todavía no tiene PlayButton, no se toca nada (comportamiento previo, todo visible).
    if (PlayButton) SetPlaySubmenuVisible(false);

    // Número de versión (desde ProjectVersion de DefaultGame.ini, que se sube en cada build).
    // Si se lanzó como la app del Playtest (5115870), agregar "Playtest" para distinguirlo del juego base.
    if (VersionText)
    {
        FString Ver;
        GConfig->GetString(TEXT("/Script/EngineSettings.GeneralProjectSettings"),
                           TEXT("ProjectVersion"), Ver, GGameIni);
        if (Ver.IsEmpty()) Ver = TEXT("0.0.0");

        const bool bPlaytest = GetGameInstance()
            && GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>()
            && GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>()->GetSteamAppId() == 5115870;

        VersionText->SetText(FText::FromString(
            bPlaytest ? FString::Printf(TEXT("v%s Playtest"), *Ver)
                      : FString::Printf(TEXT("v%s"), *Ver)));
    }

    return true;
}

void UPTMainMenuWidget::MenuSetup(int32 InNumPublicConnections, FString InLobbyPath)
{
    NumPublicConnections = InNumPublicConnections;
    LobbyPath            = InLobbyPath;

    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);

    // Animación de entrada (botones deslizándose desde los lados) al aparecer el menú.
    if (SpawnMainMenu) PlayAnimation(SpawnMainMenu);
    // No SetIsFocusable(true) acá: el teclado tiene que quedar para el juego (WASD/Space), no
    // para este widget. Los botones se clickean con mouse únicamente (ver nota de arriba).

    // Settings persistentes (volumen/idioma): aplicar lo guardado al volver al menú.
    if (UPTGameUserSettings* Settings = UPTGameUserSettings::Get())
    {
        Settings->ApplyAudioAndLanguage(GetWorld());
    }

    // Primer arranque: si el jugador TODAVÍA no eligió idioma, mostrar el overlay de selección
    // arriba de todo. Se oculta solo al confirmar (marca el flag → no vuelve a aparecer).
    if (LanguageSelectPanel)
    {
        const UPTGameUserSettings* S = UPTGameUserSettings::Get();
        const bool bNeedPick = !(S && S->HasChosenLanguage());
        if (bNeedPick)
        {
            LanguageSelectPanel->Refresh();
            LanguageSelectPanel->SetVisibility(ESlateVisibility::Visible);
        }
        else
        {
            LanguageSelectPanel->SetVisibility(ESlateVisibility::Collapsed);
        }
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
            ShowError(FText::FromString(Err)); // con auto-ocultar (antes quedaba pegado)
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
        // Invitaciones de Steam: si se acepta una estando ya en el menú, unirse enseguida.
        Sessions->OnInviteAccepted.AddUObject(this, &UPTMainMenuWidget::OnInviteAccepted);

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

    // Pantalla principal: Play/Settings/Exit/Locker + título — se ocultan mientras está abierto "PLAY".
    if (PlayButton)          PlayButton->SetVisibility(bVisible ? Hidden : Shown);
    if (SettingsButton)      SettingsButton->SetVisibility(bVisible ? Hidden : Shown);
    if (QuitButton)          QuitButton->SetVisibility(bVisible ? Hidden : Shown);
    if (LockerButton)        LockerButton->SetVisibility(bVisible ? Hidden : Shown); // no solapar en el submenú Play
    if (WorkshopButton)      WorkshopButton->SetVisibility(bVisible ? Hidden : Shown); // idem: es de la pantalla principal
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

void UPTMainMenuWidget::OnWorkshopClicked()
{
    if (!WorkshopBrowserClass)
    {
        UE_LOG(LogTemp, Warning, TEXT("[Menu] WorkshopButton sin WorkshopBrowserClass asignado (Details del WBP del menú)."));
        return;
    }
    // Crear la ventana una sola vez y reusarla; mostrarla por encima del menú.
    if (!WorkshopBrowser)
    {
        WorkshopBrowser = CreateWidget<UPTWorkshopBrowserWidget>(this, WorkshopBrowserClass);
        if (!WorkshopBrowser) return;
        WorkshopBrowser->AddToViewport(50); // por encima del menú
    }
    WorkshopBrowser->ShowPanel();
}

// ==========================================================================
// Callbacks del subsistema
// ==========================================================================

void UPTMainMenuWidget::OnLogin(bool bWasSuccessful)
{
    if (HostButton) HostButton->SetIsEnabled(bWasSuccessful);
    if (FindButton) FindButton->SetIsEnabled(bWasSuccessful);

    if (!bWasSuccessful)
    {
        // Causa típica: el cliente de Steam no está corriendo en esta PC.
        ShowError(PTText::Get(TEXT("ERR_STEAM_CONNECT")));
        return;
    }

    // Caso "el juego se LANZÓ desde una invitación de Steam": la invitación llegó durante el arranque
    // y quedó en cola. Recién ahora (login OK, ya hay LocalPlayer/net id) la procesamos → join+travel.
    if (bWasSuccessful && Sessions && Sessions->HasPendingInvite())
        Sessions->ProcessPendingInvite();
}

void UPTMainMenuWidget::OnInviteAccepted()
{
    // Aceptaron una invitación con el juego ya abierto en el menú: ya estamos logueados, unirse ya.
    if (Sessions) Sessions->ProcessPendingInvite();
}

void UPTMainMenuWidget::OnCreateSession(bool bWasSuccessful)
{
    if (!bWasSuccessful)
    {
        ShowError(PTText::Get(TEXT("ERR_CREATE_SESSION")));
        return;
    }

    if (UWorld* World = GetWorld())
    {
        MenuTearDown();

        // Host viaja al lobby como listen server. Ya no hay contraseña/código: la privacidad
        // "solo amigos" es por visibilidad (la UI filtra), no por un gate en PreLogin.
        FString TravelURL = LobbyPath + TEXT("?listen");
        if (Sessions)
        {
            // "Name" lo reconoce el motor nativamente (AGameModeBase::InitNewPlayer) y deja el
            // nombre real de Steam en PlayerState->GetPlayerName() antes de PostLogin.
            // No usar UrlEncode: CanServerTravel rechaza cualquier '%' en la URL (tildes/emojis
            // codificados la rompen entera) — sanitizar en su lugar (ver SanitizeNameForTravelURL).
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
        ShowError(PTText::Get(Result == EOnJoinSessionCompleteResult::SessionDoesNotExist
            ? TEXT("ERR_SESSION_NOT_FOUND")
            : TEXT("ERR_JOIN_SESSION")));
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

        if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
        {
            MenuTearDown();
            // Viaje por el PUNTO ÚNICO del GameInstance: guard anti-flood (no abre 2 conexiones al
            // mismo host → evita el crash del host por conexiones duplicadas) + watchdog + arma el
            // auto-reintento. La contraseña va en la URL (validación real en el PreLogin del server).
            GI->NotifyJoinedServer(TravelURL);
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
        Sessions->OnInviteAccepted.RemoveAll(this);
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

void UPTMainMenuWidget::ShowError(const FText& Msg)
{
    if (!ErrorText) return;
    ErrorText->SetText(Msg);
    // SIEMPRE reprogramar el auto-ocultar → así ningún mensaje queda pegado para siempre.
    if (UWorld* World = GetWorld())
        World->GetTimerManager().SetTimer(ErrorTextTimerHandle, this, &UPTMainMenuWidget::HideErrorText, 3.f, false);
}
