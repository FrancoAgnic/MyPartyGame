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
#include "Components/Image.h"
#include "../PTGameInstance.h" // modo captura dev (Player N)
#include "../UI/PTLoadingScreenWidget.h" // transición de entrada a la partida
#include "../PTWordBank.h"
#include "../Multiplayer/MultiplayerSessionsSubsystem.h"
#include "../UI/PTFriendsWidget.h"
#include "../UI/PTWordPackWidget.h"
#include "PTGameSettingsWidget.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/RichTextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "../Mods/PTWordPackSubsystem.h" // FPTWordPack (miniatura del banco)
#include "../Mods/PTMapModSubsystem.h"   // FPTMapMod (miniatura del mapa)
#include "ImageUtils.h"                    // ImportFileAsTexture2D / ImportBufferAsTexture2D
#include "Engine/Texture2D.h"
#include "Misc/Paths.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

bool UPTLobbyHUDWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (ChatInput)
    {
        ChatInput->OnTextCommitted.AddDynamic(this, &UPTLobbyHUDWidget::OnChatCommitted);
        ChatInput->OnTextChanged.AddDynamic(this, &UPTLobbyHUDWidget::OnChatTextChanged);
    }
    // La barra abre/cierra el chat con click (además de ENTER) e indica el estado (flecha ↑/↓ + glow).
    if (ChatBarButton)
    {
        ChatBarButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnChatBarClicked);
        ChatBarUpStyle = ChatBarButton->WidgetStyle; // guardar la flecha ARRIBA (la que pusiste en el botón)
        bChatBarStyleCached = true;
    }
    SetChatExpanded(false); // arranca colapsado → NO roba el foco del teclado al entrar (foco = juego)

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

    // Enganchar el chat del lobby una vez que el GameState ya existe (el HUD puede crearse antes).
    if (!bChatBound)
    {
        PTGS->OnLobbyChat.AddDynamic(this, &UPTLobbyHUDWidget::OnLobbyChatLine);
        bChatBound = true;
    }

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
                    // Descargando el mapa custom = todavía no lo tiene (bHasSelectedMap replicado).
                    const bool bDownloadingMap = !PTPS->bHasSelectedMap;
                    Row->SetRow(NameFor(PTPS), PTPS->bIsHost, PTPS->bIsReady,
                                ReadyColor, NotReadyColor, MaxNameChars, PTPS, bCanKick, bDownloadingMap);
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
        // Transición a la partida: cuando falta poco, tocar el AnimIn SOBRE el lobby (para no ver una pantalla
        // negra ni el nivel destino durante el travel). El server viaja en 0; el widget muere en el travel y el
        // destino arranca ya tapado (bTransitionCovering) → OUT revela. Si se cancela el countdown, se saca.
        if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
        {
            if (Seconds >= 0 && Seconds <= 2 && !PendingMatchLoading && GI->LoadingScreenClass)
            {
                PendingMatchLoading = GI->CreateLoadingScreen(/*bStartAtLoop=*/false);
                GI->bTransitionCovering = true;
            }
            else if (Seconds < 0 && PendingMatchLoading)
            {
                PendingMatchLoading->RemoveFromParent();
                PendingMatchLoading = nullptr;
                GI->bTransitionCovering = false;
            }
        }
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

    // Botón Ready: MUESTRA EL ESTADO ACTUAL (no la acción). No listo → "No listo" en rojo
    // pastel; listo → "Listo" en verde pastel. Clickearlo alterna el estado (ver OnReadyClicked).
    // El color se aplica al texto (ReadyButtonText), no al botón.
    {
        const bool bReady = (LocalPS && LocalPS->bIsReady);
        if (ReadyButtonText)
        {
            // El texto y el color reflejan tu estado actual: apenas entrás estás "No listo"
            // (rojo); al apretar pasás a "Listo" (verde).
            ReadyButtonText->SetText(PTText::Get(bReady ? TEXT("LOBBY_BTN_READY") : TEXT("LOBBY_BTN_NOT_READY")));
            ReadyButtonText->SetColorAndOpacity(bReady ? ReadyColor : NotReadyColor);
            ReadyButtonText->SetShadowColorAndOpacity(bReady ? ReadyTextShadowColor : NotReadyTextShadowColor);
        }
    }

    // (El texto de cuenta regresiva se unificó en LobbyStatusText; ya no se usa un CountdownText aparte.)
    if (CountdownText) CountdownText->SetVisibility(ESlateVisibility::Collapsed);

    // ── Estado de red: iconos arriba a la izquierda, SOLO cuando hay problema (sin texto de debug). ──
    {
        const PTNetStats::FStatus NS = PTNetStats::Query(GetOwningPlayer());
        auto Show = [](UImage* Img, bool bOn)
        {
            if (Img) Img->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        };
        Show(IconPacketLoss,   NS.bRemote && NS.bPacketLoss);
        Show(IconHighPing,     NS.bRemote && NS.bHighPing);
        Show(IconDisconnected, NS.bRemote && NS.bLost);
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
    if (SV_MapText)
    {
        const FText Map = PTGS->MatchMapTitle.IsEmpty()
            ? PTText::Get(TEXT("SV_MAP_OFFICIAL")) : FText::FromString(PTGS->MatchMapTitle);
        FFormatOrderedArguments Args; Args.Add(Map);
        SV_MapText->SetText(PTText::Format(TEXT("SV_MAP"), Args));
    }

    RefreshSettingsThumbnails(); // miniaturas de mapa/banco (local o por HTTP)
}

