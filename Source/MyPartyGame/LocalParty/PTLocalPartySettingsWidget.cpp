#include "PTLocalPartySettingsWidget.h"
#include "PTLocalPartySubsystem.h"
#include "PTSculptGameMode.h"
#include "PTSculptGameState.h"
#include "PTSculptPlayerController.h"
#include "../PTGameInstance.h"
#include "../PTTextTable.h"
#include "../UI/PTWordPackWidget.h"
#include "../UI/PTWorkshopBrowserWidget.h"
#include "PTQRCode.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "TimerManager.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/EditableTextBox.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/World.h"

namespace
{
    const FLinearColor LPS_Ink(1.f, 0.97f, 0.92f, 1.f);
    const FLinearColor LPS_Muted(0.72f, 0.68f, 0.84f, 1.f);
    const FLinearColor LPS_Accent(1.f, 0.72f, 0.3f, 1.f);
    const FLinearColor LPS_PanelCol(0.012f, 0.008f, 0.03f, 0.9f);
    const FLinearColor LPS_ButtonFill(0.12f, 0.09f, 0.2f, 1.f);
    const FLinearColor LPS_ButtonHover(0.22f, 0.16f, 0.36f, 1.f);

    bool IsAudience(const UUserWidget* W)
    {
        const UPTGameInstance* GI = W ? W->GetGameInstance<UPTGameInstance>() : nullptr;
        return GI && GI->bLocalPartyOnline;
    }
}

TSharedRef<SWidget> UPTLocalPartySettingsWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

UTextBlock* UPTLocalPartySettingsWidget::MakeText(int32 Size, const FLinearColor& Color, bool bBold)
{
    UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo F = T->GetFont();
    F.Size = Size;
    F.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
    T->SetFont(F);
    T->SetColorAndOpacity(FSlateColor(Color));
    return T;
}

UButton* UPTLocalPartySettingsWidget::MakeButton(const FText& Label, FName Name, int32 FontSize, UTextBlock** OutText)
{
    UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    FButtonStyle Style = B->GetStyle();
    Style.Normal  = FSlateRoundedBoxBrush(LPS_ButtonFill, 10.f);
    Style.Hovered = FSlateRoundedBoxBrush(LPS_ButtonHover, 10.f);
    Style.Pressed = FSlateRoundedBoxBrush(LPS_Accent, 10.f);
    Style.Disabled = FSlateRoundedBoxBrush(FLinearColor(0.08f, 0.07f, 0.1f, 1.f), 10.f);
    Style.NormalPadding = FMargin(14.f, 8.f);
    Style.PressedPadding = FMargin(14.f, 8.f);
    B->SetStyle(Style);
    UTextBlock* T = MakeText(FontSize, LPS_Ink, true);
    T->SetText(Label);
    T->SetJustification(ETextJustify::Center);
    B->AddChild(T);
    if (OutText) *OutText = T;
    return B;
}

USlider* UPTLocalPartySettingsWidget::AddSliderRow(UVerticalBox* Box, UTextBlock*& OutLabel, float Min, float Max, float Step, UTextBlock*& OutValue)
{
    UHorizontalBox* Head = WidgetTree->ConstructWidget<UHorizontalBox>();
    OutLabel = MakeText(17, LPS_Ink, false);
    if (UHorizontalBoxSlot* S = Head->AddChildToHorizontalBox(OutLabel)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    OutValue = MakeText(17, LPS_Accent, true);
    Head->AddChildToHorizontalBox(OutValue);
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Head)) S->SetPadding(FMargin(0.f, 8.f, 0.f, 2.f));

    USlider* Slider = WidgetTree->ConstructWidget<USlider>();
    Slider->SetMinValue(Min);
    Slider->SetMaxValue(Max);
    Slider->SetStepSize(Step);
    Slider->SetSliderBarColor(LPS_Muted);
    Slider->SetSliderHandleColor(LPS_Accent);
    Box->AddChildToVerticalBox(Slider);
    return Slider;
}

