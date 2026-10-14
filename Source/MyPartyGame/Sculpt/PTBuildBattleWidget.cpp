#include "PTBuildBattleWidget.h"
#include "../Lobby/PTPlayerState.h"
#include "../PTTextTable.h"
#include "PTSculptPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
    const FLinearColor BBW_Ink(1.f, 0.97f, 0.92f, 1.f);
    const FLinearColor BBW_Dim(1.f, 0.97f, 0.92f, 0.55f);
    const FLinearColor BBW_Dark(0.06f, 0.04f, 0.10f, 1.f);
    const FLinearColor BBW_Panel(0.03f, 0.02f, 0.06f, 0.88f);
    const FLinearColor BBW_Chip(0.16f, 0.12f, 0.24f, 0.95f);
    const FLinearColor BBW_ChipOff(0.16f, 0.12f, 0.24f, 0.40f);
    const FLinearColor BBW_Gold(0.98f, 0.74f, 0.16f, 1.f);
    const FLinearColor BBW_Mine(0.55f, 0.85f, 1.f, 1.f);

    // Solo reescribir si cambió: el sistema de idiomas re-traduce lo que está en pantalla y no conviene
    // pisar textos cada refresh.
    void BBW_SetText(UTextBlock* T, const FText& Text)
    {
        if (T && !T->GetText().EqualTo(Text)) T->SetText(Text);
    }

    void BBW_Show(UWidget* W, bool bShow)
    {
        const ESlateVisibility V = bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
        if (W && W->GetVisibility() != V) W->SetVisibility(V);
    }

    FText BBW_Fmt(const TCHAR* Key, const FText& A0, const FText& A1 = FText::GetEmpty())
    {
        FFormatOrderedArguments Args;
        Args.Add(A0);
        Args.Add(A1);
        return PTText::Format(FName(Key), Args);
    }
}

TSharedRef<SWidget> UPTBuildBattleWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

void UPTBuildBattleWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::HitTestInvisible); // nunca roba el mouse ni el foco
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateUObject(this, &UPTBuildBattleWidget::Refresh),
                                      0.1f, true);
    Refresh();
}

void UPTBuildBattleWidget::NativeDestruct()
{
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(RefreshTimer);
    Super::NativeDestruct();
}

UTextBlock* UPTBuildBattleWidget::MakeText(int32 Size, bool bBold, const FLinearColor& Color)
{
    UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo F = T->GetFont();
    F.Size = Size;
    if (bBold) F.TypefaceFontName = FName(TEXT("Bold"));
    T->SetFont(F);
    T->SetColorAndOpacity(FSlateColor(Color));
    T->SetJustification(ETextJustify::Center);
    return T;
}

UBorder* UPTBuildBattleWidget::MakeCard(UWidget* Content)
{
    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 20.f));
    Card->SetBrushColor(BBW_Panel);
    Card->SetPadding(FMargin(28.f, 18.f));
    Card->SetContent(Content);
    Card->SetVisibility(ESlateVisibility::Collapsed);
    return Card;
}

