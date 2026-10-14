#include "PTStreamChatSetupWidget.h"
#include "../LocalParty/PTStreamChatSubsystem.h"
#include "../PTTextTable.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
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
    const FLinearColor SCW_Ink(1.f, 0.97f, 0.92f, 1.f);
    const FLinearColor SCW_Muted(1.f, 0.97f, 0.92f, 0.6f);
    const FLinearColor SCW_Panel(0.03f, 0.02f, 0.06f, 0.97f);
    const FLinearColor SCW_Field(0.16f, 0.12f, 0.24f, 1.f);
    const FLinearColor SCW_FieldHover(0.22f, 0.17f, 0.32f, 1.f);
    const FLinearColor SCW_On(0.18f, 0.62f, 0.32f, 1.f);
    const FLinearColor SCW_Off(0.35f, 0.2f, 0.45f, 1.f);
    const FLinearColor SCW_Ok(0.36f, 0.88f, 0.54f, 1.f);
    const FLinearColor SCW_Bad(1.f, 0.45f, 0.4f, 1.f);
}

UPTStreamChatSubsystem* UPTStreamChatSetupWidget::Sub() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTStreamChatSubsystem>() : nullptr;
}

TSharedRef<SWidget> UPTStreamChatSetupWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

void UPTStreamChatSetupWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(RefreshTimer, FTimerDelegate::CreateUObject(this, &UPTStreamChatSetupWidget::Refresh),
                                      0.25f, true);
    Refresh();
    PlayPopIn();
}

void UPTStreamChatSetupWidget::NativeDestruct()
{
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(RefreshTimer);
    Super::NativeDestruct();
}

UTextBlock* UPTStreamChatSetupWidget::MakeText(int32 Size, const FLinearColor& Color, bool bBold)
{
    UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo F = T->GetFont();
    F.Size = Size;
    if (bBold) F.TypefaceFontName = FName(TEXT("Bold"));
    T->SetFont(F);
    T->SetColorAndOpacity(FSlateColor(Color));
    return T;
}

UButton* UPTStreamChatSetupWidget::MakeButton(const FText& Label, FName Name, UTextBlock** OutText)
{
    UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    FButtonStyle Style = B->GetStyle();
    Style.Normal  = FSlateRoundedBoxBrush(FLinearColor::White, 12.f);
    Style.Hovered = FSlateRoundedBoxBrush(FLinearColor(1.3f, 1.3f, 1.3f, 1.f), 12.f);
    Style.Pressed = FSlateRoundedBoxBrush(FLinearColor(0.8f, 0.8f, 0.8f, 1.f), 12.f);
    Style.NormalPadding  = FMargin(22.f, 10.f);
    Style.PressedPadding = FMargin(22.f, 10.f);
    B->SetStyle(Style);
    UTextBlock* T = MakeText(18, SCW_Ink, true);
    T->SetJustification(ETextJustify::Center);
    T->SetText(Label);
    B->AddChild(T);
    if (OutText) *OutText = T;
    return B;
}

UEditableTextBox* UPTStreamChatSetupWidget::AddChannelRow(UVerticalBox* Box, const FText& Platform, const FLinearColor& Color,
                                                          FName Name, UTextBlock*& OutStatus)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    USizeBox* LabelBox = WidgetTree->ConstructWidget<USizeBox>();
    LabelBox->SetWidthOverride(72.f);
    UTextBlock* Label = MakeText(15, Color, true);
    Label->SetText(Platform);
    LabelBox->SetContent(Label);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(LabelBox)) S->SetVerticalAlignment(VAlign_Center);

    UEditableTextBox* Input = WidgetTree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), Name);
    FEditableTextBoxStyle Style = Input->GetWidgetStyle();
    Style.BackgroundImageNormal  = FSlateRoundedBoxBrush(SCW_Field, 8.f);
    Style.BackgroundImageHovered = FSlateRoundedBoxBrush(SCW_FieldHover, 8.f);
    Style.BackgroundImageFocused = FSlateRoundedBoxBrush(SCW_FieldHover, 8.f, Color, 2.f);
    Style.Padding = FMargin(10.f, 6.f);
    Style.TextStyle.SetFont(MakeText(15, SCW_Ink, false)->GetFont());
    Style.TextStyle.SetColorAndOpacity(FSlateColor(SCW_Ink));
    Style.ForegroundColor = FSlateColor(SCW_Ink);
    Style.FocusedForegroundColor = FSlateColor(SCW_Ink);
    Input->SetWidgetStyle(Style);
    Input->SetHintText(PTText::Get(TEXT("LP_CHAT_CHANNEL_HINT")));
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Input)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));

    OutStatus = MakeText(13, SCW_Muted, false);
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(OutStatus)) S->SetPadding(FMargin(72.f, 2.f, 0.f, 0.f));
    return Input;
}