void UPTLocalPartySettingsWidget::BuildTree()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 22.f));
    Card->SetBrushColor(LPS_PanelCol);
    Card->SetPadding(FMargin(24.f, 20.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Card))
    {
        // A la derecha: el centro es del panel "unite desde tu celular", la izquierda del marcador.
        S->SetHorizontalAlignment(HAlign_Right);
        S->SetVerticalAlignment(VAlign_Center);
        S->SetPadding(FMargin(0.f, 0.f, 36.f, 0.f));
    }
    USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
    Width->SetWidthOverride(340.f);
    Card->SetContent(Width);
    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    Width->SetContent(Col);

    UTextBlock* Title = MakeText(22, LPS_Accent, true);
    Title->SetText(PTText::Get(TEXT("LP_SETTINGS_TITLE")));
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Title)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 4.f));
    if (IsAudience(this)) BuildStreamerSection(Col);
    if (IsAudience(this)) BuildChatSection(Col);

    TimeSlider   = AddSliderRow(Col, TimeLabel,   30.f, 300.f, 15.f, TimeValue);
    RoundsSlider = AddSliderRow(Col, RoundsLabel, 1.f,  IsAudience(this) ? 20.f : 10.f, 1.f, RoundsValue);
    RevealSlider = AddSliderRow(Col, RevealLabel, 0.f,  0.9f, 0.1f, RevealValue);
    TimeLabel->SetText(PTText::Get(TEXT("LP_SET_TIME")));
    RoundsLabel->SetText(PTText::Get(IsAudience(this) ? TEXT("LP_SET_WORDS_COUNT") : TEXT("LP_SET_ROUNDS")));
    RevealLabel->SetText(PTText::Get(TEXT("LP_SET_REVEAL")));
    TimeSlider->OnValueChanged.AddDynamic(this, &UPTLocalPartySettingsWidget::OnTimeChanged);
    RoundsSlider->OnValueChanged.AddDynamic(this, &UPTLocalPartySettingsWidget::OnRoundsChanged);
    RevealSlider->OnValueChanged.AddDynamic(this, &UPTLocalPartySettingsWidget::OnRevealChanged);

    // Banco de palabras.
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        UVerticalBox* Txt = WidgetTree->ConstructWidget<UVerticalBox>();
        UTextBlock* L = MakeText(17, LPS_Ink, false);
        L->SetText(PTText::Get(TEXT("LP_SET_PACK")));
        Txt->AddChildToVerticalBox(L);
        WordsValue = MakeText(15, LPS_Accent, true);
        Txt->AddChildToVerticalBox(WordsValue);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Txt))
        {
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            S->SetVerticalAlignment(VAlign_Center);
        }
        UButton* Change = MakeButton(PTText::Get(TEXT("LP_SET_CHANGE")), TEXT("WordPackButton"), 15);
        Change->OnClicked.AddDynamic(this, &UPTLocalPartySettingsWidget::OnWordsClicked);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Change)) S->SetVerticalAlignment(VAlign_Center);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 12.f, 0.f, 0.f));
    }

    // Empezar.
    StartButton = MakeButton(PTText::Get(TEXT("LP_START")), TEXT("StartButton"), 20);
    StartButton->OnClicked.AddDynamic(this, &UPTLocalPartySettingsWidget::OnStartClicked);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(StartButton)) S->SetPadding(FMargin(0.f, 18.f, 0.f, 4.f));
    StartHint = MakeText(14, LPS_Muted, false);
    StartHint->SetAutoWrapText(true);
    Col->AddChildToVerticalBox(StartHint);
}