void UPTBuildBattleWidget::BuildTree()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // ── Presentación del modo (arriba, debajo de la palabra del HUD) ──
    {
        UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
        UTextBlock* Title = MakeText(34, true, BBW_Gold);
        Title->SetText(PTText::Get(TEXT("BB_TITLE")));
        Col->AddChildToVerticalBox(Title);
        IntroWord = MakeText(24, true, BBW_Ink);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(IntroWord)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));
        USizeBox* BodyBox = WidgetTree->ConstructWidget<USizeBox>();
        BodyBox->SetWidthOverride(560.f);
        IntroBody = MakeText(17, false, BBW_Ink);
        IntroBody->SetAutoWrapText(true);
        BodyBox->SetContent(IntroBody);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(BodyBox)) S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

        IntroCard = MakeCard(Col);
        if (UOverlaySlot* S = Root->AddChildToOverlay(IntroCard))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetVerticalAlignment(VAlign_Top);
            S->SetPadding(FMargin(0.f, 170.f, 0.f, 0.f));
        }
    }

    // ── Votación (abajo al centro) ──
    {
        UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
        VoteHeader = MakeText(24, true, BBW_Ink);
        Col->AddChildToVerticalBox(VoteHeader);
        VotePrompt = MakeText(16, false, BBW_Ink);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(VotePrompt)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 12.f));

        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        for (int32 i = 0; i < 5; ++i)
        {
            UVerticalBox* ChipCol = WidgetTree->ConstructWidget<UVerticalBox>();
            UTextBlock* Num = MakeText(26, true, BBW_Ink);
            Num->SetText(FText::AsNumber(i + 1));
            ChipCol->AddChildToVerticalBox(Num);
            UTextBlock* Label = MakeText(13, true, BBW_Ink);
            Label->SetText(PTText::Get(FName(*FString::Printf(TEXT("BB_VOTE_%d"), i + 1))));
            ChipCol->AddChildToVerticalBox(Label);

            USizeBox* ChipSize = WidgetTree->ConstructWidget<USizeBox>();
            ChipSize->SetWidthOverride(118.f);
            ChipSize->SetHeightOverride(72.f);
            ChipSize->SetContent(ChipCol);

            UBorder* Chip = WidgetTree->ConstructWidget<UBorder>();
            Chip->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 14.f));
            Chip->SetBrushColor(BBW_Chip);
            Chip->SetPadding(FMargin(4.f, 6.f));
            Chip->SetVerticalAlignment(VAlign_Center);
            Chip->SetContent(ChipSize);
            if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Chip)) S->SetPadding(FMargin(5.f, 0.f));

            VoteChips.Add(Chip);
            VoteChipNums.Add(Num);
            VoteChipLabels.Add(Label);
        }
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Row)) S->SetHorizontalAlignment(HAlign_Center);

        VoteCard = MakeCard(Col);
        if (UOverlaySlot* S = Root->AddChildToOverlay(VoteCard))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetVerticalAlignment(VAlign_Bottom);
            S->SetPadding(FMargin(0.f, 0.f, 0.f, 40.f));
        }
    }

    // ── Resultados de la ronda (a la derecha: el centro queda para la escultura ganadora) ──
    {
        UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
        ResultsTitle = MakeText(24, true, BBW_Ink);
        Col->AddChildToVerticalBox(ResultsTitle);
        ResultsWinner = MakeText(30, true, BBW_Gold);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ResultsWinner)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 14.f));
        ResultsList = WidgetTree->ConstructWidget<UVerticalBox>();
        Col->AddChildToVerticalBox(ResultsList);
        ResultsFooter = MakeText(15, false, BBW_Dim);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ResultsFooter)) S->SetPadding(FMargin(0.f, 14.f, 0.f, 0.f));

        USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
        Width->SetWidthOverride(470.f);
        Width->SetContent(Col);
        ResultsCard = MakeCard(Width);
        if (UOverlaySlot* S = Root->AddChildToOverlay(ResultsCard))
        {
            S->SetHorizontalAlignment(HAlign_Right);
            S->SetVerticalAlignment(VAlign_Center);
            S->SetPadding(FMargin(0.f, 0.f, 40.f, 0.f));
        }
    }
}

void UPTBuildBattleWidget::Refresh()
{
    UWorld* W = GetWorld();
    const APTSculptGameState* G = W ? W->GetGameState<APTSculptGameState>() : nullptr;
    const bool bBB = G && G->IsBuildBattle();
    const EPTTurnPhase Phase = G ? G->TurnPhase : EPTTurnPhase::WaitingForPlayers;
    const double Now = W ? W->GetTimeSeconds() : 0.0;

    // Arranque de cada ronda de esculpido → vuelve a mostrarse la presentación.
    if (bBB && Phase == EPTTurnPhase::Drawing && (LastPhase != EPTTurnPhase::Drawing || G->CurrentRound != LastRound))
        DrawingStarted = Now;
    LastPhase = Phase;
    LastRound = G ? G->CurrentRound : -1;

    // ── Presentación: en la espera y los primeros segundos de cada ronda ──
    const bool bDrawingIntro = Phase == EPTTurnPhase::Drawing && DrawingStarted >= 0.0 && Now - DrawingStarted < IntroSeconds;
    const bool bIntro = bBB && (Phase == EPTTurnPhase::WaitingForPlayers || bDrawingIntro);
    BBW_Show(IntroCard, bIntro);
    if (bIntro)
    {
        BBW_SetText(IntroBody, PTText::Get(TEXT("BB_INTRO")));
        const bool bWord = Phase == EPTTurnPhase::Drawing && !G->MaskedWord.IsEmpty();
        BBW_Show(IntroWord, bWord);
        if (bWord) BBW_SetText(IntroWord, BBW_Fmt(TEXT("BB_INTRO_WORD"), FText::FromString(G->MaskedWord)));
    }

    const bool bVote = bBB && Phase == EPTTurnPhase::Showcase;
    BBW_Show(VoteCard, bVote);
    if (bVote) RefreshVoteCard(G);

    const bool bResults = bBB && Phase == EPTTurnPhase::RoundResults;
    BBW_Show(ResultsCard, bResults);
    if (bResults) RefreshResultsCard(G);
}

