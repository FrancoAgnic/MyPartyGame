#include "PTGameplayHUDWidget.h"
#include "PTSculptPlayerController.h"
#include "PTScoreRowWidget.h"
#include "../Lobby/PTPlayerState.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool UPTGameplayHUDWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (BtnWord0) BtnWord0->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnWord0);
    if (BtnWord1) BtnWord1->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnWord1);
    if (BtnWord2) BtnWord2->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnWord2);
    if (ChatInput) ChatInput->OnTextCommitted.AddDynamic(this, &UPTGameplayHUDWidget::OnChatCommitted);
    if (BtnPlayAgain)   BtnPlayAgain->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnPlayAgain);
    if (BtnReturnLobby) BtnReturnLobby->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnReturnLobby);

    return true;
}

void UPTGameplayHUDWidget::ShowHUD()
{
    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);
    RefreshTick();
    if (UWorld* World = GetWorld())
        World->GetTimerManager().SetTimer(RefreshTimer, this, &UPTGameplayHUDWidget::RefreshTick, 0.1f, true);
}

void UPTGameplayHUDWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(RefreshTimer);
    Super::NativeDestruct();
}

APTSculptGameState* UPTGameplayHUDWidget::GetGS() const
{
    return GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
}

APTSculptPlayerController* UPTGameplayHUDWidget::GetSculptPC() const
{
    return Cast<APTSculptPlayerController>(GetOwningPlayer());
}