void UPTLobbyHUDWidget::RefreshSettingsThumbnails()
{
    const APTGameState* PTGS = GetWorld() ? GetWorld()->GetGameState<APTGameState>() : nullptr;
    if (!PTGS) return;
    UGameInstance* GI = GetGameInstance();

    // ── MAPA: preview.png local (el host lo tiene; los clientes tras descargarlo en el lobby) ──
    if (SV_MapThumbnail)
    {
        const FString Id = PTGS->MatchMapModId;
        if (Id.IsEmpty())
        {
            if (CachedMapThumbKey != TEXT("__none__"))
            { CachedMapThumbKey = TEXT("__none__"); SV_MapThumbnail->SetVisibility(ESlateVisibility::Collapsed); }
        }
        else if (CachedMapThumbKey != Id)
        {
            FString Path;
            if (UPTMapModSubsystem* MM = GI ? GI->GetSubsystem<UPTMapModSubsystem>() : nullptr)
            {
                if (const FPTMapMod* M = MM->FindMod(Id)) Path = M->PreviewPath;
                if (Path.IsEmpty()) { MM->RescanMods(); if (const FPTMapMod* M2 = MM->FindMod(Id)) Path = M2->PreviewPath; }
            }
            if (!Path.IsEmpty() && FPaths::FileExists(Path))
                if (UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Path))
                {
                    SV_MapThumbnail->SetBrushFromTexture(Tex, false);
                    SV_MapThumbnail->SetVisibility(ESlateVisibility::HitTestInvisible);
                    CachedMapThumbKey = Id; // cachear solo cuando se logró (si no, reintenta al bajar el mapa)
                }
        }
    }

    // ── BANCO: local si esta máquina tiene el banco (oficial/suscripto); si no, por HTTP (PreviewURL) ──
    if (SV_WordPackThumbnail)
    {
        const FString Id  = PTGS->MatchWordPackId;
        const FString URL = PTGS->MatchWordPackPreviewURL;
        const FString Key = Id + TEXT("|") + URL;
        if (Id.IsEmpty() && URL.IsEmpty())
        {
            if (CachedPackThumbKey != TEXT("__none__"))
            { CachedPackThumbKey = TEXT("__none__"); SV_WordPackThumbnail->SetVisibility(ESlateVisibility::Collapsed); }
        }
        else if (CachedPackThumbKey != Key)
        {
            FString Path;
            if (UPTWordPackSubsystem* WP = GI ? GI->GetSubsystem<UPTWordPackSubsystem>() : nullptr)
                if (const FPTWordPack* P = WP->FindPack(Id)) Path = P->PreviewPath;
            if (!Path.IsEmpty() && FPaths::FileExists(Path))
            {
                if (UTexture2D* Tex = FImageUtils::ImportFileAsTexture2D(Path))
                {
                    SV_WordPackThumbnail->SetBrushFromTexture(Tex, false);
                    SV_WordPackThumbnail->SetVisibility(ESlateVisibility::HitTestInvisible);
                    CachedPackThumbKey = Key;
                }
            }
            else if (!URL.IsEmpty())
            {
                DownloadThumbnailTo(SV_WordPackThumbnail, URL);
                CachedPackThumbKey = Key; // la descarga es async; marcar para no re-pedir en loop
            }
        }
    }
}