void UPTStreamChatSetupWidget::BuildTree()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // Fondo oscuro que bloquea lo de atrás.
    UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
    Dim->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 0.f));
    Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Dim)) { S->SetHorizontalAlignment(HAlign_Fill); S->SetVerticalAlignment(VAlign_Fill); }

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 24.f));
    Card->SetBrushColor(SCW_Panel);
    Card->SetPadding(FMargin(36.f, 28.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Card)) { S->SetHorizontalAlignment(HAlign_Center); S->SetVerticalAlignment(VAlign_Center); }
    USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
    Width->SetWidthOverride(540.f);
    Card->SetContent(Width);
    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    Width->SetContent(Col);

    UTextBlock* Title = MakeText(26, SCW_Ink, true);
    Title->SetText(PTText::Get(TEXT("SC_TITLE")));
    Col->AddChildToVerticalBox(Title);
    UTextBlock* Hint = MakeText(15, SCW_Muted, false);
    Hint->SetAutoWrapText(true);
    Hint->SetText(PTText::Get(TEXT("SC_HINT")));
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Hint)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 14.f));

    ToggleButton = MakeButton(FText::GetEmpty(), TEXT("StreamChatToggle"), &ToggleText);
    ToggleButton->OnClicked.AddDynamic(this, &UPTStreamChatSetupWidget::OnToggleClicked);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ToggleButton)) S->SetHorizontalAlignment(HAlign_Left);

    TwitchInput = AddChannelRow(Col, FText::FromString(TEXT("Twitch")), FLinearColor(0.66f, 0.47f, 1.f, 1.f),
                                TEXT("StreamTwitchInput"), TwitchStatus);
    KickInput   = AddChannelRow(Col, FText::FromString(TEXT("Kick")), FLinearColor(0.33f, 0.98f, 0.36f, 1.f),
                                TEXT("StreamKickInput"), KickStatus);
    TwitchInput->OnTextCommitted.AddDynamic(this, &UPTStreamChatSetupWidget::OnTwitchCommitted);
    KickInput->OnTextCommitted.AddDynamic(this, &UPTStreamChatSetupWidget::OnKickCommitted);
    if (const UPTStreamChatSubsystem* SC = Sub())
    {
        TwitchInput->SetText(FText::FromString(SC->GetChannel(EPTChatPlatform::Twitch)));
        KickInput->SetText(FText::FromString(SC->GetChannel(EPTChatPlatform::Kick)));
    }

    ViewersText = MakeText(14, FLinearColor(1.f, 0.85f, 0.4f, 1.f), true);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ViewersText)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

    UTextBlock* Keys = MakeText(13, SCW_Muted, false);
    Keys->SetAutoWrapText(true);
    Keys->SetText(PTText::Get(TEXT("SC_KEYS")));
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Keys)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));

    UTextBlock* DoneText = nullptr;
    UButton* Done = MakeButton(PTText::Get(TEXT("MENU_CONFIRM")), TEXT("StreamChatDone"), &DoneText);
    Done->SetBackgroundColor(SCW_Off);
    Done->OnClicked.AddDynamic(this, &UPTStreamChatSetupWidget::OnDoneClicked);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Done))
    {
        S->SetHorizontalAlignment(HAlign_Right);
        S->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
    }
}

