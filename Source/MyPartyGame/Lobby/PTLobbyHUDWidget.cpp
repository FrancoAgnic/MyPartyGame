// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyHUDWidget.h"
#include "PTGameState.h"
#include "PTPlayerState.h"
#include "PTLobbyPlayerController.h"
#include "PTPlayerRowWidget.h"
#include "../PTTextTable.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "../PTNetStats.h"
#include "../PTGameInstance.h" // modo captura dev (Player N)
#include "../PTGameInstance.h"
#include "../PTWordBank.h"
#include "../Multiplayer/MultiplayerSessionsSubsystem.h"
#include "../UI/PTFriendsWidget.h"
#include "../UI/PTWordPackWidget.h"
#include "PTGameSettingsWidget.h"

bool UPTLobbyHUDWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (CopyCodeButton)  CopyCodeButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnCopyCodeClicked);
    if (LeaveGameButton) LeaveGameButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnLeaveGameClicked);
    if (StartGameButton) StartGameButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnStartGameClicked);
    if (ReadyButton)      ReadyButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnReadyClicked);
    if (LockerButton)     LockerButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnLockerClicked);
    if (InviteButton)     InviteButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnInviteClicked);
    if (FriendsPanel)     FriendsPanel->SetVisibility(ESlateVisibility::Collapsed);
    if (GameSettingsButton) GameSettingsButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnGameSettingsClicked);
    if (GameSettingsPanel)
    {
        GameSettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
        // El botón "Library Mods" vive en el settings, pero el WBP_WordPack vive acá → lo abrimos nosotros.
        GameSettingsPanel->OnRequestLibrary.BindUObject(this, &UPTLobbyHUDWidget::OnLibraryRequested);
    }
    if (LibraryPanel) LibraryPanel->SetVisibility(ESlateVisibility::Collapsed);

    return true;
}

void UPTLobbyHUDWidget::OnLibraryRequested()
{
    if (LibraryPanel) LibraryPanel->ShowPanel();
}

void UPTLobbyHUDWidget::ShowHUD()
{
    // Este HUD solo tiene sentido DENTRO de una sesión (host tras ?listen o cliente tras join →
    // NetMode ListenServer/Client). En Standalone todavía estamos en el menú Crear/Unirse (sin
    // sesión) y mostrarlo lo encimaría al MainMenu. El flujo correcto lo dispara C++ desde
    // APTLobbyPlayerController::ShowLobbyOverlay (solo en la rama networked); si ShowHUD igual
    // llega en Standalone es por un llamado de más (p.ej. un nodo BP viejo "Show HUD" en el
    // Event BeginPlay de BP_LobbyPlayerController) → ignorarlo.
    if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Lobby] ShowHUD ignorado: NetMode Standalone (menú, sin sesión). ¿Nodo BP 'Show HUD' de más?"));
        return;
    }

    // Idempotente: si ya está en el viewport (ej: lo llamaron dos veces — un nodo BP + el C++),
    // no re-agregarlo ni reiniciar el timer.
    if (IsInViewport()) return;

    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);

    RefreshPlayerList();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            RefreshTimerHandle, this, &UPTLobbyHUDWidget::RefreshPlayerList, 1.0f, true);
    }
}

void UPTLobbyHUDWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }
    Super::NativeDestruct();
}