void UPTLobbyHUDWidget::DownloadThumbnailTo(UImage* Target, const FString& URL)
{
    if (!Target || URL.IsEmpty()) return;
    TWeakObjectPtr<UImage> W = Target;
    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(URL);
    Req->SetVerb(TEXT("GET"));
    Req->OnProcessRequestComplete().BindLambda(
        [W](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
        {
            if (!bOk || !Resp) return;
            const TArray<uint8>& Bytes = Resp->GetContent();
            if (UImage* Img = W.Get())
                if (UTexture2D* Tex = FImageUtils::ImportBufferAsTexture2D(Bytes))
                {
                    Img->SetBrushFromTexture(Tex, false);
                    Img->SetVisibility(ESlateVisibility::HitTestInvisible);
                }
        });
    Req->ProcessRequest();
}

void UPTLobbyHUDWidget::ScrollChatToEndDeferred()
{
    // "Pegar" el scroll al final por ~0.5s: NativeTick lo fuerza CADA frame, así por más largo que sea el
    // mensaje (auto-wrap) el layout se asienta y siempre queda visible su última línea sobre el input.
    if (ChatScroll) ChatScroll->ScrollToEnd();
    bChatStickToEnd = true;
    ChatStickElapsed = 0.f;
}

void UPTLobbyHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (bChatStickToEnd && ChatScroll)
    {
        if (TxtChat) TxtChat->ForceLayoutPrepass(); // recalcular el alto con el wrap aplicado
        ChatScroll->ScrollToEnd();                  // mantener el final visible mientras se asienta el layout
        ChatStickElapsed += InDeltaTime;
        if (ChatStickElapsed >= 0.5f) bChatStickToEnd = false;
    }
}

void UPTLobbyHUDWidget::OnChatBarClicked()
{
    // Click en la barra: alterna abrir/cerrar (mismo toggle que ENTER).
    SetChatExpanded(!bChatExpanded);
}

void UPTLobbyHUDWidget::OpenChatFromEnter()
{
    // ENTER con el juego enfocado. Dos casos:
    //  · chat cerrado → abrir + foco al input.
    //  · chat abierto pero en los 3s de gracia tras enviar (foco en el juego) → cancelar el cierre y volver
    //    a escribir.
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ChatAutoCloseTimer);
    if (!bChatExpanded) SetChatExpanded(true); // abre (ya enfoca el input)
    else                FocusChatInput();       // estaba en gracia → volver a escribir
}

void UPTLobbyHUDWidget::UpdateChatBarVisual()
{
    // La barra queda quieta; solo cambia la imagen del botón según el estado.
    if (!ChatBarButton || !bChatBarStyleCached) return;
    FButtonStyle St = ChatBarUpStyle; // base: flecha ARRIBA (conserva tamaños/pressed originales)
    if (bChatExpanded)
    {
        if (ChatBarDownNormal)  St.Normal.SetResourceObject(ChatBarDownNormal);
        if (ChatBarDownHovered){ St.Hovered.SetResourceObject(ChatBarDownHovered); St.Pressed.SetResourceObject(ChatBarDownHovered); }
    }
    else if (bChatUnread && ChatBarUnreadMaterial) // colapsado + mensaje sin leer → material con pulse
    {
        St.Normal.SetResourceObject(ChatBarUnreadMaterial);
        St.Hovered.SetResourceObject(ChatBarUnreadMaterial);
        St.Pressed.SetResourceObject(ChatBarUnreadMaterial);
    }
    ChatBarButton->WidgetStyle = St;
    ChatBarButton->SynchronizeProperties();
}

