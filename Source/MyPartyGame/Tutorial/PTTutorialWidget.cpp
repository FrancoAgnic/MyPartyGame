#include "PTTutorialWidget.h"
#include "../PTTextTable.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WrapBox.h"
#include "Components/WrapBoxSlot.h"
#include "Components/EditableText.h"
#include "Components/ScaleBox.h"
#include "Engine/Font.h"
#include "Engine/Texture2D.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace
{
    const FLinearColor TUT_Ink(1.f, 0.97f, 0.92f, 1.f);
    const FLinearColor TUT_Muted(0.72f, 0.68f, 0.84f, 1.f);
    const FLinearColor TUT_Accent(1.f, 0.72f, 0.3f, 1.f);
    const FLinearColor TUT_Card(0.02f, 0.012f, 0.045f, 0.92f);
    const FLinearColor TUT_Chip(0.12f, 0.09f, 0.2f, 1.f);
    const FLinearColor TUT_Green(0.36f, 0.88f, 0.54f, 1.f);
    const FLinearColor TUT_PolaroidInk(0.12f, 0.1f, 0.14f, 1.f);

    UTextBlock* TutText(UWidgetTree* Tree, int32 Size, const FLinearColor& Color, bool bBold)
    {
        UTextBlock* T = Tree->ConstructWidget<UTextBlock>();
        FSlateFontInfo F = T->GetFont();
        F.Size = Size;
        F.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
        T->SetFont(F);
        T->SetColorAndOpacity(FSlateColor(Color));
        return T;
    }
}

// ── Polaroid ────────────────────────────────────────────────────────────────

TSharedRef<SWidget> UPTTutorialPolaroid::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

UFont* UPTTutorialPolaroid::GameFont()
{
    static TWeakObjectPtr<UFont> Cached;
    if (!Cached.IsValid())
        Cached = LoadObject<UFont>(nullptr, TEXT("/Game/Template/UI/Fonts/Super_Bouncer_Font.Super_Bouncer_Font"));
    return Cached.Get();
}

void UPTTutorialPolaroid::BuildTree()
{
    // Marco blanco con borde fino; abajo, más ancho (como una polaroid): nombre + logo.
    UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Frame;
    Frame->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.98f, 0.97f, 0.95f, 1.f), 6.f));
    Frame->SetPadding(FMargin(26.f, 26.f, 26.f, 18.f));
    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    Frame->SetContent(Col);

    PhotoBox = WidgetTree->ConstructWidget<USizeBox>();
    PhotoBox->SetWidthOverride(960.f);
    PhotoBox->SetHeightOverride(540.f);
    PhotoImage = WidgetTree->ConstructWidget<UImage>();
    PhotoBox->SetContent(PhotoImage);
    Col->AddChildToVerticalBox(PhotoBox);

    UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>();
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Bottom)) S->SetPadding(FMargin(6.f, 16.f, 6.f, 4.f));

    // El nombre con la fuente de los menús: fijo (para la foto) o editable (en pantalla).
    FSlateFontInfo Font = GameFont() ? FSlateFontInfo(GameFont(), 46) : FSlateFontInfo();
    if (!GameFont()) Font.Size = 44;
    CaptionText = TutText(WidgetTree, 44, TUT_PolaroidInk, true);
    if (GameFont()) CaptionText->SetFont(Font);
    if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(CaptionText))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetVerticalAlignment(VAlign_Center);
    }
    CaptionEdit = WidgetTree->ConstructWidget<UEditableText>(UEditableText::StaticClass(), TEXT("DogNameInput"));
    {
        FEditableTextStyle St;
        St.SetFont(Font);
        St.SetColorAndOpacity(FSlateColor(TUT_PolaroidInk));
        CaptionEdit->SetWidgetStyle(St);
        CaptionEdit->SetFont(Font);
        CaptionEdit->SetSelectAllTextWhenFocused(true); // al hacer clic se reemplaza "Mi perro" de una
    }
    CaptionEdit->SetHintText(PTText::Get(TEXT("TUT_NAME_HINT")));
    CaptionEdit->OnTextCommitted.AddDynamic(this, &UPTTutorialPolaroid::HandleCaptionCommitted);
    if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(CaptionEdit))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetVerticalAlignment(VAlign_Center);
    }
    CaptionEdit->SetVisibility(ESlateVisibility::Collapsed);

    // Logo: caja de alto fijo + ScaleBox → siempre con su proporción (no se estira).
    USizeBox* LogoBox = WidgetTree->ConstructWidget<USizeBox>();
    LogoBox->SetHeightOverride(90.f);
    LogoBox->SetMaxDesiredWidth(340.f);
    UScaleBox* Fit = WidgetTree->ConstructWidget<UScaleBox>();
    Fit->SetStretch(EStretch::ScaleToFit);
    LogoBox->SetContent(Fit);
    LogoImage = WidgetTree->ConstructWidget<UImage>();
    Fit->SetContent(LogoImage);
    if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(LogoBox))
    {
        S->SetVerticalAlignment(VAlign_Center);
        S->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
    }
}

