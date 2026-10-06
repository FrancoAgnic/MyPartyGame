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
    Zone->SetPadding(FMargin(10.f));
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
    PrivateQr = WidgetTree->ConstructWidget<UImage>();
    PrivateQr->SetDesiredSizeOverride(FVector2D(176.f, 176.f)); // llena el recuadro (la textura es chica)
    PrivateQr->SetVisibility(ESlateVisibility::Collapsed);
    if (UOverlaySlot* S = ZoneContent->AddChildToOverlay(PrivateQr))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
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
    if (WordsValue)   WordsValue->SetText(GI->SelectedWordPackTitle.IsEmpty() ? PTText::Get(TEXT("GS_DEFAULT"))
                                                                              : FText::FromString(GI->SelectedWordPackTitle));
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

    // Solo los TEXTOS (los sliders no: pisarían al jugador mientras arrastra). El banco de palabras
    // puede haber cambiado en su propio panel.
    if (const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
        if (WordsValue) WordsValue->SetText(GI->SelectedWordPackTitle.IsEmpty() ? PTText::Get(TEXT("GS_DEFAULT"))
                                                                               : FText::FromString(GI->SelectedWordPackTitle));

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