void UPTLocalPartySettingsWidget::BuildStreamerSection(UVerticalBox* Col)
{
    const FLinearColor Red(1.f, 0.36f, 0.36f, 1.f);

    UVerticalBox* Sec = WidgetTree->ConstructWidget<UVerticalBox>();
    StreamerSection = Sec;
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Sec)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 10.f));

    UTextBlock* Head = MakeText(17, LPS_Ink, true);
    Head->SetText(PTText::Get(TEXT("LP_AUD_SECTION")));
    Sec->AddChildToVerticalBox(Head);
    StreamerStatus = MakeText(15, LPS_Muted, false);
    StreamerStatus->SetAutoWrapText(true);
    if (UVerticalBoxSlot* S = Sec->AddChildToVerticalBox(StreamerStatus)) S->SetPadding(FMargin(0.f, 2.f, 0.f, 6.f));

    // ZONA PRIVADA: recuadro rojo fijo y marcado ANTES de mostrar nada, para que el streamer sepa
    // exactamente qué tapar en OBS. El QR del streamer aparece solo acá (nunca en el QR público).
    USizeBox* ZoneSize = WidgetTree->ConstructWidget<USizeBox>();
    ZoneSize->SetWidthOverride(200.f);
    ZoneSize->SetHeightOverride(200.f);
    UBorder* Zone = WidgetTree->ConstructWidget<UBorder>();
    Zone->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.f, 0.f, 0.f, 0.35f), 12.f, Red, 3.f));
    Zone->SetPadding(FMargin(6.f));
    Zone->SetHorizontalAlignment(HAlign_Center);
    Zone->SetVerticalAlignment(VAlign_Center);
    ZoneSize->SetContent(Zone);
    UOverlay* ZoneContent = WidgetTree->ConstructWidget<UOverlay>();
    Zone->SetContent(ZoneContent);
    PrivateZoneHint = MakeText(14, Red, true);
    PrivateZoneHint->SetAutoWrapText(true);
    PrivateZoneHint->SetJustification(ETextJustify::Center);
    PrivateZoneHint->SetText(PTText::Get(TEXT("LP_AUD_ZONE")));
    if (UOverlaySlot* S = ZoneContent->AddChildToOverlay(PrivateZoneHint))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }
    // El QR va en una caja de tamaño FIJO (como el QR público): solo con la imagen, UMG la dibujaba
    // al tamaño por defecto del brush (32 px) y no se podía escanear.
    USizeBox* QrBox = WidgetTree->ConstructWidget<USizeBox>();
    QrBox->SetWidthOverride(176.f);
    QrBox->SetHeightOverride(176.f);
    PrivateQr = WidgetTree->ConstructWidget<UImage>();
    QrBox->SetContent(PrivateQr);
    PrivateQr->SetVisibility(ESlateVisibility::Collapsed);
    if (UOverlaySlot* S = ZoneContent->AddChildToOverlay(QrBox))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }
    PrivateZone = ZoneSize;
    if (UVerticalBoxSlot* S = Sec->AddChildToVerticalBox(ZoneSize)) S->SetHorizontalAlignment(HAlign_Center);

    // Botones: mostrar/ocultar QR · abrir en esta PC · copiar link
    UVerticalBox* Btns = WidgetTree->ConstructWidget<UVerticalBox>();
    StreamerButtons = Btns;
    UButton* Toggle = MakeButton(PTText::Get(TEXT("LP_AUD_SHOW_QR")), TEXT("ShowHostQrButton"), 15, &ToggleQrText);
    Toggle->OnClicked.AddDynamic(this, &UPTLocalPartySettingsWidget::OnToggleHostQr);
    if (UVerticalBoxSlot* S = Btns->AddChildToVerticalBox(Toggle)) S->SetPadding(FMargin(0.f, 8.f, 0.f, 4.f));
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    UButton* Open = MakeButton(PTText::Get(TEXT("LP_AUD_OPEN_PC")), TEXT("OpenHostPcButton"), 14);
    Open->OnClicked.AddDynamic(this, &UPTLocalPartySettingsWidget::OnOpenHostOnPC);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Open))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetPadding(FMargin(0.f, 0.f, 4.f, 0.f));
    }
    UButton* Copy = MakeButton(PTText::Get(TEXT("LP_AUD_COPY")), TEXT("CopyHostLinkButton"), 14, &CopyText);
    Copy->OnClicked.AddDynamic(this, &UPTLocalPartySettingsWidget::OnCopyHostLink);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Copy))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetPadding(FMargin(4.f, 0.f, 0.f, 0.f));
    }
    Btns->AddChildToVerticalBox(Row);
    Sec->AddChildToVerticalBox(Btns);
}