void UPTTutorialPolaroid::Setup(UTexture2D* Photo, const FText& Caption, UTexture2D* Logo, float PhotoWidth, bool bEditable)
{
    if (!WidgetTree || !WidgetTree->RootWidget) BuildTree();
    if (PhotoImage && Photo) PhotoImage->SetBrushFromTexture(Photo, false);
    if (PhotoBox && Photo && Photo->GetSizeX() > 0)
    {
        PhotoBox->SetWidthOverride(PhotoWidth);
        PhotoBox->SetHeightOverride(PhotoWidth * Photo->GetSizeY() / (float)Photo->GetSizeX());
    }
    if (CaptionText) { CaptionText->SetText(Caption); CaptionText->SetVisibility(bEditable ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
    if (CaptionEdit) { CaptionEdit->SetText(Caption); CaptionEdit->SetVisibility(bEditable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
    if (LogoImage)
    {
        LogoImage->SetVisibility(Logo ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (Logo) LogoImage->SetBrushFromTexture(Logo, /*bMatchSize=*/true); // tamaño nativo: el ScaleBox lo ajusta sin deformar
    }
}

void UPTTutorialPolaroid::LockCaption()
{
    const FText T = GetCaption();
    if (CaptionText) { CaptionText->SetText(T); CaptionText->SetVisibility(ESlateVisibility::HitTestInvisible); }
    if (CaptionEdit) CaptionEdit->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTTutorialPolaroid::SetCaption(const FText& T)
{
    if (CaptionEdit) CaptionEdit->SetText(T);
    if (CaptionText) CaptionText->SetText(T);
}

FText UPTTutorialPolaroid::GetCaption() const
{
    if (CaptionEdit && CaptionEdit->GetVisibility() == ESlateVisibility::Visible) return CaptionEdit->GetText();
    return CaptionText ? CaptionText->GetText() : FText::GetEmpty();
}

void UPTTutorialPolaroid::FocusCaption()
{
    if (CaptionEdit) CaptionEdit->SetKeyboardFocus();
}

void UPTTutorialPolaroid::HandleCaptionCommitted(const FText& Text, ETextCommit::Type Method)
{
    if (Method == ETextCommit::OnEnter) OnCaptionCommitted.Broadcast();
}

// ── Widget principal ────────────────────────────────────────────────────────

TSharedRef<SWidget> UPTTutorialWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

UTextBlock* UPTTutorialWidget::MakeText(int32 Size, const FLinearColor& Color, bool bBold)
{
    return TutText(WidgetTree, Size, Color, bBold);
}

UButton* UPTTutorialWidget::MakeButton(const FText& Label, FName Name, int32 FontSize, const FLinearColor& Fill)
{
    UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    FButtonStyle Style = B->GetStyle();
    Style.Normal  = FSlateRoundedBoxBrush(Fill, 12.f);
    Style.Hovered = FSlateRoundedBoxBrush(Fill * 1.35f, 12.f, TUT_Accent, 2.f);
    Style.Pressed = FSlateRoundedBoxBrush(TUT_Accent, 12.f);
    Style.NormalPadding = FMargin(22.f, 12.f);
    Style.PressedPadding = FMargin(22.f, 12.f);
    B->SetStyle(Style);
    UTextBlock* T = MakeText(FontSize, TUT_Ink, true);
    T->SetText(Label);
    T->SetJustification(ETextJustify::Center);
    B->AddChild(T);
    return B;
}

void UPTTutorialWidget::BuildTree()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // ── Cuadro de diálogo (abajo, centrado; deja libre el hotbar de la derecha) ──
    {
        UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
        Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 22.f, TUT_Accent, 2.f));
        Card->SetBrushColor(TUT_Card);
        Card->SetPadding(FMargin(28.f, 18.f, 28.f, 20.f));
        DialogCard = Card;
        if (UOverlaySlot* S = Root->AddChildToOverlay(Card))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetVerticalAlignment(VAlign_Bottom);
            S->SetPadding(FMargin(0.f, 0.f, 0.f, 18.f)); // abajo, a la izquierda del hotbar
        }
        USizeBox* Width = WidgetTree->ConstructWidget<USizeBox>();
        DialogWidth = Width;
        Width->SetWidthOverride(900.f);
        Card->SetContent(Width);
        UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
        Width->SetContent(Col);

        UTextBlock* Name = MakeText(18, TUT_Accent, true);
        Name->SetText(FText::FromString(TEXT("SCULPI")));
        Col->AddChildToVerticalBox(Name);

        BodyText = MakeText(24, TUT_Ink, false);
        BodyText->SetAutoWrapText(true);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(BodyText)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 6.f));

        HintsBox = WidgetTree->ConstructWidget<UWrapBox>();
        HintsBox->SetInnerSlotPadding(FVector2D(8.f, 6.f));
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(HintsBox)) S->SetPadding(FMargin(0.f, 4.f, 0.f, 0.f));

        AimText = MakeText(18, TUT_Green, true);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(AimText)) S->SetPadding(FMargin(0.f, 8.f, 0.f, 0.f));
        AimText->SetVisibility(ESlateVisibility::Collapsed);

        UHorizontalBox* PRow = WidgetTree->ConstructWidget<UHorizontalBox>();
        ProgressRow = PRow;
        Progress = WidgetTree->ConstructWidget<UProgressBar>();
        Progress->SetFillColorAndOpacity(TUT_Green);
        if (UHorizontalBoxSlot* S = PRow->AddChildToHorizontalBox(Progress))
        {
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            S->SetVerticalAlignment(VAlign_Center);
        }
        ProgressLabel = MakeText(18, TUT_Green, true);
        if (UHorizontalBoxSlot* S = PRow->AddChildToHorizontalBox(ProgressLabel))
        {
            S->SetVerticalAlignment(VAlign_Center);
            S->SetPadding(FMargin(14.f, 0.f, 0.f, 0.f));
        }
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(PRow)) S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
        PRow->SetVisibility(ESlateVisibility::Collapsed);
    }

    // ── Aviso arriba a la derecha: cómo saltar ──
    {
        UTextBlock* Skip = MakeText(15, TUT_Muted, false);
        Skip->SetText(PTText::Get(TEXT("TUT_SKIP_HINT")));
        if (UOverlaySlot* S = Root->AddChildToOverlay(Skip))
        {
            S->SetHorizontalAlignment(HAlign_Right);
            S->SetVerticalAlignment(VAlign_Top);
            S->SetPadding(FMargin(0.f, 18.f, 28.f, 0.f));
        }
    }

    // ── Panel de pausa (con el menú ESC abierto): arriba al centro, encima del menú ──
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        PausePanel = Row;
        UButton* SkipBtn = MakeButton(PTText::Get(TEXT("TUT_SKIP")), TEXT("SkipTutorialButton"), 20, FLinearColor(0.35f, 0.08f, 0.1f, 1.f));
        SkipBtn->OnClicked.AddDynamic(this, &UPTTutorialWidget::HandleSkip);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(SkipBtn)) S->SetPadding(FMargin(8.f, 0.f));
        DoneButton = MakeButton(PTText::Get(TEXT("TUT_DONE_BTN")), TEXT("TutorialDoneButton"), 20, FLinearColor(0.08f, 0.3f, 0.16f, 1.f));
        DoneButton->OnClicked.AddDynamic(this, &UPTTutorialWidget::HandleDone);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(DoneButton)) S->SetPadding(FMargin(8.f, 0.f));
        if (UOverlaySlot* S = Root->AddChildToOverlay(Row))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetVerticalAlignment(VAlign_Top);
            S->SetPadding(FMargin(0.f, 40.f, 0.f, 0.f));
        }
        Row->SetVisibility(ESlateVisibility::Collapsed);
    }

    // ── Cuenta regresiva ──
    {
        CountdownText = MakeText(200, FLinearColor::White, true);
        CountdownText->SetShadowOffset(FVector2D(4.f, 4.f));
        CountdownText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.6f));
        if (UOverlaySlot* S = Root->AddChildToOverlay(CountdownText))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetVerticalAlignment(VAlign_Center);
        }
        CountdownText->SetVisibility(ESlateVisibility::Collapsed);
    }

    // ── Foto en su marco ──
    {
        UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
        PhotoPanel = Col;
        Polaroid = CreateWidget<UPTTutorialPolaroid>(this, UPTTutorialPolaroid::StaticClass());
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Polaroid)) S->SetHorizontalAlignment(HAlign_Center);
        SavedText = MakeText(18, TUT_Ink, true);
        SavedText->SetJustification(ETextJustify::Center);
        SavedText->SetShadowOffset(FVector2D(2.f, 2.f));
        SavedText->SetShadowColorAndOpacity(FLinearColor(0.f, 0.f, 0.f, 0.7f));
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(SavedText))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetPadding(FMargin(0.f, 14.f, 0.f, 10.f));
        }
        SavePhotoButton = MakeButton(PTText::Get(TEXT("TUT_SAVE_PHOTO")), TEXT("TutorialSavePhotoButton"), 22, FLinearColor(0.08f, 0.3f, 0.16f, 1.f));
        SavePhotoButton->OnClicked.AddDynamic(this, &UPTTutorialWidget::HandleSavePhoto);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(SavePhotoButton)) S->SetHorizontalAlignment(HAlign_Center);
        ShareButton = MakeButton(PTText::Get(TEXT("TUT_SHARE_STEAM")), TEXT("TutorialShareButton"), 22, FLinearColor(0.09f, 0.16f, 0.32f, 1.f));
        ShareButton->OnClicked.AddDynamic(this, &UPTTutorialWidget::HandleShare);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ShareButton)) { S->SetHorizontalAlignment(HAlign_Center); S->SetPadding(FMargin(0.f, 0.f, 0.f, 8.f)); }
        ShareButton->SetVisibility(ESlateVisibility::Collapsed);
        ContinueButton = MakeButton(PTText::Get(TEXT("TUT_CONTINUE")), TEXT("TutorialContinueButton"), 22, FLinearColor(0.08f, 0.3f, 0.16f, 1.f));
        ContinueButton->OnClicked.AddDynamic(this, &UPTTutorialWidget::HandleContinue);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ContinueButton)) S->SetHorizontalAlignment(HAlign_Center);
        ContinueButton->SetVisibility(ESlateVisibility::Collapsed);
        BuildKeyboard(Col);
        if (UOverlaySlot* S = Root->AddChildToOverlay(Col))
        {
            S->SetHorizontalAlignment(HAlign_Center);
            S->SetVerticalAlignment(VAlign_Center);
            S->SetPadding(FMargin(0.f, 0.f, 0.f, 120.f));
        }
        Col->SetVisibility(ESlateVisibility::Collapsed);
    }

    // ── Flash blanco (foto) ──
    {
        FlashOverlay = WidgetTree->ConstructWidget<UBorder>();
        FlashOverlay->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, 0.f));
        if (UOverlaySlot* S = Root->AddChildToOverlay(FlashOverlay))
        {
            S->SetHorizontalAlignment(HAlign_Fill);
            S->SetVerticalAlignment(VAlign_Fill);
        }
        FlashOverlay->SetVisibility(ESlateVisibility::Collapsed);
    }

    // Por defecto nada intercepta el mouse; los paneles con botones se vuelven Visible al mostrarse.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    if (DialogCard) DialogCard->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPTTutorialWidget::Say(const FText& Text)
{
    FullText = Text.ToString();
    Revealed = 0.f;
    ShownChars = -1;
    VoiceCounter = 0;
    if (BodyText) BodyText->SetText(FText::GetEmpty());
}