void UPTGameplayHUDWidget::RefreshTick()
{
    APTSculptGameState* G = GetGS();

    // Engancharse al chat una sola vez, cuando el GameState ya esté replicado.
    if (G && !bChatBound)
    {
        G->OnChatLine.AddDynamic(this, &UPTGameplayHUDWidget::OnChatLine);
        bChatBound = true;
    }
    if (!G) return;

    APTSculptPlayerController* PC = GetSculptPC();
    const bool bSculptor = G->IsLocalPlayerSculptor();

    // El "quién esculpe" ya NO va arriba: se muestra en el marcador con el emoji 🖌️.
    // Arriba queda solo el timer + la palabra. Limpiamos TxtSculptor si sigue en el WBP.
    if (TxtSculptor)
        TxtSculptor->SetText(FText::GetEmpty());

    // ── Panel de palabras + texto de la palabra, según la fase ──
    FString WordText;
    bool bShowPanel = false;
    switch (G->TurnPhase)
    {
    case EPTTurnPhase::ChoosingWord:
        if (bSculptor)
        {
            bShowPanel = true;
            const TArray<FString>& Ch = PC ? PC->CurrentWordChoices : TArray<FString>();
            if (TxtWord0) TxtWord0->SetText(FText::FromString(Ch.IsValidIndex(0) ? Ch[0] : FString()));
            if (TxtWord1) TxtWord1->SetText(FText::FromString(Ch.IsValidIndex(1) ? Ch[1] : FString()));
            if (TxtWord2) TxtWord2->SetText(FText::FromString(Ch.IsValidIndex(2) ? Ch[2] : FString()));
        }
        break;
    case EPTTurnPhase::Drawing:
        WordText = bSculptor && PC ? PC->CurrentSecretWord : G->MaskedWord;
        break;
    case EPTTurnPhase::TurnEnd:
        WordText = G->MaskedWord; // ya revelada
        break;
    case EPTTurnPhase::WaitingForPlayers:
        WordText = TEXT("Esperando jugadores...");
        break;
    default: break; // GameOver (el panel de resultados tapa esto)
    }
    if (WordPickPanel)
        WordPickPanel->SetVisibility(bShowPanel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (TxtWord)
        TxtWord->SetText(FText::FromString(WordText));

    // La caja de texto SÓLO se muestra mientras el chat está abierto (Enter). Colapsada,
    // Tab no puede darle foco ni congelar el input. Vale para todos, incluido el escultor
    // (que ahora también puede chatear con Enter; el anti-spoiler descarta la palabra).
    if (ChatInput)
        ChatInput->SetVisibility(bChatOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    // ── Reloj (solo en Drawing) ──
    if (TxtTimer)
    {
        if (G->TurnPhase == EPTTurnPhase::Drawing)
            TxtTimer->SetText(FText::FromString(FString::Printf(TEXT("%d Seg"),
                              FMath::CeilToInt(G->GetTurnSecondsRemaining()))));
        else
            TxtTimer->SetText(FText::GetEmpty());
    }

    // ── Ronda + marcador en vivo ──
    if (TxtRound)
    {
        if (G->TotalRounds > 0 && G->TurnPhase != EPTTurnPhase::WaitingForPlayers &&
            G->TurnPhase != EPTTurnPhase::GameOver)
            TxtRound->SetText(FText::FromString(FString::Printf(TEXT("Round %d/%d"),
                                                               G->CurrentRound, G->TotalRounds)));
        else
            TxtRound->SetText(FText::GetEmpty());
    }
    RebuildScoreboard();

    // ── Pantalla de fin de partida ──
    const bool bGameOver = (G->TurnPhase == EPTTurnPhase::GameOver);
    if (ResultsPanel)
        ResultsPanel->SetVisibility(bGameOver ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bGameOver)
    {
        if (TxtResults)
            TxtResults->SetText(FText::FromString(TEXT("Resultados\n") + BuildScoreboard()));
        // Los botones de decisión son solo para el anfitrión.
        const bool bHost = IsLocalPlayerHost();
        const ESlateVisibility BtnVis = bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        if (BtnPlayAgain)   BtnPlayAgain->SetVisibility(BtnVis);
        if (BtnReturnLobby) BtnReturnLobby->SetVisibility(BtnVis);
    }

    // ── Modo de input ──
    // Base = Game Only (moverse/esculpir siempre funcionan). Solo se muestra el cursor
    // cuando de verdad hace falta la UI: el escultor eligiendo palabra, con el chat
    // abierto (Enter), o en la pantalla de fin (para clickear los botones).
    const bool bWantUI = bChatOpen || bGameOver ||
                         (bSculptor && G->TurnPhase == EPTTurnPhase::ChoosingWord);
    ApplyInputMode(!bWantUI);
}

void UPTGameplayHUDWidget::FocusChat()
{
    if (bChatOpen) return;
    bChatOpen = true;
    if (ChatInput) ChatInput->SetVisibility(ESlateVisibility::Visible);
    ApplyInputMode(false); // GameAndUI + cursor
    // Recién visible: enfocar en el próximo tick (Slate necesita un frame para darle foco).
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimerForNextTick([this]() { if (ChatInput) ChatInput->SetKeyboardFocus(); });
}

void UPTGameplayHUDWidget::ApplyInputMode(bool bGameOnly)
{
    if (bInputModeInit && bGameOnly == bWantsGameOnly) return; // solo al cambiar
    bInputModeInit = true;
    bWantsGameOnly = bGameOnly;

    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;
    if (bGameOnly)
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->SetShowMouseCursor(false);
    }
    else
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
        PC->SetShowMouseCursor(true);
    }
}

void UPTGameplayHUDWidget::OnBtnWord0() { ChooseWord(0); }
void UPTGameplayHUDWidget::OnBtnWord1() { ChooseWord(1); }
void UPTGameplayHUDWidget::OnBtnWord2() { ChooseWord(2); }

void UPTGameplayHUDWidget::ChooseWord(int32 Index)
{
    if (APTSculptPlayerController* PC = GetSculptPC())
        PC->Server_ChooseWord(Index);
    if (WordPickPanel)
        WordPickPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTGameplayHUDWidget::OnChatCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
    {
        const FString Msg = Text.ToString();
        if (!Msg.IsEmpty())
            if (APTSculptPlayerController* PC = GetSculptPC())
                PC->Server_SendChat(Msg);
    }
    // Enviar (Enter) o perder el foco: limpiar, COLAPSAR (para que Tab no la reenfoque)
    // y devolver el control al juego.
    if (ChatInput)
    {
        ChatInput->SetText(FText::GetEmpty());
        ChatInput->SetVisibility(ESlateVisibility::Collapsed);
    }
    bChatOpen = false;
    ApplyInputMode(true); // Game Only
}

