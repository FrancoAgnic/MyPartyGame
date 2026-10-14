#include "PTChatTabsWidget.h"
#include "../PTTextTable.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/TextBlock.h"

namespace
{
    const FLinearColor CTW_Active(0.98f, 0.74f, 0.16f, 0.95f);
    const FLinearColor CTW_Idle(0.08f, 0.05f, 0.14f, 0.75f);
    const FLinearColor CTW_InkDark(0.06f, 0.04f, 0.10f, 1.f);
    const FLinearColor CTW_Ink(1.f, 0.97f, 0.92f, 0.9f);
}

TSharedRef<SWidget> UPTChatTabsWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

UBorder* UPTChatTabsWidget::MakePill(UTextBlock*& OutText)
{
    UBorder* Pill = WidgetTree->ConstructWidget<UBorder>();
    Pill->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 9.f));
    Pill->SetPadding(FMargin(10.f, 3.f));
    OutText = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo F = OutText->GetFont();
    F.Size = 12;
    F.TypefaceFontName = FName(TEXT("Bold"));
    OutText->SetFont(F);
    Pill->SetContent(OutText);
    return Pill;
}

void UPTChatTabsWidget::BuildTree()
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Row;
    GamePill   = MakePill(GameText);
    StreamPill = MakePill(StreamText);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(GamePill))   S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f));
    Row->AddChildToHorizontalBox(StreamPill);
    SetState(false, 0, 0, FText::FromString(TEXT("Stream")));
}

void UPTChatTabsWidget::StylePill(UBorder* Pill, UTextBlock* Text, bool bActive, const FText& Label) const
{
    if (!Pill || !Text) return;
    if (Pill->GetBrushColor() != (bActive ? CTW_Active : CTW_Idle)) Pill->SetBrushColor(bActive ? CTW_Active : CTW_Idle);
    Text->SetColorAndOpacity(FSlateColor(bActive ? CTW_InkDark : CTW_Ink));
    if (!Text->GetText().EqualTo(Label)) Text->SetText(Label);
}

void UPTChatTabsWidget::SetState(bool bStreamActive, int32 UnreadGame, int32 UnreadStream, const FText& StreamLabel)
{
    // "Juego · Shift+1" / "Twitch · Shift+2 (3)": el contador solo en la pestaña que NO estás mirando.
    auto Label = [](const FText& Name, const TCHAR* Keys, int32 Unread)
    {
        FString S = FString::Printf(TEXT("%s · %s"), *Name.ToString(), Keys);
        if (Unread > 0) S += FString::Printf(TEXT("  (%d)"), FMath::Min(Unread, 99));
        return FText::FromString(S);
    };
    StylePill(GamePill, GameText, !bStreamActive,
              Label(PTText::Get(TEXT("STREAM_TAB_GAME")), TEXT("Shift+1"), bStreamActive ? UnreadGame : 0));
    StylePill(StreamPill, StreamText, bStreamActive,
              Label(StreamLabel, TEXT("Shift+2"), bStreamActive ? 0 : UnreadStream));
}