void UPTTutorialWidget::FinishTyping()
{
    Revealed = (float)FullText.Len();
}

void UPTTutorialWidget::SetHints(const TArray<FPTTutHint>& Hints)
{
    if (!HintsBox) return;
    FString Sig;
    for (const FPTTutHint& H : Hints)
    {
        Sig += H.Key.ToString() + TEXT("|") + H.Label.ToString() + FString::Printf(TEXT("|%d"), H.State);
        for (const UTexture2D* I : H.Icons) Sig += I ? I->GetName() : TEXT("-");
        Sig += TEXT(";");
    }
    if (Sig == HintsSig) return; // no rearmar cada frame
    HintsSig = Sig;
    HintsBox->ClearChildren();
    for (const FPTTutHint& H : Hints)
    {
        UBorder* Chip = WidgetTree->ConstructWidget<UBorder>();
        // Paso a paso: el que toca va con borde; lo hecho, en verde; lo que viene, apagado.
        if (H.State == 3)      Chip->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0.08f, 0.36f, 0.18f, 1.f), 10.f, TUT_Green, 2.f));
        else if (H.State == 2) Chip->SetBrush(FSlateRoundedBoxBrush(TUT_Chip, 10.f, TUT_Accent, 3.f));
        else                   Chip->SetBrush(FSlateRoundedBoxBrush(TUT_Chip, 10.f));
        if (H.State == 1) Chip->SetRenderOpacity(0.4f);
        Chip->SetPadding(FMargin(10.f, 5.f));
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        Chip->SetContent(Row);
        // La tecla: su ícono (Content/UMG/Texture/NewUI/Gameplay_UI/Keyboards) o, si no hay, una
        // "keycap" clara con el nombre. Al lado, para qué sirve.
        if (H.Icons.Num() > 0)
        {
            for (UTexture2D* Icon : H.Icons)
            {
                USizeBox* IB = WidgetTree->ConstructWidget<USizeBox>();
                IB->SetHeightOverride(38.f);
                IB->SetWidthOverride(Icon && Icon->GetSizeY() > 0 ? 38.f * Icon->GetSizeX() / (float)Icon->GetSizeY() : 38.f);
                UImage* Img = WidgetTree->ConstructWidget<UImage>();
                Img->SetBrushFromTexture(Icon, false);
                IB->SetContent(Img);
                if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(IB))
                {
                    S->SetVerticalAlignment(VAlign_Center);
                    S->SetPadding(FMargin(0.f, 0.f, 2.f, 0.f));
                }
            }
        }
        else
        {
            UBorder* Cap = WidgetTree->ConstructWidget<UBorder>();
            Cap->SetBrush(FSlateRoundedBoxBrush(TUT_Ink, 6.f));
            Cap->SetPadding(FMargin(8.f, 2.f));
            UTextBlock* K = MakeText(16, TUT_PolaroidInk, true);
            K->SetText(H.Key);
            Cap->SetContent(K);
            if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(Cap)) S->SetVerticalAlignment(VAlign_Center);
        }
        UTextBlock* L = MakeText(16, TUT_Ink, H.State == 2);
        L->SetText(H.State == 3 ? FText::FromString(H.Label.ToString() + TEXT("  \u2713")) : H.Label);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(L))
        {
            S->SetVerticalAlignment(VAlign_Center);
            S->SetPadding(FMargin(8.f, 0.f, 0.f, 0.f));
        }
        HintsBox->AddChildToWrapBox(Chip);
    }
    HintsBox->SetVisibility(Hints.Num() > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UPTTutorialWidget::SetProgress(float Fraction, const FText& Label)
{
    if (!ProgressRow) return;
    if (Fraction < 0.f) { ProgressRow->SetVisibility(ESlateVisibility::Collapsed); return; }
    ProgressRow->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (Progress) Progress->SetPercent(FMath::Clamp(Fraction, 0.f, 1.f));
    if (ProgressLabel && !ProgressLabel->GetText().EqualTo(Label)) ProgressLabel->SetText(Label);
}

void UPTTutorialWidget::SetDockLeft(bool bLeft)
{
    if (bLeft == bDockedLeft || !DialogCard) return;
    bDockedLeft = bLeft;
    if (UOverlaySlot* S = Cast<UOverlaySlot>(DialogCard->Slot))
    {
        S->SetHorizontalAlignment(bLeft ? HAlign_Left : HAlign_Center);
        S->SetPadding(bLeft ? FMargin(24.f, 0.f, 0.f, 18.f) : FMargin(0.f, 0.f, 0.f, 18.f));
    }
    if (DialogWidth) DialogWidth->SetWidthOverride(bLeft ? 520.f : 900.f);
}

void UPTTutorialWidget::SetAim(const FText& Text, const FLinearColor& Color)
{
    if (!AimText) return;
    AimText->SetVisibility(Text.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
    if (!AimText->GetText().EqualTo(Text)) AimText->SetText(Text);
    AimText->SetColorAndOpacity(FSlateColor(Color));
}

void UPTTutorialWidget::SetDialogVisible(bool bVisible)
{
    if (DialogCard) DialogCard->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UPTTutorialWidget::SetPauseOptions(bool bShow, bool bShowDone)
{
    if (PausePanel) PausePanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (DoneButton) DoneButton->SetVisibility(bShowDone ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UPTTutorialWidget::ShowCountdown(int32 N)
{
    if (!CountdownText) return;
    CountdownText->SetVisibility(N > 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (N > 0) CountdownText->SetText(FText::AsNumber(N));
}

void UPTTutorialWidget::Flash()
{
    FlashAlpha = 1.f;
    if (FlashOverlay) FlashOverlay->SetVisibility(ESlateVisibility::HitTestInvisible);
}

void UPTTutorialWidget::ShowPhoto(UTexture2D* Photo, const FText& DefaultName)
{
    if (!LoadedLogo) LoadedLogo = LogoTexture.LoadSynchronous();
    if (Polaroid)
    {
        Polaroid->Setup(Photo, DefaultName, LoadedLogo, 760.f, /*bEditable=*/true); // entra en 900p con los botones
        Polaroid->OnCaptionCommitted.RemoveAll(this);
        Polaroid->OnCaptionCommitted.AddUObject(this, &UPTTutorialWidget::HandleSavePhoto);
    }
    if (SavedText) SavedText->SetText(PTText::Get(TEXT("TUT_NAME_HINT_LONG")));
    if (SavePhotoButton) SavePhotoButton->SetVisibility(ESlateVisibility::Visible);
    if (ContinueButton) ContinueButton->SetVisibility(ESlateVisibility::Collapsed);
    if (PhotoPanel) PhotoPanel->SetVisibility(ESlateVisibility::Visible);
    if (Polaroid) Polaroid->FocusCaption();
}

void UPTTutKeyButton::HandleClick()
{
    if (UPTTutorialWidget* W = Owner.Get()) W->TypeKey(Key);
}

void UPTTutorialWidget::BuildKeyboard(UVerticalBox* Into)
{
    // Teclado en pantalla (para joystick): filas de botones; el navegador de UI del juego los recorre.
    UVerticalBox* KB = WidgetTree->ConstructWidget<UVerticalBox>();
    KeyboardPanel = KB;
    static const TCHAR* Rows[] = { TEXT("1234567890"), TEXT("QWERTYUIOP"), TEXT("ASDFGHJKL\u00D1"), TEXT("ZXCVBNM") };
    auto AddKey = [this](UHorizontalBox* Row, const FString& Key, const FString& Label, float Width)
    {
        UPTTutKeyButton* B = WidgetTree->ConstructWidget<UPTTutKeyButton>(UPTTutKeyButton::StaticClass());
        FButtonStyle Style = B->GetStyle();
        Style.Normal  = FSlateRoundedBoxBrush(TUT_Chip, 8.f);
        Style.Hovered = FSlateRoundedBoxBrush(TUT_Chip * 1.6f, 8.f, TUT_Accent, 2.f);
        Style.Pressed = FSlateRoundedBoxBrush(TUT_Accent, 8.f);
        Style.NormalPadding = FMargin(4.f);
        Style.PressedPadding = FMargin(4.f);
        B->SetStyle(Style);
        B->Key = Key;
        B->Owner = this;
        B->Bind();
        USizeBox* Size = WidgetTree->ConstructWidget<USizeBox>();
        Size->SetWidthOverride(Width);
        Size->SetHeightOverride(52.f);
        UTextBlock* T = MakeText(22, TUT_Ink, true);
        T->SetText(FText::FromString(Label));
        T->SetJustification(ETextJustify::Center);
        Size->SetContent(T);
        B->AddChild(Size);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(B)) S->SetPadding(FMargin(3.f));
        if (!FirstKey) FirstKey = B;
    };
    for (const TCHAR* R : Rows)
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        for (const TCHAR* C = R; *C; ++C) AddKey(Row, FString(1, C), FString(1, C), 52.f);
        if (UVerticalBoxSlot* S = KB->AddChildToVerticalBox(Row)) S->SetHorizontalAlignment(HAlign_Center);
    }
    UHorizontalBox* Last = WidgetTree->ConstructWidget<UHorizontalBox>();
    AddKey(Last, TEXT(" "), PTText::GetStr(TEXT("TUT_KB_SPACE")), 220.f);
    AddKey(Last, TEXT("BKSP"), TEXT("\u2190"), 110.f);
    AddKey(Last, TEXT("OK"), PTText::GetStr(TEXT("TUT_SAVE_PHOTO")), 220.f);
    if (UVerticalBoxSlot* S = KB->AddChildToVerticalBox(Last)) S->SetHorizontalAlignment(HAlign_Center);
    if (UVerticalBoxSlot* S = Into->AddChildToVerticalBox(KB))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetPadding(FMargin(0.f, 10.f, 0.f, 0.f));
    }
    KB->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTTutorialWidget::SetOnScreenKeyboard(bool bShow)
{
    if (!KeyboardPanel || bShow == bKeyboardShown) return;
    bKeyboardShown = bShow;
    KeyboardPanel->SetVisibility(bShow ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bShow && FirstKey) FirstKey->SetKeyboardFocus(); // el joystick arranca en la primera tecla
}

void UPTTutorialWidget::TypeKey(const FString& Key)
{
    if (!Polaroid) return;
    if (Key == TEXT("OK")) { HandleSavePhoto(); return; }
    FString Name = bKeyboardFresh ? FString() : Polaroid->GetCaption().ToString();
    bKeyboardFresh = false;
    if (Key == TEXT("BKSP")) Name.LeftChopInline(1);
    else if (Name.Len() < 24)
    {
        // Mayúscula al principio y después de un espacio; el resto en minúscula.
        const bool bUpper = Name.IsEmpty() || Name.EndsWith(TEXT(" "));
        Name += bUpper ? Key.ToUpper() : Key.ToLower();
    }
    Polaroid->SetCaption(FText::FromString(Name));
}

void UPTTutorialWidget::ShowPhotoSaved(const FText& SavedMsg, bool bCanShare)
{
    if (ShareButton) ShareButton->SetVisibility(bCanShare ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (Polaroid) Polaroid->LockCaption();
    SetOnScreenKeyboard(false);
    if (SavedText) SavedText->SetText(SavedMsg);
    if (SavePhotoButton) SavePhotoButton->SetVisibility(ESlateVisibility::Collapsed);
    if (ContinueButton) ContinueButton->SetVisibility(ESlateVisibility::Visible);
}

FText UPTTutorialWidget::GetPhotoName() const
{
    return Polaroid ? Polaroid->GetCaption() : FText::GetEmpty();
}

void UPTTutorialWidget::HidePhoto()
{
    if (PhotoPanel) PhotoPanel->SetVisibility(ESlateVisibility::Collapsed);
}

bool UPTTutorialWidget::IsPhotoShown() const
{
    return PhotoPanel && PhotoPanel->GetVisibility() == ESlateVisibility::Visible;
}

void UPTTutorialWidget::PlayVoice()
{
    const double Now = FPlatformTime::Seconds();
    if (Now - LastVoiceTime < 0.075) return;
    LastVoiceTime = Now;
    if (VoiceSounds.Num() == 0) return;
    USoundBase* Snd = VoiceSounds[FMath::RandRange(0, VoiceSounds.Num() - 1)].LoadSynchronous();
    if (Snd) UGameplayStatics::PlaySound2D(this, Snd, 0.32f, FMath::FRandRange(1.05f, 1.45f));
}

void UPTTutorialWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Texto letra por letra + "bla bla" cada par de letras.
    if (Revealed < FullText.Len())
    {
        Revealed = FMath::Min((float)FullText.Len(), Revealed + InDeltaTime * CharsPerSecond);
    }
    const int32 N = FMath::FloorToInt(Revealed);
    if (N != ShownChars && BodyText)
    {
        for (int32 i = FMath::Max(0, ShownChars); i < N; ++i)
            if (!FChar::IsWhitespace(FullText[i]) && (++VoiceCounter % 3) == 1) PlayVoice();
        ShownChars = N;
        BodyText->SetText(FText::FromString(FullText.Left(N)));
    }

    if (FlashAlpha > 0.f && FlashOverlay)
    {
        FlashAlpha = FMath::Max(0.f, FlashAlpha - InDeltaTime * 2.2f);
        FlashOverlay->SetBrushColor(FLinearColor(1.f, 1.f, 1.f, FlashAlpha));
        if (FlashAlpha <= 0.f) FlashOverlay->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UPTTutorialWidget::HandleSkip()     { OnSkipClicked.Broadcast(); }
void UPTTutorialWidget::HandleDone()     { OnDoneClicked.Broadcast(); }
void UPTTutorialWidget::HandleContinue() { OnContinueClicked.Broadcast(); }
void UPTTutorialWidget::HandleSavePhoto() { OnSavePhotoClicked.Broadcast(); }
void UPTTutorialWidget::HandleShare() { OnShareClicked.Broadcast(); }
