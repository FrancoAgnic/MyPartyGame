#include "PTStreamerChoosePopup.h"
#include "PTLocalPartySubsystem.h"
#include "../PTTextTable.h"
#include "../PTGameInstance.h"
#include "../Sculpt/PTSculptGameState.h"
#include "../Lobby/PTPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/Border.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "HAL/PlatformProcess.h"

TSharedRef<SWidget> UPTStreamerChoosePopup::RebuildWidget()
{
    if (!WidgetTree->RootWidget)
    {
        const FLinearColor Panel(0.012f, 0.008f, 0.03f, 0.92f);
        const FLinearColor Accent(1.f, 0.48f, 0.08f, 1.f);
        const FLinearColor Ink(0.96f, 0.96f, 0.99f, 1.f);
        const FLinearColor Muted(0.7f, 0.7f, 0.78f, 1.f);

        auto MakeText = [&](int32 Size, const FLinearColor& Col, bool bBold) -> UTextBlock*
        {
            UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
            FSlateFontInfo F = T->GetFont();
            F.Size = Size;
            F.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
            T->SetFont(F);
            T->SetColorAndOpacity(FSlateColor(Col));
            T->SetJustification(ETextJustify::Center);
            return T;
        };

        UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
        Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible); // no tapa toda la pantalla
        WidgetTree->RootWidget = Root;

        // Tarjeta centrada arriba.
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
        Card->SetBrush(FSlateRoundedBoxBrush(Panel, 16.f, Accent, 2.f));
        Card->SetPadding(FMargin(28.f, 20.f));
        Card->SetHorizontalAlignment(HAlign_Center);
        Card->SetVerticalAlignment(VAlign_Center);
        RootCard = Card;
        if (UOverlaySlot* S = Root->AddChildToOverlay(Card))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetVerticalAlignment(VAlign_Top);
            S->SetPadding(FMargin(0.f, 120.f, 0.f, 0.f));
        }

        UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
        Card->SetContent(Col);

        UTextBlock* Title = MakeText(22, Accent, true);
        Title->SetText(PTText::Get(TEXT("LP_AUD_PICK_TITLE")));
        Col->AddChildToVerticalBox(Title);

        TimerText = MakeText(40, Ink, true);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(TimerText)) S->SetPadding(FMargin(0.f, 2.f, 0.f, 10.f));

        // Botón "Seleccionar palabra".
        UButton* Btn = WidgetTree->ConstructWidget<UButton>();
        FButtonStyle BS = Btn->GetStyle();
        BS.Normal   = FSlateRoundedBoxBrush(Accent, 10.f);
        BS.Hovered  = FSlateRoundedBoxBrush(Accent * 1.15f, 10.f);
        BS.Pressed  = FSlateRoundedBoxBrush(Accent * 0.9f, 10.f);
        Btn->SetStyle(BS);
        Btn->OnClicked.AddDynamic(this, &UPTStreamerChoosePopup::OnSelectClicked);
        UTextBlock* BtnLabel = MakeText(18, FLinearColor(0.05f, 0.03f, 0.f, 1.f), true);
        BtnLabel->SetText(PTText::Get(TEXT("LP_AUD_PICK_BTN")));
        Btn->SetContent(BtnLabel);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Btn)) S->SetHorizontalAlignment(HAlign_Center);

        UTextBlock* Hint = MakeText(14, Muted, false);
        Hint->SetAutoWrapText(true);
        Hint->SetText(PTText::Get(TEXT("LP_AUD_PICK_HINT")));
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Hint)) S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
    }

    if (RootCard) RootCard->SetVisibility(ESlateVisibility::Collapsed); // oculto hasta que toque elegir
    return Super::RebuildWidget();
}

void UPTStreamerChoosePopup::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!RootCard) return;

    const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    const APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;

    bool bShow = false;
    if (GI && GI->bLocalPartyMode && GI->bLocalPartyOnline && G && G->TurnPhase == EPTTurnPhase::ChoosingWord)
        bShow = G->IsActingSculptor(GetOwningPlayerState()); // en audiencia, la TV es el escultor

    if (bShow)
    {
        const int32 Secs = FMath::Max(0, FMath::CeilToInt(G->GetPhaseSecondsRemaining()));
        if (TimerText)
            TimerText->SetText(FText::FromString(FString::Printf(TEXT("%d %s"), Secs, *PTText::GetStr(TEXT("HUD_SECONDS_SHORT")))));
    }
    RootCard->SetVisibility(bShow ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}

void UPTStreamerChoosePopup::OnSelectClicked()
{
    const UPTLocalPartySubsystem* LP = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLocalPartySubsystem>() : nullptr;
    const FString Url = LP ? LP->GetHostJoinUrl() : FString();
    if (!Url.IsEmpty()) FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}