UEditableTextBox* UPTLocalPartySettingsWidget::AddChannelRow(UVerticalBox* Box, const FText& Platform, const FLinearColor& Color, FName Name, UTextBlock*& OutStatus)
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
    Style.BackgroundImageNormal  = FSlateRoundedBoxBrush(LPS_ButtonFill, 8.f);
    Style.BackgroundImageHovered = FSlateRoundedBoxBrush(LPS_ButtonHover, 8.f);
    Style.BackgroundImageFocused = FSlateRoundedBoxBrush(LPS_ButtonHover, 8.f, Color, 2.f);
    Style.Padding = FMargin(10.f, 6.f);
    Style.TextStyle.SetFont(MakeText(15, LPS_Ink, false)->GetFont());
    Style.TextStyle.SetColorAndOpacity(FSlateColor(LPS_Ink));
    Style.ForegroundColor = FSlateColor(LPS_Ink);
    Style.FocusedForegroundColor = FSlateColor(LPS_Ink);
    Input->SetWidgetStyle(Style);
    Input->SetHintText(PTText::Get(TEXT("LP_CHAT_CHANNEL_HINT")));
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Input)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 6.f, 0.f, 0.f));

    OutStatus = MakeText(13, LPS_Muted, false);
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(OutStatus)) S->SetPadding(FMargin(72.f, 2.f, 0.f, 0.f));
    return Input;
}

void UPTLocalPartySettingsWidget::BuildChatSection(UVerticalBox* Col)
{
    UVerticalBox* Sec = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Sec)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f));

    UTextBlock* Head = MakeText(17, LPS_Ink, true);
    Head->SetText(PTText::Get(TEXT("LP_CHAT_SECTION")));
    Sec->AddChildToVerticalBox(Head);
    UTextBlock* Hint = MakeText(13, LPS_Muted, false);
    Hint->SetAutoWrapText(true);
    Hint->SetText(PTText::Get(TEXT("LP_CHAT_HINT")));
    Sec->AddChildToVerticalBox(Hint);

    TwitchInput = AddChannelRow(Sec, FText::FromString(TEXT("Twitch")), FLinearColor(0.66f, 0.47f, 1.f, 1.f), TEXT("TwitchChannelInput"), TwitchStatus);
    KickInput   = AddChannelRow(Sec, FText::FromString(TEXT("Kick")),   FLinearColor(0.33f, 0.98f, 0.36f, 1.f), TEXT("KickChannelInput"), KickStatus);
    TwitchInput->OnTextCommitted.AddDynamic(this, &UPTLocalPartySettingsWidget::OnTwitchCommitted);
    KickInput->OnTextCommitted.AddDynamic(this, &UPTLocalPartySettingsWidget::OnKickCommitted);

    ChatPlayersText = MakeText(14, LPS_Accent, true);
    if (UVerticalBoxSlot* S = Sec->AddChildToVerticalBox(ChatPlayersText)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

    if (const UPTLocalPartySubsystem* LP = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLocalPartySubsystem>() : nullptr)
    {
        TwitchInput->SetText(FText::FromString(LP->GetStreamChannel(EPTChatPlatform::Twitch)));
        KickInput->SetText(FText::FromString(LP->GetStreamChannel(EPTChatPlatform::Kick)));
    }
}