void UPTBuildBattleWidget::RefreshVoteCard(const APTSculptGameState* G)
{
    const int32 Secs = FMath::Max(0, FMath::CeilToInt(G->GetPhaseSecondsRemaining()));
    const FText Header = FText::Format(FText::FromString(TEXT("{0}   ·   {1} {2}")),
        BBW_Fmt(TEXT("BB_SHOWCASE_HEADER"), FText::AsNumber(G->BBShowcaseIndex), FText::AsNumber(G->BBShowcaseCount)),
        FText::AsNumber(Secs), PTText::Get(TEXT("HUD_SECONDS_SHORT")));
    BBW_SetText(VoteHeader, Header);

    const bool bOwn = G->IsLocalShowcaseOwn();
    const APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(GetOwningPlayer());
    const int32 MyVote = PC ? PC->GetBuildBattleVote() : 0;
    BBW_SetText(VotePrompt, PTText::Get(bOwn ? TEXT("BB_VOTE_OWN") : (MyVote > 0 ? TEXT("BB_VOTE_SENT") : TEXT("BB_VOTE_PROMPT"))));

    for (int32 i = 0; i < VoteChips.Num(); ++i)
    {
        const bool bSel = !bOwn && MyVote == i + 1;
        const FLinearColor Fill = bOwn ? BBW_ChipOff : (bSel ? BBW_Gold : BBW_Chip);
        const FLinearColor Ink  = bOwn ? BBW_Dim : (bSel ? BBW_Dark : BBW_Ink);
        if (VoteChips[i]->GetBrushColor() != Fill) VoteChips[i]->SetBrushColor(Fill);
        VoteChipNums[i]->SetColorAndOpacity(FSlateColor(Ink));
        VoteChipLabels[i]->SetColorAndOpacity(FSlateColor(Ink));
    }
}

void UPTBuildBattleWidget::RefreshResultsCard(const APTSculptGameState* G)
{
    const TArray<FPTBuildBattleResult>& Rows = G->BBRoundResults;
    BBW_SetText(ResultsTitle, BBW_Fmt(TEXT("BB_RESULTS_TITLE"), FText::AsNumber(G->CurrentRound)));
    const bool bAnyVotes = Rows.Num() > 0 && Rows[0].NumVotes > 0;
    BBW_SetText(ResultsWinner, bAnyVotes
        ? BBW_Fmt(TEXT("BB_RESULTS_WINNER"), FText::FromString(Rows[0].Player ? Rows[0].Player->GetDisplayNameSafe() : Rows[0].Name))
        : PTText::Get(TEXT("BB_RESULTS_NO_VOTES")));
    const int32 Secs = FMath::Max(0, FMath::CeilToInt(G->GetPhaseSecondsRemaining()));
    BBW_SetText(ResultsFooter, BBW_Fmt(TEXT("BB_RESULTS_NEXT"), FText::AsNumber(Secs)));

    // La tabla solo se rearma si cambió (nombres, votos o idioma).
    const APlayerState* LocalPS = GetOwningPlayer() ? GetOwningPlayer()->PlayerState : nullptr;
    FString Sig = FString::Printf(TEXT("%d|%d|"), G->CurrentRound, PTText::GetCurrentLanguageIndex());
    for (const FPTBuildBattleResult& R : Rows)
        Sig += FString::Printf(TEXT("%s/%.2f/%d/%d;"), *R.Name, R.AvgStars, R.NumVotes, R.Points);
    if (Sig == CachedResultsSig) return;
    CachedResultsSig = Sig;

    ResultsList->ClearChildren();
    for (int32 i = 0; i < Rows.Num(); ++i)
    {
        const FPTBuildBattleResult& R = Rows[i];
        const bool bMine = R.Player && R.Player == LocalPS;
        const FLinearColor Ink = (i == 0 && bAnyVotes) ? BBW_Gold : (bMine ? BBW_Mine : BBW_Ink);
        const FString Name = R.Player ? R.Player->GetDisplayNameSafe() : R.Name;

        UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>();
        UTextBlock* NameT = MakeText(19, true, Ink);
        NameT->SetJustification(ETextJustify::Left);
        NameT->SetClipping(EWidgetClipping::ClipToBounds);
        NameT->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis); // nombres largos: "Nombre…" sin pisar las estrellas
        NameT->SetText(FText::FromString(FString::Printf(TEXT("%d. %s%s"), i + 1, *Name,
            bMine ? *(TEXT(" ") + PTText::GetStr(TEXT("BB_YOU"))) : TEXT(""))));
        if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(NameT)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

        UTextBlock* Stars = MakeText(19, true, Ink);
        Stars->SetText(R.NumVotes > 0 ? FText::FromString(FString::Printf(TEXT("★ %.1f"), R.AvgStars)) : FText::FromString(TEXT("—")));
        if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Stars)) S->SetPadding(FMargin(12.f, 0.f));

        UTextBlock* Votes = MakeText(14, false, BBW_Dim);
        Votes->SetText(BBW_Fmt(TEXT("BB_RESULTS_VOTES"), FText::AsNumber(R.NumVotes)));
        if (UHorizontalBoxSlot* S = Line->AddChildToHorizontalBox(Votes))
        {
            S->SetVerticalAlignment(VAlign_Center);
            S->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
        }

        UTextBlock* Pts = MakeText(19, true, Ink);
        Pts->SetText(FText::FromString(FString::Printf(TEXT("+%d"), R.Points)));
        Line->AddChildToHorizontalBox(Pts);

        if (UVerticalBoxSlot* S = ResultsList->AddChildToVerticalBox(Line)) S->SetPadding(FMargin(0.f, 3.f));
    }
}