void UPTStreamChatSetupWidget::SetStatus(UTextBlock* Status, EPTChatPlatform Platform) const
{
    const UPTStreamChatSubsystem* SC = Sub();
    if (!Status || !SC) return;
    const bool bHasChannel = !SC->GetChannel(Platform).IsEmpty();
    const FPTStreamChat::EStatus St = SC->GetStatus(Platform);
    const TCHAR* Key = TEXT("LP_CHAT_ST_OFF");
    FLinearColor C = SCW_Muted;
    if (bHasChannel && SC->IsActive())
    {
        if (St == FPTStreamChat::EStatus::Connected)  { Key = TEXT("LP_CHAT_ST_OK"); C = SCW_Ok; }
        else if (St == FPTStreamChat::EStatus::Error) { Key = TEXT("LP_CHAT_ST_RETRY"); C = SCW_Bad; }
        else                                          { Key = TEXT("LP_CHAT_ST_CONNECTING"); }
    }
    const FText Want = PTText::Get(Key);
    if (!Status->GetText().EqualTo(Want)) Status->SetText(Want);
    Status->SetColorAndOpacity(FSlateColor(C));
}

void UPTStreamChatSetupWidget::Refresh()
{
    UPTStreamChatSubsystem* SC = Sub();
    if (!SC) return;
    const bool bOn = SC->IsEnabled();
    if (ToggleButton) ToggleButton->SetBackgroundColor(bOn ? SCW_On : SCW_Off);
    if (ToggleText)
    {
        const FText Want = PTText::Get(bOn ? TEXT("GS_STREAMCHAT_ON") : TEXT("GS_STREAMCHAT_OFF"));
        if (!ToggleText->GetText().EqualTo(Want)) ToggleText->SetText(Want);
    }
    SetStatus(TwitchStatus, EPTChatPlatform::Twitch);
    SetStatus(KickStatus, EPTChatPlatform::Kick);
    if (ViewersText)
    {
        const int32 N = SC->GetViewerCount();
        ViewersText->SetVisibility(N > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (N > 0)
        {
            FFormatOrderedArguments Args;
            Args.Add(FText::AsNumber(N));
            const FText Want = PTText::Format(TEXT("LP_CHAT_PLAYERS"), Args);
            if (!ViewersText->GetText().EqualTo(Want)) ViewersText->SetText(Want);
        }
    }
}

void UPTStreamChatSetupWidget::OnToggleClicked()
{
    if (UPTStreamChatSubsystem* SC = Sub()) SC->SetEnabled(!SC->IsEnabled());
    Refresh();
}

void UPTStreamChatSetupWidget::OnDoneClicked()
{
    // Lo escrito sin Enter también cuenta (el commit por perder el foco puede no llegar si se cierra de una).
    if (UPTStreamChatSubsystem* SC = Sub())
    {
        if (TwitchInput) SC->SetChannel(EPTChatPlatform::Twitch, TwitchInput->GetText().ToString());
        if (KickInput)   SC->SetChannel(EPTChatPlatform::Kick, KickInput->GetText().ToString());
    }
    RemoveFromParent();
    OnClosed.Broadcast();
}

void UPTStreamChatSetupWidget::OnTwitchCommitted(const FText& Text, ETextCommit::Type Method)
{
    if (UPTStreamChatSubsystem* SC = Sub())
    {
        SC->SetChannel(EPTChatPlatform::Twitch, Text.ToString());
        if (TwitchInput) TwitchInput->SetText(FText::FromString(SC->GetChannel(EPTChatPlatform::Twitch)));
    }
}

void UPTStreamChatSetupWidget::OnKickCommitted(const FText& Text, ETextCommit::Type Method)
{
    if (UPTStreamChatSubsystem* SC = Sub())
    {
        SC->SetChannel(EPTChatPlatform::Kick, Text.ToString());
        if (KickInput) KickInput->SetText(FText::FromString(SC->GetChannel(EPTChatPlatform::Kick)));
    }
}