void UPTLocalPartySettingsWidget::UpdateChatSection()
{
    const UPTLocalPartySubsystem* LP = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLocalPartySubsystem>() : nullptr;
    if (!LP || !TwitchStatus) return;
    auto SetStatus = [LP](UTextBlock* T, EPTChatPlatform P)
    {
        if (!T) return;
        const bool bHasChannel = !LP->GetStreamChannel(P).IsEmpty();
        const FPTStreamChat::EStatus St = LP->GetStreamChatStatus(P);
        const TCHAR* Key = TEXT("LP_CHAT_ST_OFF");
        FLinearColor C = LPS_Muted;
        if (bHasChannel && St == FPTStreamChat::EStatus::Connected) { Key = TEXT("LP_CHAT_ST_OK"); C = FLinearColor(0.36f, 0.88f, 0.54f, 1.f); }
        else if (bHasChannel && St == FPTStreamChat::EStatus::Error)
        {
            Key = LP->HasStreamChatGivenUp(P) ? TEXT("LP_CHAT_ST_NOTFOUND") : TEXT("LP_CHAT_ST_RETRY");
            C = FLinearColor(1.f, 0.45f, 0.4f, 1.f);
        }
        else if (bHasChannel) Key = TEXT("LP_CHAT_ST_CONNECTING");
        // Solo se reescribe si cambió (la traducción automática de PTText la pisa si se escribe cada vez).
        const FText Want = PTText::Get(Key);
        if (!T->GetText().EqualTo(Want)) T->SetText(Want);
        T->SetColorAndOpacity(FSlateColor(C));
    };
    SetStatus(TwitchStatus, EPTChatPlatform::Twitch);
    SetStatus(KickStatus, EPTChatPlatform::Kick);

    // Si el canal cambió por otro lado (consola), mostrarlo; nunca mientras el streamer está escribiendo.
    auto SyncInput = [LP](UEditableTextBox* In, EPTChatPlatform P)
    {
        const FString Saved = LP->GetStreamChannel(P);
        if (In && !In->HasKeyboardFocus() && In->GetText().ToString() != Saved) In->SetText(FText::FromString(Saved));
    };
    SyncInput(TwitchInput, EPTChatPlatform::Twitch);
    SyncInput(KickInput, EPTChatPlatform::Kick);

    if (ChatPlayersText)
    {
        const int32 N = LP->GetChatPlayerCount();
        ChatPlayersText->SetVisibility(N > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (N > 0)
        {
            const FText Want = PTText::Format(TEXT("LP_CHAT_PLAYERS"), FFormatOrderedArguments{ FFormatArgumentValue(N) });
            if (!ChatPlayersText->GetText().EqualTo(Want)) ChatPlayersText->SetText(Want);
        }
    }
}

void UPTLocalPartySettingsWidget::OnTwitchCommitted(const FText& Text, ETextCommit::Type Method)
{
    if (UPTLocalPartySubsystem* LP = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLocalPartySubsystem>() : nullptr)
    {
        LP->SetStreamChannel(EPTChatPlatform::Twitch, Text.ToString());
        if (TwitchInput) TwitchInput->SetText(FText::FromString(LP->GetStreamChannel(EPTChatPlatform::Twitch)));
    }
}

void UPTLocalPartySettingsWidget::OnKickCommitted(const FText& Text, ETextCommit::Type Method)
{
    if (UPTLocalPartySubsystem* LP = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLocalPartySubsystem>() : nullptr)
    {
        LP->SetStreamChannel(EPTChatPlatform::Kick, Text.ToString());
        if (KickInput) KickInput->SetText(FText::FromString(LP->GetStreamChannel(EPTChatPlatform::Kick)));
    }
}

void UPTLocalPartySettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshValues();
    UpdateState();
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(StateTimer, this, &UPTLocalPartySettingsWidget::UpdateState, 0.2f, true);
}

void UPTLocalPartySettingsWidget::NativeDestruct()
{
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(StateTimer);
    Super::NativeDestruct();
}

bool UPTLocalPartySettingsWidget::IsRevealHeld() const
{
    const APlayerController* PC = GetOwningPlayer();
    return PC && (PC->IsInputKeyDown(EKeys::Gamepad_Special_Left) || PC->IsInputKeyDown(EKeys::I));
}

