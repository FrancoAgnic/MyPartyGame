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
#include "../PTGameInstance.h"
#include "../PTWordBank.h"
#include "../Multiplayer/MultiplayerSessionsSubsystem.h"
#include "../UI/PTFriendsWidget.h"

bool UPTLobbyHUDWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (CopyCodeButton)  CopyCodeButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnCopyCodeClicked);
    if (LeaveGameButton) LeaveGameButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnLeaveGameClicked);
    if (StartGameButton) StartGameButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnStartGameClicked);
    if (ReadyButton)      ReadyButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnReadyClicked);
    if (LockerButton)     LockerButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnLockerClicked);
    if (InviteButton)     InviteButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnInviteClicked);
    if (FriendsOnlyCheckbox) FriendsOnlyCheckbox->OnCheckStateChanged.AddDynamic(this, &UPTLobbyHUDWidget::OnFriendsOnlyChanged);
    if (FriendsPanel)     FriendsPanel->SetVisibility(ESlateVisibility::Collapsed);

    // Config de partida (host).
    if (TurnTimeMinus) TurnTimeMinus->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnTurnTimeMinus);
    if (TurnTimePlus)  TurnTimePlus->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnTurnTimePlus);
    if (RoundsMinus)   RoundsMinus->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnRoundsMinus);
    if (RoundsPlus)    RoundsPlus->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnRoundsPlus);
    if (RevealMinus)   RevealMinus->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnRevealMinus);
    if (RevealPlus)    RevealPlus->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnRevealPlus);
    if (DiffFacilButton)   DiffFacilButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnDiffFacil);
    if (DiffMediaButton)   DiffMediaButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnDiffMedia);
    if (DiffDificilButton) DiffDificilButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnDiffDificil);
    if (LoadCSVButton)  LoadCSVButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnLoadCSV);
    if (ClearCSVButton) ClearCSVButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnClearCSV);
    if (GameSettingsButton)  GameSettingsButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnGameSettingsClicked);
    if (CloseSettingsButton) CloseSettingsButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnCloseSettingsClicked);

    // El panel arranca cerrado.
    if (HostSettingsPanel) HostSettingsPanel->SetVisibility(ESlateVisibility::Collapsed);

    return true;
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
            if (APTPlayerState* PTPS = Cast<APTPlayerState>(PS)) Ordered.Add(PTPS);
        Ordered.StableSort([](const APTPlayerState& A, const APTPlayerState& B)
            { return A.bIsHost && !B.bIsHost; }); // true si A (host) debe ir antes que B

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
                    Row->SetRow(PTPS->GetDisplayNameSafe(), PTPS->bIsHost, PTPS->bIsReady,
                                ReadyColor, NotReadyColor, MaxNameChars, PTPS, bCanKick);
                    PlayersBox->AddChildToVerticalBox(Row);
                }
            }
            else
            {
                // Fallback (sin WBP de fila asignado): texto plano como antes.
                UTextBlock* Row = NewObject<UTextBlock>(this);
                FString Label = PTPS->GetDisplayNameSafe();
                if (PTPS->bIsHost) Label += TEXT(" (") + PTText::GetStr(TEXT("LOBBY_HOST")) + TEXT(")");
                Label += TEXT(" — ") + PTText::GetStr(PTPS->bIsReady ? TEXT("LOBBY_READY") : TEXT("LOBBY_WAITING"));
                Row->SetText(FText::FromString(Label));
                PlayersBox->AddChildToVerticalBox(Row);
            }
        }
    }

    if (PlayersCountText)
    {
        PlayersCountText->SetText(FText::FromString(
            FString::Printf(TEXT("%d/%d"), PTGS->PlayerArray.Num(), PTGS->MaxPlayers)));
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

    // Invitar: cualquier miembro puede invitar amigos a la sala. El toggle de visibilidad
    // (pública/solo-amigos) SOLO lo ve/usa el host.
    if (FriendsOnlyCheckbox)
    {
        FriendsOnlyCheckbox->SetVisibility(bLocalIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bLocalIsHost)
            if (UMultiplayerSessionsSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
                FriendsOnlyCheckbox->SetIsChecked(S->IsSessionFriendsOnly());
    }

    // Config de partida: el botón "Game Settings" lo ve solo el host. El panel se abre/cierra
    // con ese botón (y la X interna); los que no son host nunca lo ven.
    if (GameSettingsButton)
        GameSettingsButton->SetVisibility(bLocalIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (HostSettingsPanel)
        HostSettingsPanel->SetVisibility((bLocalIsHost && bSettingsOpen) ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bLocalIsHost)
    {
        if (!bCategoryChecksBuilt) BuildCategoryChecks();
        RefreshHostSettingsUI();
    }

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

void UPTLobbyHUDWidget::OnFriendsOnlyChanged(bool bIsChecked)
{
    // El host cambia la visibilidad de la sala en vivo (el host ES el server → llamada directa).
    if (UMultiplayerSessionsSubsystem* S = GetGameInstance() ? GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>() : nullptr)
        S->SetSessionFriendsOnly(bIsChecked);
}

// ── Config de partida (host) ─────────────────────────────────────────────────
// Todo escribe directo en el GameInstance del host (que ES el server). Al viajar a Lvl-01 el
// GameInstance sobrevive y APTSculptGameMode lee PendingMatchSettings al arrancar.

UPTGameInstance* UPTLobbyHUDWidget::GetGI() const
{
    return GetWorld() ? GetWorld()->GetGameInstance<UPTGameInstance>() : nullptr;
}

void UPTLobbyHUDWidget::BuildCategoryChecks()
{
    if (bCategoryChecksBuilt || !CategoriesBox || !WidgetTree) return;
    bCategoryChecksBuilt = true;

    CategoryNames = PTWordBank::GetDefaultCategories(); // 18 categorías del DataTable
    CategoryChecks.Reset();

    // Grid de 3 columnas adentro de CategoriesBox (así 9 categorías no forman una columna larga
    // que rompe el layout). C++ crea el grid, no importa qué contenedor sea CategoriesBox.
    const int32 Cols = 3;
    UUniformGridPanel* Grid = WidgetTree->ConstructWidget<UUniformGridPanel>();
    if (!Grid) return;
    Grid->SetSlotPadding(FMargin(2.f, 2.f));
    CategoriesBox->AddChild(Grid);

    UPTGameInstance* GI = GetGI();
    for (int32 i = 0; i < CategoryNames.Num(); ++i)
    {
        const FName Cat = CategoryNames[i];
        UHorizontalBox* Cell  = WidgetTree->ConstructWidget<UHorizontalBox>();
        UCheckBox*      Chk   = WidgetTree->ConstructWidget<UCheckBox>();
        UTextBlock*     Label = WidgetTree->ConstructWidget<UTextBlock>();
        if (!Cell || !Chk || !Label) continue;

        // Estado inicial: ActiveCategories vacío = todas activas.
        const bool bActive = !GI || GI->PendingMatchSettings.ActiveCategories.Num() == 0
                          || GI->PendingMatchSettings.ActiveCategories.Contains(Cat);
        Chk->SetIsChecked(bActive);
        Chk->OnCheckStateChanged.AddDynamic(this, &UPTLobbyHUDWidget::OnCategoryChanged);

        // Display: los FName vienen tipo "CRIATURAS_MITOLOGICAS" → mostrar "CRIATURAS MITOLOGICAS".
        Label->SetText(FText::FromString(Cat.ToString().Replace(TEXT("_"), TEXT(" "))));

        Cell->AddChild(Chk);
        if (UHorizontalBoxSlot* LSlot = Cast<UHorizontalBoxSlot>(Cell->AddChild(Label)))
            LSlot->SetPadding(FMargin(4.f, 0.f, 8.f, 0.f));

        // Fila i → (fila i/3, columna i%3) → 3 columnas.
        if (UUniformGridSlot* GSlot = Cast<UUniformGridSlot>(Grid->AddChildToUniformGrid(Cell, i / Cols, i % Cols)))
            GSlot->SetHorizontalAlignment(HAlign_Left);

        CategoryChecks.Add(Chk);
    }
}

void UPTLobbyHUDWidget::OnCategoryChanged(bool /*bChecked*/)
{
    UPTGameInstance* GI = GetGI();
    if (!GI) return;

    TArray<FName> Active;
    for (int32 i = 0; i < CategoryChecks.Num(); ++i)
        if (CategoryChecks[i] && CategoryChecks[i]->IsChecked() && CategoryNames.IsValidIndex(i))
            Active.Add(CategoryNames[i]);

    // Todas marcadas → tratar como "sin filtro" (vacío = todas).
    if (Active.Num() == CategoryChecks.Num()) Active.Reset();
    GI->PendingMatchSettings.ActiveCategories = Active;
}

void UPTLobbyHUDWidget::RefreshHostSettingsUI()
{
    UPTGameInstance* GI = GetGI();
    if (!GI) return;
    const FPTMatchSettings& S = GI->PendingMatchSettings;

    if (TurnTimeText) TurnTimeText->SetText(FText::FromString(FString::Printf(TEXT("%d s"), FMath::RoundToInt(S.TurnDuration))));
    if (RoundsText)   RoundsText->SetText(FText::FromString(FString::Printf(TEXT("%d"), S.NumRounds)));
    if (RevealText)   RevealText->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(S.RevealFraction * 100.f))));

    // Dificultad: coloreada si activa (ActiveDifficulties vacío = todas activas).
    auto DiffActive = [&S](EPTWordDifficulty D){ return S.ActiveDifficulties.Num() == 0 || S.ActiveDifficulties.Contains(D); };
    auto Paint = [](UButton* B, bool bOn){ if (B) B->SetBackgroundColor(bOn ? FLinearColor(0.47f, 0.87f, 0.50f) : FLinearColor(0.45f, 0.45f, 0.45f)); };
    Paint(DiffFacilButton,   DiffActive(EPTWordDifficulty::Facil));
    Paint(DiffMediaButton,   DiffActive(EPTWordDifficulty::Media));
    Paint(DiffDificilButton, DiffActive(EPTWordDifficulty::Dificil));

    // Estado del banco / CSV.
    const bool bCustom = S.bUseCustomWords && S.CustomWords.Num() > 0;
    if (CSVStatusText)
    {
        if (bCustom)
        {
            FFormatOrderedArguments Args;
            Args.Add(FText::AsNumber(S.CustomWords.Num()));
            CSVStatusText->SetText(PTText::Format(TEXT("LOBBY_CSV_CUSTOM"), Args));
        }
        else CSVStatusText->SetText(PTText::Get(TEXT("LOBBY_CSV_DEFAULT")));
    }
    // Con CSV propio, el filtro por categorías del banco default no tiene sentido → deshabilitar.
    for (UCheckBox* C : CategoryChecks) if (C) C->SetIsEnabled(!bCustom);
}