void UPTGameplayHUDWidget::OnChatLine(const FString& Name, const FString& Message, EPTChatType Type)
{
    const FString ShortName = Name.Left(10); // nombre máximo 10 caracteres
    // El nombre va en color (estilo "name" del RichTextBlock); el mensaje queda blanco.
    FString Line;
    switch (Type)
    {
    case EPTChatType::Correct: Line = FString::Printf(TEXT("<name>%s</> <correct>adivinó la palabra!</>"), *ShortName); break;
    case EPTChatType::System:  Line = Message; break;
    default:                   Line = FString::Printf(TEXT("<name>%s</>: %s"), *ShortName, *Message); break; // Normal
    }
    ChatLog += Line + TEXT("\n");
    if (TxtChat)    TxtChat->SetText(FText::FromString(ChatLog));
    if (ChatScroll) ChatScroll->ScrollToEnd();
}

void UPTGameplayHUDWidget::OnBtnPlayAgain()
{
    if (APTSculptPlayerController* PC = GetSculptPC())
        PC->Server_RequestPlayAgain();
}

void UPTGameplayHUDWidget::OnBtnReturnLobby()
{
    if (APTSculptPlayerController* PC = GetSculptPC())
        PC->Server_RequestReturnToLobby();
}

FString UPTGameplayHUDWidget::BuildScoreboard() const
{
    APTSculptGameState* G = GetGS();
    if (!G) return FString();

    TArray<APTPlayerState*> Players;
    for (APlayerState* PS : G->PlayerArray)
        if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
            Players.Add(PT);

    Players.Sort([](const APTPlayerState& A, const APTPlayerState& B){ return A.GameScore > B.GameScore; });

    FString Out;
    for (const APTPlayerState* PT : Players)
        Out += FString::Printf(TEXT("%s: %d\n"), *PT->DisplayName.Left(10), PT->GameScore);
    return Out;
}

void UPTGameplayHUDWidget::RebuildScoreboard()
{
    if (!ScoreboardBox || !ScoreRowClass) return;
    APTSculptGameState* G = GetGS();
    if (!G) return;

    TArray<APTPlayerState*> Players;
    for (APlayerState* PS : G->PlayerArray)
        if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
            Players.Add(PT);
    Players.Sort([](const APTPlayerState& A, const APTPlayerState& B){ return A.GameScore > B.GameScore; });

    // Firma del estado visible: solo reconstruimos las filas si algo cambió (no cada tick).
    FString Sig;
    for (const APTPlayerState* PT : Players)
        Sig += FString::Printf(TEXT("%s:%d:%d|"), *PT->DisplayName,
                               PT->GameScore, PT == G->CurrentSculptor ? 1 : 0);
    if (Sig == CachedScoreSig) return;
    CachedScoreSig = Sig;

    ScoreboardBox->ClearChildren();
    for (APTPlayerState* PT : Players)
    {
        UPTScoreRowWidget* Row = CreateWidget<UPTScoreRowWidget>(GetOwningPlayer(), ScoreRowClass);
        if (!Row) continue;
        Row->SetRow(PT->DisplayName, PT->GameScore, PT == G->CurrentSculptor);
        ScoreboardBox->AddChild(Row);
    }
}

bool UPTGameplayHUDWidget::IsLocalPlayerHost() const
{
    if (const APlayerController* PC = GetOwningPlayer())
        if (const APTPlayerState* PT = PC->GetPlayerState<APTPlayerState>())
            return PT->bIsHost;
    return false;
}