void UPTLocalPartySettingsWidget::UpdateStreamerSection()
{
    if (!StreamerSection) return;
    const UPTLocalPartySubsystem* LP = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLocalPartySubsystem>() : nullptr;
    const bool bConnected = LP && LP->IsHostConnected();
    if (bConnected) bHostQrShown = false; // ya entró: el QR privado no vuelve a mostrarse

    if (StreamerStatus)
    {
        StreamerStatus->SetText(PTText::Get(bConnected ? TEXT("LP_AUD_HOST_OK") : TEXT("LP_AUD_HOST_STEPS")));
        StreamerStatus->SetColorAndOpacity(FSlateColor(bConnected ? FLinearColor(0.36f, 0.88f, 0.54f, 1.f) : LPS_Muted));
    }
    auto Show = [](UWidget* W, bool bOn) { if (W) W->SetVisibility(bOn ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); };
    Show(PrivateZone, !bConnected);
    if (StreamerButtons) StreamerButtons->SetVisibility(bConnected ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);

    // El QR solo se dibuja si el streamer lo pidió (botón o mantener View / I).
    const bool bShowQr = !bConnected && (bHostQrShown || IsRevealHeld());
    const FString Url = (bShowQr && LP) ? LP->GetHostJoinUrl() : FString();
    if (Url != PrivateQrUrl)
    {
        PrivateQrUrl = Url;
        PrivateQrTexture = Url.IsEmpty() ? nullptr : PTQR::MakeTexture(Url, 6);
        if (PrivateQr && PrivateQrTexture) PrivateQr->SetBrushFromTexture(PrivateQrTexture, false);
    }
    Show(PrivateQr, bShowQr && PrivateQrTexture);
    Show(PrivateZoneHint, !(bShowQr && PrivateQrTexture));
    if (ToggleQrText) ToggleQrText->SetText(PTText::Get(bHostQrShown ? TEXT("LP_AUD_HIDE_QR") : TEXT("LP_AUD_SHOW_QR")));
    if (CopyText) CopyText->SetText(PTText::Get(FPlatformTime::Seconds() < CopiedUntil ? TEXT("LP_AUD_COPIED") : TEXT("LP_AUD_COPY")));
}

void UPTLocalPartySettingsWidget::OnToggleHostQr()
{
    bHostQrShown = !bHostQrShown;
    UpdateStreamerSection();
}

void UPTLocalPartySettingsWidget::OnOpenHostOnPC()
{
    const UPTLocalPartySubsystem* LP = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLocalPartySubsystem>() : nullptr;
    const FString Url = LP ? LP->GetHostJoinUrl() : FString();
    if (!Url.IsEmpty()) FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

void UPTLocalPartySettingsWidget::OnCopyHostLink()
{
    const UPTLocalPartySubsystem* LP = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLocalPartySubsystem>() : nullptr;
    const FString Url = LP ? LP->GetHostJoinUrl() : FString();
    if (Url.IsEmpty()) return;
    FPlatformApplicationMisc::ClipboardCopy(*Url);
    CopiedUntil = FPlatformTime::Seconds() + 2.0;
    UpdateStreamerSection();
}

void UPTLocalPartySettingsWidget::RefreshValues()
{
    const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    if (!GI) return;
    const FPTMatchSettings& M = GI->PendingMatchSettings;
    if (TimeSlider)   TimeSlider->SetValue(M.TurnDuration);
    if (RoundsSlider) RoundsSlider->SetValue((float)M.NumRounds);
    if (RevealSlider) RevealSlider->SetValue(M.RevealFraction);
    if (TimeValue)    TimeValue->SetText(FText::FromString(FString::Printf(TEXT("%d s"), FMath::RoundToInt(M.TurnDuration))));
    if (RoundsValue)  RoundsValue->SetText(FText::AsNumber(M.NumRounds));
    if (RevealValue)  RevealValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(M.RevealFraction * 100.f))));
    if (WordsValue && GI->SelectedWordPackTitle != ShownPackTitle)
    {
        ShownPackTitle = GI->SelectedWordPackTitle;
        WordsValue->SetText(ShownPackTitle.IsEmpty() ? PTText::Get(TEXT("GS_DEFAULT")) : FText::FromString(ShownPackTitle));
    }
}

