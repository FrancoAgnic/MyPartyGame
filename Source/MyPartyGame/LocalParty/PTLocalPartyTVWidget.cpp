#include "PTLocalPartyTVWidget.h"
#include "PTLocalPartySubsystem.h"
#include "PTQRCode.h"
#include "../PTGamepad.h"
#include "PTSculptGameState.h"
#include "PTSculptGameMode.h"
#include "PTSculptPlayerController.h"
#include "../PTTextTable.h"
#include "../Lobby/PTPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
    FString Fmt(const TCHAR* Key, const FString& Arg)
    {
        return PTText::Format(FName(Key), FFormatOrderedArguments{ FFormatArgumentValue(FText::FromString(Arg)) }).ToString();
    }
}

TSharedRef<SWidget> UPTLocalPartyTVWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

UTextBlock* UPTLocalPartyTVWidget::MakeText(int32 Size, const FLinearColor& Color, bool bBold, bool bWrap)
{
    UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo F = T->GetFont();
    F.Size = Size;
    F.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
    T->SetFont(F);
    T->SetColorAndOpacity(FSlateColor(Color));
    T->SetAutoWrapText(bWrap);
    T->SetShadowOffset(FVector2D(1.f, 1.f));
    T->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
    return T;
}

UBorder* UPTLocalPartyTVWidget::MakePanel(const FLinearColor& Color, float Radius, const FMargin& InPadding)
{
    UBorder* B = WidgetTree->ConstructWidget<UBorder>();
    B->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, Radius));
    B->SetBrushColor(Color);
    B->SetPadding(InPadding);
    return B;
}

void UPTLocalPartyTVWidget::BuildTree()
{
    const FLinearColor Ink(1.f, 0.97f, 0.92f, 1.f);
    const FLinearColor Muted(0.72f, 0.68f, 0.84f, 1.f);

    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // ── Panel central: unirse con el celular ────────────────────────────────
    LobbyPanel = MakePanel(PanelColor, 28.f, FMargin(40.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(LobbyPanel))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    LobbyPanel->SetContent(Row);

    // QR (fondo blanco ya incluido en la textura, con su borde silencioso).
    USizeBox* QrBox = WidgetTree->ConstructWidget<USizeBox>();
    QrBox->SetWidthOverride(QrSize);
    QrBox->SetHeightOverride(QrSize);
    QrImage = WidgetTree->ConstructWidget<UImage>();
    QrBox->SetContent(QrImage);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(QrBox)) S->SetVerticalAlignment(VAlign_Center);

    USpacer* Gap = WidgetTree->ConstructWidget<USpacer>();
    Gap->SetSize(FVector2D(40.f, 1.f));
    Row->AddChildToHorizontalBox(Gap);

    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Col)) S->SetVerticalAlignment(VAlign_Center);

    auto AddToCol = [Col](UWidget* W, float Bottom)
    {
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(W)) S->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
    };

    UTextBlock* Title = MakeText(TitleFontSize, AccentColor, true);
    Title->SetText(PTText::Get(TEXT("LP_JOIN_TITLE")));
    AddToCol(Title, 14.f);

    UTextBlock* Step1 = MakeText(BodyFontSize, Ink, false);
    Step1->SetText(PTText::Get(TEXT("LP_JOIN_STEP1")));
    AddToCol(Step1, 4.f);
    UTextBlock* Step2 = MakeText(BodyFontSize, Ink, false);
    Step2->SetText(PTText::Get(TEXT("LP_JOIN_STEP2")));
    AddToCol(Step2, 6.f);

    UrlText = MakeText(UrlFontSize, FLinearColor::White, true);
    AddToCol(UrlText, 2.f);
    RevealHint = MakeText(BodyFontSize - 6, Muted, false);
    RevealHint->SetText(PTText::Get(TEXT("LP_SHOW_IP")));
    AddToCol(RevealHint, 22.f);

    PlayersTitle = MakeText(BodyFontSize, Muted, true);
    AddToCol(PlayersTitle, 8.f);

    PlayersBox = WidgetTree->ConstructWidget<UVerticalBox>();
    AddToCol(PlayersBox, 18.f);

    StatusText = MakeText(BodyFontSize, AccentColor, true, /*bWrap=*/true);
    AddToCol(StatusText, 0.f);

    // ── Cartel de turno ("¡Pásale el joystick a X!") ────────────────────────
    TurnBanner = MakePanel(PanelColor, 24.f, FMargin(40.f, 24.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(TurnBanner))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }
    UVerticalBox* BCol = WidgetTree->ConstructWidget<UVerticalBox>();
    TurnBanner->SetContent(BCol);
    BannerTitle = MakeText(BannerFontSize, AccentColor, true);
    BannerTitle->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* S = BCol->AddChildToVerticalBox(BannerTitle)) S->SetHorizontalAlignment(HAlign_Center);
    BannerSub = MakeText(BodyFontSize + 4, Ink, false);
    BannerSub->SetJustification(ETextJustify::Center);
    if (UVerticalBoxSlot* S = BCol->AddChildToVerticalBox(BannerSub))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
    }

    // ── Esquina: URL chiquita para los que llegan tarde ─────────────────────
    CornerPanel = MakePanel(PanelColor, 12.f, FMargin(14.f, 8.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(CornerPanel))
    {
        // Abajo al centro: a la izquierda está el chat y a la derecha el hotbar.
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Bottom);
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 16.f));
    }
    CornerText = MakeText(16, Ink, true);
    CornerPanel->SetContent(CornerText);

    // ── Ayuda de controles del joystick (mientras se esculpe) ───────────────
    PadPanel = MakePanel(PanelColor, 12.f, FMargin(14.f, 8.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(PadPanel))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Bottom);
        S->SetPadding(FMargin(0.f, 0.f, 0.f, 60.f)); // arriba de la URL chica (arriba está el reloj)
    }
    PadText = MakeText(15, Ink, false, /*bWrap=*/true);
    PadText->SetJustification(ETextJustify::Center);
    USizeBox* PadWidth = WidgetTree->ConstructWidget<USizeBox>();
    PadWidth->SetMaxDesiredWidth(820.f); // la lista es larga: que corte en 2-3 líneas
    PadWidth->SetContent(PadText);
    PadPanel->SetContent(PadWidth);

    SetVisibility(ESlateVisibility::HitTestInvisible); // nunca roba el mouse
}

void UPTLocalPartyTVWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    RefreshAccum += InDeltaTime;
    if (RefreshAccum >= 0.25f)
    {
        RefreshAccum = 0.f;
        Refresh();
    }
}

void UPTLocalPartyTVWidget::Refresh()
{
    if (!LobbyPanel) return;
    UGameInstance* GI = GetGameInstance();
    UPTLocalPartySubsystem* LP = GI ? GI->GetSubsystem<UPTLocalPartySubsystem>() : nullptr;
    const APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
    if (!LP) return;

    const EPTTurnPhase Phase = G ? G->TurnPhase : EPTTurnPhase::WaitingForPlayers;
    const FString Url = LP->GetJoinUrl();
    const bool bServerOk = LP->IsServerRunning() && !Url.IsEmpty();

    auto Show = [](UWidget* W, bool bOn)
    {
        if (W) W->SetVisibility(bOn ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    };

    // QR (se regenera solo si cambió la URL).
    if (Url != QrForUrl)
    {
        QrForUrl = Url;
        QrTexture = Url.IsEmpty() ? nullptr : PTQR::MakeTexture(Url, 8);
        if (QrImage && QrTexture) QrImage->SetBrushFromTexture(QrTexture, /*bMatchSize=*/false);
        Show(QrImage, QrTexture != nullptr);
    }
    const bool bReveal = IsRevealHeld();
    if (UrlText) UrlText->SetText(FText::FromString(DisplayAddress(Url, bReveal)));
    Show(RevealHint, bMaskJoinAddress && bServerOk && !bReveal);

    // Lista de jugadores (se rearma solo si cambió algo).
    const TArray<FPTPhonePlayer>& Players = LP->GetPlayers();
    FString Sig;
    for (const FPTPhonePlayer& P : Players)
        Sig += FString::Printf(TEXT("%s|%d|%d;"), *P.Name, P.bOnline ? 1 : 0, P.bVip ? 1 : 0);
    if (Sig != PlayersSig && PlayersBox)
    {
        PlayersSig = Sig;
        PlayersBox->ClearChildren();
        for (const FPTPhonePlayer& P : Players)
        {
            UHorizontalBox* R = WidgetTree->ConstructWidget<UHorizontalBox>();
            USizeBox* DotBox = WidgetTree->ConstructWidget<USizeBox>();
            DotBox->SetWidthOverride(18.f);
            DotBox->SetHeightOverride(18.f);
            DotBox->SetContent(MakePanel(P.Color, 9.f, FMargin(0.f)));
            if (UHorizontalBoxSlot* S = R->AddChildToHorizontalBox(DotBox))
            {
                S->SetVerticalAlignment(VAlign_Center);
                S->SetPadding(FMargin(0.f, 0.f, 10.f, 0.f));
            }
            UTextBlock* N = MakeText(BodyFontSize + 2, P.bOnline ? FLinearColor::White : FLinearColor(1.f, 1.f, 1.f, 0.4f), true);
            N->SetText(FText::FromString(P.bVip ? P.Name + TEXT("  ★") : P.Name));
            if (UHorizontalBoxSlot* S = R->AddChildToHorizontalBox(N)) S->SetVerticalAlignment(VAlign_Center);
            if (UVerticalBoxSlot* S = PlayersBox->AddChildToVerticalBox(R)) S->SetPadding(FMargin(0.f, 3.f));
        }
    }
    if (PlayersTitle)
        PlayersTitle->SetText(Players.Num() > 0 ? FText::FromString(Fmt(TEXT("LP_PLAYERS"), FString::FromInt(Players.Num())))
                                                : PTText::Get(TEXT("LP_NOBODY")));

    // Qué falta para arrancar.
    int32 MinPlayers = 2;
    if (const UWorld* W = GetWorld())
        if (const APTSculptGameMode* GM = W->GetAuthGameMode<APTSculptGameMode>())
            MinPlayers = GM->LocalParty_GetMinPlayers();
    FString Status;
    if (!bServerOk)                         Status = PTText::GetStr(TEXT("LP_NO_SERVER"));
    else if (Players.Num() < MinPlayers)    Status = Fmt(TEXT("LP_NEED_MORE"), FString::FromInt(MinPlayers - Players.Num()));
    else                                    Status = Fmt(TEXT("LP_WAIT_VIP"), LP->GetVipName());
    if (StatusText) StatusText->SetText(FText::FromString(Status));

    // Visibilidad por fase.
    const bool bLobby = Phase == EPTTurnPhase::WaitingForPlayers;
    // Con la pausa abierta, el panel grande no tapa el menú.
    const APTSculptPlayerController* SPC = Cast<APTSculptPlayerController>(GetOwningPlayer());
    const bool bPaused = SPC && SPC->IsEscapeMenuOpen();
    Show(LobbyPanel, bLobby && !bPaused);

    const bool bChoosing = Phase == EPTTurnPhase::ChoosingWord;
    Show(TurnBanner, bChoosing);
    if (bChoosing && G && G->CurrentSculptor)
    {
        const FString Name = G->CurrentSculptor->GetPlayerName();
        if (BannerTitle) BannerTitle->SetText(FText::FromString(Fmt(TEXT("LP_PASS_PAD"), Name)));
        if (BannerSub)   BannerSub->SetText(PTText::Get(TEXT("LP_CHOOSING")));
        if (const FPTPhonePlayer* Rec = LP->FindByPlayerState(G->CurrentSculptor))
            if (BannerTitle) BannerTitle->SetColorAndOpacity(FSlateColor(Rec->Color));
    }

    Show(CornerPanel, !bLobby && bServerOk);
    if (CornerText) CornerText->SetText(FText::FromString(Fmt(TEXT("LP_JOIN_SMALL"), DisplayAddress(Url, bReveal))));

    Show(PadPanel, Phase == EPTTurnPhase::Drawing);
    // Con los botones ACTUALES (se pueden reasignar en el panel "Joystick").
    if (PadText && Phase == EPTTurnPhase::Drawing) PadText->SetText(FText::FromString(PTGamepad::BuildHintLine()));
}

bool UPTLocalPartyTVWidget::IsRevealHeld() const
{
    const APlayerController* PC = GetOwningPlayer();
    return PC && (PC->IsInputKeyDown(EKeys::Gamepad_Special_Left) || PC->IsInputKeyDown(EKeys::I));
}

FString UPTLocalPartyTVWidget::DisplayAddress(const FString& Url, bool bReveal) const
{
    FString Addr = Url.Replace(TEXT("http://"), TEXT(""));
    if (!bMaskJoinAddress || bReveal || Addr.IsEmpty()) return Addr;

    // "192.168.1.15:8787" → "192.168.•••.•••:8787": los dos primeros octetos son los de casi cualquier
    // red de casa (no identifican nada); se tapan los dos últimos.
    FString Host = Addr, Port;
    Addr.Split(TEXT(":"), &Host, &Port);
    TArray<FString> Oct;
    Host.ParseIntoArray(Oct, TEXT("."));
    if (Oct.Num() != 4) return TEXT("•••");
    const FString Dots = TEXT("•••");
    FString Out = Oct[0] + TEXT(".") + Oct[1] + TEXT(".") + Dots + TEXT(".") + Dots;
    if (!Port.IsEmpty()) Out += TEXT(":") + Port;
    return Out;
}
