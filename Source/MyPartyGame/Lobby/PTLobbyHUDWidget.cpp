// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyHUDWidget.h"
#include "PTGameState.h"
#include "PTPlayerState.h"
#include "PTLobbyPlayerController.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Blueprint/WidgetTree.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "../PTNetStats.h"
#include "../PTGameInstance.h"

bool UPTLobbyHUDWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (CopyCodeButton)  CopyCodeButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnCopyCodeClicked);
    if (LeaveGameButton) LeaveGameButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnLeaveGameClicked);
    if (StartGameButton) StartGameButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnStartGameClicked);
    if (ReadyButton)      ReadyButton->OnClicked.AddDynamic(this, &UPTLobbyHUDWidget::OnReadyClicked);

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

        for (APlayerState* PS : PTGS->PlayerArray)
        {
            APTPlayerState* PTPS = Cast<APTPlayerState>(PS);
            if (!PTPS) continue;

            UTextBlock* Row = NewObject<UTextBlock>(this);
            FString Label = PTPS->DisplayName;
            if (PTPS->bIsHost) Label += TEXT(" (Host)");
            Label += PTPS->bIsReady ? TEXT(" — Listo") : TEXT(" — Esperando");
            Row->SetText(FText::FromString(Label));
            PlayersBox->AddChildToVerticalBox(Row);
        }
    }

    if (PlayersCountText)
    {
        PlayersCountText->SetText(FText::FromString(
            FString::Printf(TEXT("%d/%d"), PTGS->PlayerArray.Num(), PTGS->MaxPlayers)));
    }

    if (LobbyStatusText)
    {
        FString Status;
        switch (PTGS->LobbyState)
        {
        case EPTLobbyState::Starting: Status = TEXT("Starting..."); break;
        case EPTLobbyState::InGame:   Status = TEXT("In game");     break;
        default:                      Status = TEXT("Waiting for players...");
        }
        LobbyStatusText->SetText(FText::FromString(Status));
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
            ReadyButtonText->SetText(FText::FromString(bReady ? TEXT("Ready") : TEXT("Not Ready")));
        if (ReadyButton)
            ReadyButton->SetBackgroundColor(bReady
                ? FLinearColor(0.47f, 0.87f, 0.50f)    // verde pastel (+50% saturación)
                : FLinearColor(0.94f, 0.36f, 0.36f));  // rojo pastel (+50% saturación)
    }

    if (CountdownText)
    {
        const int32 Seconds = PTGS->CountdownSecondsRemaining;
        if (Seconds >= 0)
        {
            CountdownText->SetVisibility(ESlateVisibility::Visible);
            CountdownText->SetText(FText::FromString(FString::Printf(TEXT("Arranca en %d..."), Seconds)));
        }
        else
        {
            CountdownText->SetVisibility(ESlateVisibility::Collapsed);
        }
    }

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

    CategoryNames = PTDefaultWordCategories();
    CategoryChecks.Reset();

    UPTGameInstance* GI = GetGI();
    for (const FName& Cat : CategoryNames)
    {
        UHorizontalBox* Row  = WidgetTree->ConstructWidget<UHorizontalBox>();
        UCheckBox*      Chk   = WidgetTree->ConstructWidget<UCheckBox>();
        UTextBlock*     Label = WidgetTree->ConstructWidget<UTextBlock>();
        if (!Row || !Chk || !Label) continue;

        // Estado inicial: ActiveCategories vacío = todas activas.
        const bool bActive = !GI || GI->PendingMatchSettings.ActiveCategories.Num() == 0
                          || GI->PendingMatchSettings.ActiveCategories.Contains(Cat);
        Chk->SetIsChecked(bActive);
        Chk->OnCheckStateChanged.AddDynamic(this, &UPTLobbyHUDWidget::OnCategoryChanged);

        Label->SetText(FText::FromName(Cat));

        Row->AddChild(Chk);
        if (UHorizontalBoxSlot* LSlot = Cast<UHorizontalBoxSlot>(Row->AddChild(Label)))
            LSlot->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f));

        CategoriesBox->AddChild(Row);
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
        CSVStatusText->SetText(FText::FromString(bCustom
            ? FString::Printf(TEXT("CSV propio: %d palabras"), S.CustomWords.Num())
            : TEXT("Banco default")));
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
    if (HostSettingsPanel) HostSettingsPanel->SetVisibility(ESlateVisibility::Visible);
}

void UPTLobbyHUDWidget::OnCloseSettingsClicked()
{
    // Los ajustes ya se aplican en vivo (cada control escribe al GameInstance al tocarlo), así
    // que "aplicar y cerrar" es simplemente cerrar el panel.
    bSettingsOpen = false;
    if (HostSettingsPanel) HostSettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
}