void UPTLocalPartySettingsWidget::UpdateState()
{
    // Solo mientras se espera a los jugadores (y sin la pausa encima).
    const APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
    const APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(GetOwningPlayer());
    // Mientras está abierto el banco de palabras o el Workshop, el panel se corre (vuelve solo al cerrarlos).
    bool bOtherPanel = WordPack && WordPack->IsInViewport() && WordPack->IsVisible();
    if (!bOtherPanel && GetWorld())
    {
        TArray<UUserWidget*> Browsers;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(GetWorld(), Browsers, UPTWorkshopBrowserWidget::StaticClass(), true);
        for (UUserWidget* B : Browsers) if (B && B->IsVisible()) { bOtherPanel = true; break; }
    }
    const bool bShow = G && G->TurnPhase == EPTTurnPhase::WaitingForPlayers && !(PC && PC->IsEscapeMenuOpen()) && !bOtherPanel;
    SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
    if (!bShow) { bHostQrShown = false; return; } // al irse el panel, el QR privado se oculta
    UpdateStreamerSection();
    UpdateChatSection();

    // El banco de palabras puede haber cambiado en su propio panel. Se escribe SOLO si cambió: el
    // subsistema de localización traduce el título en pantalla y reescribirlo lo hacía parpadear.
    if (const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
        if (WordsValue && GI->SelectedWordPackTitle != ShownPackTitle)
        {
            ShownPackTitle = GI->SelectedWordPackTitle;
            WordsValue->SetText(ShownPackTitle.IsEmpty() ? PTText::Get(TEXT("GS_DEFAULT")) : FText::FromString(ShownPackTitle));
        }

    // ¿Se puede empezar?
    const APTSculptGameMode* GM = GetWorld()->GetAuthGameMode<APTSculptGameMode>();
    const UPTLocalPartySubsystem* LP = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLocalPartySubsystem>() : nullptr;
    const int32 Have = LP ? LP->GetGuesserCount() : 0;
    const int32 Need = GM ? GM->LocalParty_GetMinPlayers() : 2;
    if (StartButton) StartButton->SetIsEnabled(Have >= Need);
    if (StartHint)
    {
        if (Have < Need)
            StartHint->SetText(PTText::Format(TEXT("LP_NEED_MORE"), FFormatOrderedArguments{ FFormatArgumentValue(Need - Have) }));
        else if (IsAudience(this) && LP && !LP->IsHostConnected())
            StartHint->SetText(PTText::Get(TEXT("LP_AUD_HOST_MISSING")));
        else
            StartHint->SetText(FText::GetEmpty());
    }
}

void UPTLocalPartySettingsWidget::OnTimeChanged(float V)
{
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
        GI->PendingMatchSettings.TurnDuration = FMath::Clamp(FMath::RoundToFloat(V / 15.f) * 15.f, 30.f, 300.f);
    RefreshValues();
}

void UPTLocalPartySettingsWidget::OnRoundsChanged(float V)
{
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
        GI->PendingMatchSettings.NumRounds = FMath::Clamp(FMath::RoundToInt(V), 1, IsAudience(this) ? 20 : 10);
    RefreshValues();
}

void UPTLocalPartySettingsWidget::OnRevealChanged(float V)
{
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
        GI->PendingMatchSettings.RevealFraction = FMath::Clamp(FMath::RoundToFloat(V * 10.f) / 10.f, 0.f, 0.9f);
    RefreshValues();
}

void UPTLocalPartySettingsWidget::OnWordsClicked()
{
    if (!WordPack)
    {
        UClass* Cls = WordPackClass.TryLoadClass<UPTWordPackWidget>();
        if (!Cls) return;
        WordPack = CreateWidget<UPTWordPackWidget>(GetOwningPlayer(), Cls);
        if (!WordPack) return;
        WordPack->AddToViewport(60);
    }
    WordPack->ShowPanel();
}

void UPTLocalPartySettingsWidget::OnStartClicked()
{
    if (APTSculptGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APTSculptGameMode>() : nullptr)
        GM->LocalParty_RequestStart();
}
