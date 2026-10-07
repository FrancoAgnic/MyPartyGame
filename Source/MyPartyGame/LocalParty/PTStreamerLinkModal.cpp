#include "PTStreamerLinkModal.h"
#include "PTLocalPartySubsystem.h"
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
#include "HAL/PlatformApplicationMisc.h"

namespace
{
    const FLinearColor SLM_Ink(1.f, 0.97f, 0.92f, 1.f);
    const FLinearColor SLM_Accent(1.f, 0.72f, 0.3f, 1.f);
    const FLinearColor SLM_Red(1.f, 0.36f, 0.36f, 1.f);
    const FLinearColor SLM_ChatFill(0.1f, 0.34f, 0.2f, 1.f);
    const FLinearColor SLM_PrivateFill(0.42f, 0.08f, 0.1f, 1.f);
    const FLinearColor SLM_ButtonFill(0.12f, 0.09f, 0.2f, 1.f);

    UPTLocalPartySubsystem* SLM_GetLP(const UUserWidget* W)
    {
        UGameInstance* GI = W ? W->GetGameInstance() : nullptr;
        return GI ? GI->GetSubsystem<UPTLocalPartySubsystem>() : nullptr;
    }
}

void UPTStreamerLinkModal::Open()
{
    if (IsInViewport()) return;
    PrivateCopiedUntil = ChatCopiedUntil = 0.0;
    ShownState = -1;
    AddToViewport(500); // encima del HUD y de los paneles de la partida (el navegador va en 900+)
}

void UPTStreamerLinkModal::Close()
{
    if (IsInViewport()) RemoveFromParent();
}

TSharedRef<SWidget> UPTStreamerLinkModal::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

UTextBlock* UPTStreamerLinkModal::MakeText(int32 Size, const FLinearColor& Color, bool bBold)
{
    UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo F = T->GetFont();
    F.Size = Size;
    F.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
    T->SetFont(F);
    T->SetColorAndOpacity(FSlateColor(Color));
    T->SetAutoWrapText(true);
    return T;
}

UButton* UPTStreamerLinkModal::MakeButton(const FText& Label, FName Name, const FLinearColor& Fill, UTextBlock** OutText)
{
    UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    FButtonStyle Style = B->GetStyle();
    FLinearColor Hover = Fill * 1.7f;
    Hover.A = 1.f;
    Style.Normal  = FSlateRoundedBoxBrush(Fill, 10.f);
    Style.Hovered = FSlateRoundedBoxBrush(Hover, 10.f, SLM_Accent, 2.f);
    Style.Pressed = FSlateRoundedBoxBrush(SLM_Accent, 10.f);
    Style.NormalPadding = FMargin(16.f, 10.f);
    Style.PressedPadding = FMargin(16.f, 10.f);
    B->SetStyle(Style);
    UTextBlock* T = MakeText(17, SLM_Ink, true);
    T->SetAutoWrapText(false);
    T->SetText(Label);
    T->SetJustification(ETextJustify::Center);
    B->AddChild(T);
    if (OutText) *OutText = T;
    return B;
}