void UPTLobbyHUDWidget::RefreshPlayerList()
{
    APTGameState* PTGS = GetWorld() ? GetWorld()->GetGameState<APTGameState>() : nullptr;
    if (!PTGS) return;

    const APTPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APTPlayerState>() : nullptr;
    const bool bLocalIsHost = LocalPS && LocalPS->bIsHost;

    if (PlayersBox)
    {
        PlayersBox->ClearChildren();

        // Ordenar: el ANFITRIÓN (corona) siempre primero; el resto en su orden original. Así todos
        // (host y clientes) ven al host arriba de la lista.
        TArray<APTPlayerState*> Ordered;
        for (APlayerState* PS : PTGS->PlayerArray)
            if (APTPlayerState* PTPS = Cast<APTPlayerState>(PS))
                if (!PTPS->bIsDevSpectator) Ordered.Add(PTPS); // los espectadores dev no van en la lista
        Ordered.StableSort([](const APTPlayerState& A, const APTPlayerState& B)
            { return A.bIsHost && !B.bIsHost; }); // true si A (host) debe ir antes que B

        // Modo captura dev: nombres → "Player N" (local, no se replica).
        UPTGameInstance* CapGI = GetGameInstance<UPTGameInstance>();
        auto NameFor = [&](APTPlayerState* PS) -> FString
        {
            if (CapGI && CapGI->IsCaptureMode())
            {
                const FString Cap = CapGI->GetCaptureName(PS);
                if (!Cap.IsEmpty()) return Cap;
            }
            return PS->GetDisplayNameSafe();
        };

        for (APTPlayerState* PTPS : Ordered)
        {
            if (!PTPS) continue;

            if (PlayerRowClass)
            {
                // Fila = widget propio (nombre + corona si es host + check listo/no-listo teñido).
                if (UPTPlayerRowWidget* Row = CreateWidget<UPTPlayerRowWidget>(this, PlayerRowClass))
                {
                    // Kick: solo lo ve el host, en filas de OTROS (no en la del propio host).
                    const bool bCanKick = bLocalIsHost && !PTPS->bIsHost;
                    Row->SetRow(NameFor(PTPS), PTPS->bIsHost, PTPS->bIsReady,
                                ReadyColor, NotReadyColor, MaxNameChars, PTPS, bCanKick);
                    PlayersBox->AddChildToVerticalBox(Row);
                }
            }
            else
            {
                // Fallback (sin WBP de fila asignado): texto plano como antes.
                UTextBlock* Row = NewObject<UTextBlock>(this);
                FString Label = NameFor(PTPS);
                if (PTPS->bIsHost) Label += TEXT(" (") + PTText::GetStr(TEXT("LOBBY_HOST")) + TEXT(")");
                Label += TEXT(" — ") + PTText::GetStr(PTPS->bIsReady ? TEXT("LOBBY_READY") : TEXT("LOBBY_WAITING"));
                Row->SetText(FText::FromString(Label));
                PlayersBox->AddChildToVerticalBox(Row);
            }
        }
    }

    if (PlayersCountText)
    {
        // Contar sin los espectadores dev.
        int32 NumActive = 0;
        for (APlayerState* PS : PTGS->PlayerArray)
        {
            const APTPlayerState* PTPS = Cast<APTPlayerState>(PS);
            if (!PTPS || !PTPS->bIsDevSpectator) ++NumActive;
        }
        PlayersCountText->SetText(FText::FromString(
            FString::Printf(TEXT("%d/%d"), NumActive, PTGS->MaxPlayers)));
    }

    if (LobbyStatusText)
    {
        // UN solo texto: normalmente "Esperando jugadores..."; durante la cuenta regresiva (todos listos)
        // muestra "Empezando en X...". Si alguien saca el listo, CountdownSecondsRemaining vuelve a -1 y
        // el texto vuelve solo al de esperando.
        const int32 Seconds = PTGS->CountdownSecondsRemaining;
        if (Seconds >= 0)
        {
            FFormatOrderedArguments Args; Args.Add(FText::AsNumber(Seconds));
            LobbyStatusText->SetText(PTText::Format(TEXT("LOBBY_STARTS_IN"), Args));
        }
        else
        {
            FString Status;
            switch (PTGS->LobbyState)
            {
            case EPTLobbyState::Starting: Status = PTText::GetStr(TEXT("LOBBY_STARTING")); break;
            case EPTLobbyState::InGame:   Status = PTText::GetStr(TEXT("LOBBY_IN_GAME"));  break;
            default:                      Status = PTText::GetStr(TEXT("LOBBY_WAITING_PLAYERS"));
            }
            LobbyStatusText->SetText(FText::FromString(Status));
        }
    }

    CachedRoomCode = PTGS->SessionCode;
    const bool bPrivate = !CachedRoomCode.IsEmpty();

    if (PrivateRoomPanel) PrivateRoomPanel->SetVisibility(bPrivate ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (RoomCodeText)
    {
        RoomCodeText->SetVisibility(bPrivate ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bPrivate) RoomCodeText->SetText(FText::FromString(CachedRoomCode));
    }

    if (StartGameButton)
    {
        StartGameButton->SetVisibility(bLocalIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    // Config de partida: el botón "Game Settings" lo ve solo el host y abre el panel (widget aparte).
    if (GameSettingsButton)
        GameSettingsButton->SetVisibility(bLocalIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    // Vista read-only de la config elegida por el host (la ven todos).
    RefreshSettingsView();

    // Botón Ready: refleja el estado propio. No listo → "Not Ready" en rojo pastel; listo →
    // "Ready" en verde pastel. Clickearlo alterna (ver OnReadyClicked). SetBackgroundColor tiñe
    // el brush del botón, así que conviene que en el WBP el botón tenga brushes blancos.
    {
        const bool bReady = (LocalPS && LocalPS->bIsReady);
        if (ReadyButtonText)
            ReadyButtonText->SetText(PTText::Get(bReady ? TEXT("LOBBY_BTN_READY") : TEXT("LOBBY_BTN_NOT_READY")));
        if (ReadyButton)
            ReadyButton->SetBackgroundColor(bReady ? ReadyColor : NotReadyColor); // mismos colores que los checks
    }

    // (El texto de cuenta regresiva se unificó en LobbyStatusText; ya no se usa un CountdownText aparte.)
    if (CountdownText) CountdownText->SetVisibility(ESlateVisibility::Collapsed);

    // ── Diagnóstico de red (ping + packet loss) ──
    // On-screen (visible en build Development). Ya en el lobby ves tu ping antes de arrancar.
    if (GEngine)
    {
        const PTNetStats::FLine NS = PTNetStats::Build(GetOwningPlayer());
        if (!NS.Text.IsEmpty())
            GEngine->AddOnScreenDebugMessage(987710, 1.5f, NS.Color, NS.Text);
    }
}

void UPTLobbyHUDWidget::RefreshSettingsView()
{
    const APTGameState* PTGS = GetWorld() ? GetWorld()->GetGameState<APTGameState>() : nullptr;
    if (!PTGS) return;

    // El panel de los clientes solo se ve mientras el host tiene su Game Settings abierto (y este
    // jugador no es el host, que ya tiene su propio panel editable).
    const APTPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APTPlayerState>() : nullptr;
    const bool bLocalIsHost = LocalPS && LocalPS->bIsHost;
    const bool bShowClientPanel = PTGS->bHostSettingsPanelOpen && !bLocalIsHost;
    if (GameSettingsClientsPanel)
        GameSettingsClientsPanel->SetVisibility(bShowClientPanel ? ESlateVisibility::Visible
                                                                  : ESlateVisibility::Collapsed);
    if (!bShowClientPanel) return; // nada que rellenar si está oculto

    if (SV_PrivateText)
    {
        const FText YesNo = PTText::Get(PTGS->bMatchFriendsOnly ? TEXT("SV_YES") : TEXT("SV_NO"));
        FFormatOrderedArguments Args; Args.Add(YesNo);
        SV_PrivateText->SetText(PTText::Format(TEXT("SV_PRIVATE"), Args));
    }
    if (SV_TurnTimeText)
    {
        FFormatOrderedArguments Args; Args.Add(FText::AsNumber(FMath::RoundToInt(PTGS->MatchTurnDuration)));
        SV_TurnTimeText->SetText(PTText::Format(TEXT("SV_TURNTIME"), Args));
    }
    if (SV_RoundsText)
    {
        FFormatOrderedArguments Args; Args.Add(FText::AsNumber(PTGS->MatchNumRounds));
        SV_RoundsText->SetText(PTText::Format(TEXT("SV_ROUNDS"), Args));
    }
    if (SV_RevealText)
    {
        FFormatOrderedArguments Args; Args.Add(FText::AsNumber(FMath::RoundToInt(PTGS->MatchRevealFraction * 100.f)));
        SV_RevealText->SetText(PTText::Format(TEXT("SV_REVEAL"), Args));
    }
    if (SV_WordPackText)
    {
        const FText Pack = PTGS->MatchWordPackTitle.IsEmpty()
            ? PTText::Get(TEXT("GS_DEFAULT")) : FText::FromString(PTGS->MatchWordPackTitle);
        FFormatOrderedArguments Args; Args.Add(Pack);
        SV_WordPackText->SetText(PTText::Format(TEXT("SV_LIBRARY"), Args));
    }
}

void UPTLobbyHUDWidget::OnCopyCodeClicked()
{
    if (!CachedRoomCode.IsEmpty())
    {
        FPlatformApplicationMisc::ClipboardCopy(*CachedRoomCode);
    }
}

void UPTLobbyHUDWidget::OnLeaveGameClicked()
{
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

void UPTLobbyHUDWidget::OnStartGameClicked()
{
    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
    {
        PC->Server_RequestStartGame();
    }
}

void UPTLobbyHUDWidget::OnReadyClicked()
{
    const APTPlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->GetPlayerState<APTPlayerState>() : nullptr;
    if (!LocalPS) return;

    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
    {
        PC->Server_SetReady(!LocalPS->bIsReady);
    }
}

void UPTLobbyHUDWidget::OnLockerClicked()
{
    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer())) PC->OpenLocker();
}

void UPTLobbyHUDWidget::OnInviteClicked()
{
    // Preferir el panel de amigos embebido; si no está en el WBP, caer al overlay de Steam.
    if (FriendsPanel)
    {
        FriendsPanel->ShowPanel();
        return;
    }
    if (UMultiplayerSessionsSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
        S->ShowSteamInviteOverlay();
}


void UPTLobbyHUDWidget::OnGameSettingsClicked()
{
    // El panel de ajustes es su propio widget (GameSettingsPanel). Solo lo mostramos; toda la lógica
    // (steppers, dificultad, categorías, privada, biblioteca) vive adentro de PTGameSettingsWidget.
    if (GameSettingsPanel) GameSettingsPanel->ShowPanel();
}