void UPTLobbyHUDWidget::FocusChatInput()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        FInputModeGameAndUI Mode;
        if (ChatInput) Mode.SetWidgetToFocus(ChatInput->TakeWidget());
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
        PC->SetShowMouseCursor(true);
    }
    if (ChatInput) ChatInput->SetKeyboardFocus();
}

void UPTLobbyHUDWidget::ReturnFocusToGame()
{
    // Reaplicar el input del lobby + enfocar el VIEWPORT del juego (lo hace el PlayerController). Así el
    // WASD vuelve INMEDIATAMENTE, sin tener que clickear la pantalla.
    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
        PC->RestoreLobbyMovementFocus();
}

void UPTLobbyHUDWidget::SetChatExpanded(bool bExpanded)
{
    bChatExpanded = bExpanded;
    if (ChatPanel) ChatPanel->SetVisibility(bExpanded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    if (bExpanded)
    {
        // Abrir: limpiar "no leídos", scrollear al final y dar foco al input para escribir.
        bChatUnread = false;
        OnChatUnreadChanged(false);
        if (ChatUnreadIndicator) ChatUnreadIndicator->SetVisibility(ESlateVisibility::Collapsed);
        ScrollChatToEndDeferred(); // al abrir, mostrar los últimos mensajes
        FocusChatInput();
    }
    else
    {
        // Cerrar: cancelar el auto-cierre, vaciar la caja y devolver el foco al movimiento.
        if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ChatAutoCloseTimer);
        if (ChatInput) ChatInput->SetText(FText::GetEmpty());
        ReturnFocusToGame();
    }
    UpdateChatBarVisual(); // flecha ↓/↑ o material pulse según estado
}

void UPTLobbyHUDWidget::OnChatTextChanged(const FText& /*Text*/)
{
    // Estás escribiendo → cancelar el auto-cierre (no cerrar mientras componés un mensaje).
    if (GetWorld()) GetWorld()->GetTimerManager().ClearTimer(ChatAutoCloseTimer);
}

void UPTLobbyHUDWidget::OnChatCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    // El input tiene el foco (chat abierto). ENTER: si hay texto, enviar; si está vacío, cerrar (toggle).
    if (CommitMethod != ETextCommit::OnEnter) return;
    const FString Msg = Text.ToString().TrimStartAndEnd();

    if (Msg.IsEmpty()) { SetChatExpanded(false); return; } // ENTER con la caja vacía = cerrar

    if (ChatInput) ChatInput->SetText(FText::GetEmpty());
    if (APTLobbyPlayerController* PC = Cast<APTLobbyPlayerController>(GetOwningPlayer()))
        PC->Server_SendLobbyChat(Msg);

    // Tras enviar: el foco vuelve al PERSONAJE enseguida (te movés) y el panel se queda 3s. Si apretás
    // ENTER antes de que terminen, se interrumpe el cierre y volvés a escribir (OpenChatFromEnter).
    ReturnFocusToGame();
    if (GetWorld())
        GetWorld()->GetTimerManager().SetTimer(ChatAutoCloseTimer, this,
            &UPTLobbyHUDWidget::CloseChatAuto, ChatAutoCloseDelay, false);
}

// ¿El codepoint es un emoji? (rangos principales; alcanza para el chat de un party game).
static bool PT_IsEmojiCP(uint32 CP)
{
    return (CP >= 0x1F300 && CP <= 0x1FAFF) || // símbolos & pictogramas, caras, objetos, banderas, etc.
           (CP >= 0x1F000 && CP <= 0x1F2FF) || // mahjong/dominó/cartas/enclosed
           (CP >= 0x2600  && CP <= 0x27BF)  || // misc symbols + dingbats (☀ ✂ ✅ ...)
           (CP >= 0x2B00  && CP <= 0x2BFF)  || // estrellas/flechas (⭐ ⬆ ...)
           (CP >= 0x2190  && CP <= 0x21FF)  || // flechas
           (CP >= 0x2300  && CP <= 0x23FF);    // ⌚ ⏰ ⏳ ...
}

