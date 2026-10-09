#include "PTTutorialChoiceWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"

namespace
{
    const FLinearColor TCW_Ink(1.f, 0.97f, 0.92f, 1.f);
    const FLinearColor TCW_Panel(0.03f, 0.02f, 0.06f, 0.97f);
    const FLinearColor TCW_Yes(0.18f, 0.62f, 0.32f, 1.f);
    const FLinearColor TCW_No(0.35f, 0.20f, 0.45f, 1.f);
}

void UPTTutorialChoiceWidget::Setup(const FText& Question, const FText& YesLabel, const FText& NoLabel)
{
    PendingQuestion = Question; PendingYes = YesLabel; PendingNo = NoLabel;
    if (QuestionText) QuestionText->SetText(Question);
    if (YesText)      YesText->SetText(YesLabel);
    if (NoText)       NoText->SetText(NoLabel);
}

TSharedRef<SWidget> UPTTutorialChoiceWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

UButton* UPTTutorialChoiceWidget::MakeButton(const FText& Label, FName Name, const FLinearColor& Fill, UTextBlock** OutText)
{
    UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    FButtonStyle Style = B->GetStyle();
    Style.Normal  = FSlateRoundedBoxBrush(Fill, 12.f);
    Style.Hovered = FSlateRoundedBoxBrush(Fill * 1.3f, 12.f);
    Style.Pressed = FSlateRoundedBoxBrush(Fill * 0.8f, 12.f);
    Style.NormalPadding = FMargin(26.f, 12.f);
    Style.PressedPadding = FMargin(26.f, 12.f);
    B->SetStyle(Style);
    UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo F = T->GetFont(); F.Size = 22; F.TypefaceFontName = FName(TEXT("Bold"));
    T->SetFont(F); T->SetColorAndOpacity(FSlateColor(TCW_Ink)); T->SetText(Label);
    B->AddChild(T);
    if (OutText) *OutText = T;
    return B;
}

void UPTTutorialChoiceWidget::BuildTree()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // Fondo oscuro que bloquea lo de atrás.
    UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
    Dim->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 0.f));
    Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.6f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Dim)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 24.f));
    Card->SetBrushColor(TCW_Panel);
    Card->SetPadding(FMargin(40.f, 32.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Card)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }

    USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
    CardSize->SetWidthOverride(620.f);
    Card->SetContent(CardSize);

    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    CardSize->SetContent(Col);

    QuestionText = WidgetTree->ConstructWidget<UTextBlock>();
    { FSlateFontInfo F = QuestionText->GetFont(); F.Size = 26; F.TypefaceFontName = FName(TEXT("Bold")); QuestionText->SetFont(F); }
    QuestionText->SetColorAndOpacity(FSlateColor(TCW_Ink));
    QuestionText->SetJustification(ETextJustify::Center);
    QuestionText->SetAutoWrapText(true);
    QuestionText->SetText(PendingQuestion);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(QuestionText)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 24.f));

    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    UButton* YesB = MakeButton(PendingYes, TEXT("YesButton"), TCW_Yes, &YesText);
    UButton* NoB  = MakeButton(PendingNo,  TEXT("NoButton"),  TCW_No,  &NoText);
    YesB->OnClicked.AddDynamic(this, &UPTTutorialChoiceWidget::HandleYes);
    NoB->OnClicked.AddDynamic(this, &UPTTutorialChoiceWidget::HandleNo);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(NoB))  { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f)); S->SetHorizontalAlignment(HAlign_Center); }
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(YesB)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(10.f, 0.f, 0.f, 0.f)); S->SetHorizontalAlignment(HAlign_Center); }
    Col->AddChildToVerticalBox(Row);
}

void UPTTutorialChoiceWidget::HandleYes() { OnYes.Broadcast(); }
void UPTTutorialChoiceWidget::HandleNo()  { OnNo.Broadcast(); }