void UPTLobbyHUDWidget::ToggleDifficulty(EPTWordDifficulty Diff)
{
    UPTGameInstance* GI = GetGI();
    if (!GI) return;
    TArray<EPTWordDifficulty>& A = GI->PendingMatchSettings.ActiveDifficulties;

    // "Vacío = todas" → expandir a las 3 para poder sacar una.
    if (A.Num() == 0) A = { EPTWordDifficulty::Facil, EPTWordDifficulty::Media, EPTWordDifficulty::Dificil };
    if (A.Contains(Diff)) A.Remove(Diff); else A.AddUnique(Diff);
    if (A.Num() == 0) A = { EPTWordDifficulty::Facil, EPTWordDifficulty::Media, EPTWordDifficulty::Dificil }; // no dejar sin ninguna
    if (A.Num() == 3) A.Reset(); // las 3 = sin filtro
    RefreshHostSettingsUI();
}

void UPTLobbyHUDWidget::OnTurnTimeMinus() { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.TurnDuration = FMath::Clamp(GI->PendingMatchSettings.TurnDuration - 15.f, 15.f, 300.f); RefreshHostSettingsUI(); } }
void UPTLobbyHUDWidget::OnTurnTimePlus()  { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.TurnDuration = FMath::Clamp(GI->PendingMatchSettings.TurnDuration + 15.f, 15.f, 300.f); RefreshHostSettingsUI(); } }
void UPTLobbyHUDWidget::OnRoundsMinus()   { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.NumRounds = FMath::Clamp(GI->PendingMatchSettings.NumRounds - 1, 1, 10); RefreshHostSettingsUI(); } }
void UPTLobbyHUDWidget::OnRoundsPlus()    { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.NumRounds = FMath::Clamp(GI->PendingMatchSettings.NumRounds + 1, 1, 10); RefreshHostSettingsUI(); } }
void UPTLobbyHUDWidget::OnRevealMinus()   { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.RevealFraction = FMath::Clamp(GI->PendingMatchSettings.RevealFraction - 0.1f, 0.f, 0.9f); RefreshHostSettingsUI(); } }
void UPTLobbyHUDWidget::OnRevealPlus()    { if (UPTGameInstance* GI=GetGI()){ GI->PendingMatchSettings.RevealFraction = FMath::Clamp(GI->PendingMatchSettings.RevealFraction + 0.1f, 0.f, 0.9f); RefreshHostSettingsUI(); } }
void UPTLobbyHUDWidget::OnDiffFacil()   { ToggleDifficulty(EPTWordDifficulty::Facil); }
void UPTLobbyHUDWidget::OnDiffMedia()   { ToggleDifficulty(EPTWordDifficulty::Media); }
void UPTLobbyHUDWidget::OnDiffDificil() { ToggleDifficulty(EPTWordDifficulty::Dificil); }

void UPTLobbyHUDWidget::OnLoadCSV()  { if (UPTGameInstance* GI=GetGI()){ GI->LoadCustomWordsFromCSVDialog(); RefreshHostSettingsUI(); } }
void UPTLobbyHUDWidget::OnClearCSV() { if (UPTGameInstance* GI=GetGI()){ GI->ClearCustomWords(); RefreshHostSettingsUI(); } }

void UPTLobbyHUDWidget::OnGameSettingsClicked()
{
    bSettingsOpen = true;
    if (!bCategoryChecksBuilt) BuildCategoryChecks();
    RefreshHostSettingsUI();
    if (HostSettingsPanel)
    {
        HostSettingsPanel->SetVisibility(ESlateVisibility::Visible);
        PlayPopInOn(HostSettingsPanel); // blop del panel de settings
    }
}

void UPTLobbyHUDWidget::OnCloseSettingsClicked()
{
    // Los ajustes ya se aplican en vivo (cada control escribe al GameInstance al tocarlo), así
    // que "aplicar y cerrar" es simplemente cerrar el panel.
    bSettingsOpen = false;
    if (HostSettingsPanel) HostSettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
}
