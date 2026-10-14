#include "PTStreamerZoneWidget.h"
#include "../PTTextTable.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
    const FLinearColor SZW_Ink(1.f, 0.97f, 0.92f, 1.f);
    const FLinearColor SZW_Muted(1.f, 0.97f, 0.92f, 0.6f);
    const FLinearColor SZW_Gold(0.98f, 0.78f, 0.25f, 1.f);
    const FLinearColor SZW_Card(0.03f, 0.02f, 0.06f, 0.92f);
    const FLinearColor SZW_Placeholder(0.03f, 0.02f, 0.06f, 0.45f);
    const FLinearColor SZW_Button(0.35f, 0.20f, 0.45f, 1.f);
}

TSharedRef<SWidget> UPTStreamerZoneWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

void UPTStreamerZoneWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::SelfHitTestInvisible); // solo los botones reciben clicks
}

UTextBlock* UPTStreamerZoneWidget::MakeText(int32 Size, const FLinearColor& Color, bool bBold)
{
    UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo F = T->GetFont();
    F.Size = Size;
    if (bBold) F.TypefaceFontName = FName(TEXT("Bold"));
    T->SetFont(F);
    T->SetColorAndOpacity(FSlateColor(Color));
    T->SetJustification(ETextJustify::Center);
    T->SetAutoWrapText(true);
    return T;
}

void UPTStreamerZoneWidget::SetTextIfChanged(UTextBlock* T, const FText& Text)
{
    if (T && !T->GetText().EqualTo(Text)) T->SetText(Text);
}

void UPTStreamerZoneWidget::BuildTree()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 16.f));
    Card->SetBrushColor(SZW_Card);
    Card->SetPadding(FMargin(16.f, 12.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Card))
    {
        S->SetHorizontalAlignment(HAlign_Right);
        S->SetVerticalAlignment(VAlign_Top);
        S->SetPadding(FMargin(0.f, ZoneMargin, ZoneMargin, 0.f));
    }
    USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
    Size->SetWidthOverride(ZoneSize.X);
    Size->SetHeightOverride(ZoneSize.Y);
    Card->SetContent(Size);

    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    Size->SetContent(Col);

    TagText = MakeText(11, SZW_Muted, false);
    TagText->SetText(PTText::Get(TEXT("STREAMER_ZONE_TAG")));
    Col->AddChildToVerticalBox(TagText);

    TitleText = MakeText(17, SZW_Ink, true);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(TitleText)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 6.f));

    WordText = MakeText(34, SZW_Gold, true);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(WordText))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetVerticalAlignment(VAlign_Center);
    }

    ChoiceBox = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ChoiceBox)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    for (int32 i = 0; i < 3; ++i)
    {
        UButton* B = WidgetTree->ConstructWidget<UButton>();
        FButtonStyle Style = B->GetStyle();
        Style.Normal  = FSlateRoundedBoxBrush(SZW_Button, 10.f);
        Style.Hovered = FSlateRoundedBoxBrush(SZW_Button * 1.35f, 10.f);
        Style.Pressed = FSlateRoundedBoxBrush(SZW_Button * 0.8f, 10.f);
        Style.NormalPadding  = FMargin(10.f, 5.f);
        Style.PressedPadding = FMargin(10.f, 5.f);
        B->SetStyle(Style);
        UTextBlock* T = MakeText(20, SZW_Ink, true);
        B->AddChild(T);
        if (UVerticalBoxSlot* S = ChoiceBox->AddChildToVerticalBox(B)) S->SetPadding(FMargin(0.f, 3.f));
        ChoiceButtons.Add(B);
        ChoiceTexts.Add(T);
    }
    ChoiceButtons[0]->OnClicked.AddDynamic(this, &UPTStreamerZoneWidget::OnChoice0);
    ChoiceButtons[1]->OnClicked.AddDynamic(this, &UPTStreamerZoneWidget::OnChoice1);
    ChoiceButtons[2]->OnClicked.AddDynamic(this, &UPTStreamerZoneWidget::OnChoice2);

    TypingText = MakeText(15, SZW_Ink, false);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(TypingText)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

    HideZone();
}

void UPTStreamerZoneWidget::ShowPlaceholder()
{
    if (!Card) return;
    Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Card->SetBrushColor(SZW_Placeholder);
    SetTextIfChanged(TitleText, PTText::Get(TEXT("STREAMER_ZONE_CAM")));
    SetTextIfChanged(WordText, PTText::Get(TEXT("STREAMER_ZONE_CAM_HINT")));
    WordText->SetColorAndOpacity(FSlateColor(SZW_Muted));
    { FSlateFontInfo F = WordText->GetFont(); if (F.Size != 15) { F.Size = 15; WordText->SetFont(F); } }
    WordText->SetVisibility(ESlateVisibility::HitTestInvisible);
    ChoiceBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTStreamerZoneWidget::ShowChoices(const TArray<FString>& Choices, int32 SecondsLeft)
{
    if (!Card) return;
    Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Card->SetBrushColor(SZW_Card);
    FFormatOrderedArguments Args;
    Args.Add(FText::AsNumber(SecondsLeft));
    SetTextIfChanged(TitleText, PTText::Format(TEXT("STREAMER_ZONE_CHOOSE"), Args));
    WordText->SetVisibility(ESlateVisibility::Collapsed);
    ChoiceBox->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    for (int32 i = 0; i < ChoiceButtons.Num(); ++i)
    {
        const bool bHas = Choices.IsValidIndex(i) && !Choices[i].IsEmpty();
        ChoiceButtons[i]->SetVisibility(bHas ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        if (bHas) SetTextIfChanged(ChoiceTexts[i], FText::FromString(Choices[i].ToUpper()));
    }
}

void UPTStreamerZoneWidget::ShowWord(const FText& Title, const FString& Word)
{
    if (!Card) return;
    Card->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Card->SetBrushColor(SZW_Card);
    SetTextIfChanged(TitleText, Title);
    SetTextIfChanged(WordText, FText::FromString(Word.ToUpper()));
    WordText->SetColorAndOpacity(FSlateColor(SZW_Gold));
    { FSlateFontInfo F = WordText->GetFont(); if (F.Size != 34) { F.Size = 34; WordText->SetFont(F); } }
    WordText->SetVisibility(ESlateVisibility::HitTestInvisible);
    ChoiceBox->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTStreamerZoneWidget::HideZone()
{
    if (Card && Card->GetVisibility() != ESlateVisibility::Collapsed) Card->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTStreamerZoneWidget::SetTyping(const FString& Text)
{
    if (!TypingText) return;
    TypingText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (!Text.IsEmpty())
    {
        FFormatOrderedArguments Args;
        Args.Add(FText::FromString(Text));
        SetTextIfChanged(TypingText, PTText::Format(TEXT("STREAMER_ZONE_TYPING"), Args));
    }
}

void UPTStreamerZoneWidget::OnChoice0() { OnChoose.ExecuteIfBound(0); }
void UPTStreamerZoneWidget::OnChoice1() { OnChoose.ExecuteIfBound(1); }
void UPTStreamerZoneWidget::OnChoice2() { OnChoose.ExecuteIfBound(2); }