void UPTStreamerLinkModal::BuildTree()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // Fondo oscuro a pantalla completa: bloquea el mouse y tapa (para el joystick) lo de abajo.
    UBorder* Backdrop = WidgetTree->ConstructWidget<UBorder>();
    Backdrop->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.7f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Backdrop))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
    }

    Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 22.f));
    Card->SetBrushColor(FLinearColor(0.02f, 0.014f, 0.045f, 0.98f));
    Card->SetPadding(FMargin(30.f, 24.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Card))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }
    USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
    Width->SetWidthOverride(560.f);
    Card->SetContent(Width);
    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    Width->SetContent(Col);
    auto Add = [Col](UWidget* W, const FMargin& Pad)
    {
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(W)) S->SetPadding(Pad);
    };

    UTextBlock* Title = MakeText(26, SLM_Accent, true);
    Title->SetText(PTText::Get(TEXT("LP_AUD_LINKS_TITLE")));
    Add(Title, FMargin(0.f, 0.f, 0.f, 14.f));

    // ── Link público (el que va al chat). Va PRIMERO: es el que recibe el foco del joystick. ──
    UTextBlock* ChatHint = MakeText(16, SLM_Ink, false);
    ChatHint->SetText(PTText::Get(TEXT("LP_AUD_CHAT_HINT")));
    Add(ChatHint, FMargin(0.f, 0.f, 0.f, 2.f));
    ChatUrlText = MakeText(18, FLinearColor(0.45f, 0.95f, 0.62f, 1.f), true);
    Add(ChatUrlText, FMargin(0.f, 0.f, 0.f, 8.f));
    UButton* Chat = MakeButton(PTText::Get(TEXT("LP_AUD_COPY_CHAT")), TEXT("CopyChatLinkButton"), SLM_ChatFill, &ChatText);
    Chat->OnClicked.AddDynamic(this, &UPTStreamerLinkModal::OnCopyChat);
    Add(Chat, FMargin(0.f, 0.f, 0.f, 20.f));

    // ── Link privado del streamer: aviso en rojo + botón rojo. El link nunca se muestra escrito. ──
    UBorder* Warn = WidgetTree->ConstructWidget<UBorder>();
    Warn->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.3f, 0.02f, 0.03f, 0.6f), 12.f, SLM_Red, 3.f));
    Warn->SetPadding(FMargin(14.f, 12.f));
    UVerticalBox* WarnCol = WidgetTree->ConstructWidget<UVerticalBox>();
    Warn->SetContent(WarnCol);
    UTextBlock* WarnText = MakeText(17, SLM_Red, true);
    WarnText->SetText(PTText::Get(TEXT("LP_AUD_PRIVATE_WARN")));
    if (UVerticalBoxSlot* S = WarnCol->AddChildToVerticalBox(WarnText)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));
    UButton* Private = MakeButton(PTText::Get(TEXT("LP_AUD_COPY_PRIVATE")), TEXT("CopyPrivateLinkButton"), SLM_PrivateFill, &PrivateText);
    Private->OnClicked.AddDynamic(this, &UPTStreamerLinkModal::OnCopyPrivate);
    WarnCol->AddChildToVerticalBox(Private);
    Add(Warn, FMargin(0.f, 0.f, 0.f, 20.f));

    UButton* CloseBtn = MakeButton(PTText::Get(TEXT("SETTINGS_CLOSE")), TEXT("CloseLinkModalButton"), SLM_ButtonFill, nullptr);
    CloseBtn->OnClicked.AddDynamic(this, &UPTStreamerLinkModal::OnCloseClicked);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(CloseBtn)) S->SetHorizontalAlignment(HAlign_Center);
}

void UPTStreamerLinkModal::NativeConstruct()
{
    Super::NativeConstruct();
    SetVisibility(ESlateVisibility::Visible);
    RefreshLabels();
    if (Card) PlayPopInOn(Card);
}

void UPTStreamerLinkModal::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshLabels();
}

void UPTStreamerLinkModal::RefreshLabels()
{
    const double Now = FPlatformTime::Seconds();
    const int32 State = (Now < PrivateCopiedUntil ? 1 : 0) | (Now < ChatCopiedUntil ? 2 : 0);
    if (State != ShownState)
    {
        ShownState = State;
        if (PrivateText) PrivateText->SetText(PTText::Get((State & 1) ? TEXT("LP_AUD_COPIED") : TEXT("LP_AUD_COPY_PRIVATE")));
        if (ChatText)    ChatText->SetText(PTText::Get((State & 2) ? TEXT("LP_AUD_COPIED") : TEXT("LP_AUD_COPY_CHAT")));
    }
    const UPTLocalPartySubsystem* LP = SLM_GetLP(this);
    const FString Url = LP ? LP->GetJoinUrl() : FString();
    if (Url != ShownChatUrl && ChatUrlText)
    {
        ShownChatUrl = Url;
        ChatUrlText->SetText(FText::FromString(Url.IsEmpty() ? FString(TEXT("…")) : Url));
    }
}

void UPTStreamerLinkModal::OnCopyPrivate()
{
    const UPTLocalPartySubsystem* LP = SLM_GetLP(this);
    const FString Url = LP ? LP->GetHostJoinUrl() : FString();
    if (Url.IsEmpty()) return;
    FPlatformApplicationMisc::ClipboardCopy(*Url);
    PrivateCopiedUntil = FPlatformTime::Seconds() + 2.0;
    ChatCopiedUntil = 0.0;
    RefreshLabels();
}

void UPTStreamerLinkModal::OnCopyChat()
{
    const UPTLocalPartySubsystem* LP = SLM_GetLP(this);
    const FString Url = LP ? LP->GetJoinUrl() : FString();
    if (Url.IsEmpty()) return;
    FPlatformApplicationMisc::ClipboardCopy(*Url);
    ChatCopiedUntil = FPlatformTime::Seconds() + 2.0;
    PrivateCopiedUntil = 0.0;
    RefreshLabels();
}

void UPTStreamerLinkModal::OnCloseClicked()
{
    Close();
}