// Convierte los emoji unicode del texto en tags <img id="e_<hex>"/> (los renderiza el image decorator del
// RichTextBlock, a color) y escapa < > & para no romper el parseo. El resto del texto queda igual.
static FString PT_MessageToRich(const FString& In)
{
    FString Out;
    const int32 N = In.Len();
    for (int32 i = 0; i < N; ++i)
    {
        const int32 Start = i;
        uint32 CP = (uint32)In[i];
        if (CP >= 0xD800 && CP <= 0xDBFF && i + 1 < N) // combinar surrogate pair → codepoint real
        {
            const uint32 Lo = (uint32)In[i + 1];
            if (Lo >= 0xDC00 && Lo <= 0xDFFF) { CP = 0x10000 + ((CP - 0xD800) << 10) + (Lo - 0xDC00); ++i; }
        }
        // Modificadores invisibles (variation selector, ZWJ, tonos de piel): se descartan.
        if (CP == 0x200D || CP == 0xFE0F || CP == 0xFE0E || (CP >= 0x1F3FB && CP <= 0x1F3FF)) continue;
        if (PT_IsEmojiCP(CP)) { Out += FString::Printf(TEXT("<img id=\"e_%x\"/>"), CP); continue; }
        if (CP == (uint32)'<') { Out += TEXT("&lt;");  continue; }
        if (CP == (uint32)'>') { Out += TEXT("&gt;");  continue; }
        if (CP == (uint32)'&') { Out += TEXT("&amp;"); continue; }
        Out += In.Mid(Start, i - Start + 1); // code units originales (1 o 2)
    }
    return Out;
}

void UPTLobbyHUDWidget::OnLobbyChatLine(const FString& Name, const FString& Message)
{
    // Modo captura dev: nombre → "Player N" (local).
    FString DispName = Name;
    if (const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
        if (GI->IsCaptureMode() && !Name.IsEmpty())
            if (const APTGameState* G = GetWorld() ? GetWorld()->GetGameState<APTGameState>() : nullptr)
                for (APlayerState* PS : G->PlayerArray)
                    if (PS && PS->GetPlayerName() == Name)
                    { const FString Cap = GI->GetCaptureName(PS); if (!Cap.IsEmpty()) { DispName = Cap; } break; }

    // RichText: el nombre va con el estilo "name" (si existe en el Text Style Set del WBP; si no, color
    // default). El mensaje en texto plano.
    // Nombre con estilo de color; mensaje con emojis convertidos a <img> (color) y < > & escapados.
    ChatLines.Add(FString::Printf(TEXT("<name>%s</>: %s"), *DispName.Left(14), *PT_MessageToRich(Message)));
    // Mostrar SOLO los últimos N (los viejos se van "subiendo" y salen) → el más reciente queda siempre
    // arriba del input sin que la caja crezca hacia abajo ni lo tape.
    const int32 Keep = FMath::Max(1, MaxVisibleChatLines);
    if (ChatLines.Num() > Keep) ChatLines.RemoveAt(0, ChatLines.Num() - Keep);
    if (TxtChat) TxtChat->SetText(FText::FromString(FString::Join(ChatLines, TEXT("\n"))));
    ScrollChatToEndDeferred();

    // Chat colapsado + llegó un mensaje → marcar "no leído": el botón de la barra pasa a su material con
    // pulse (UpdateChatBarVisual) + avisos opcionales para el WBP.
    if (!bChatExpanded && !bChatUnread)
    {
        bChatUnread = true;
        UpdateChatBarVisual();
        OnChatUnreadChanged(true);
        if (ChatUnreadIndicator) ChatUnreadIndicator->SetVisibility(ESlateVisibility::HitTestInvisible);
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
