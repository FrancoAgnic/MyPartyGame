#include "PTGamepadSettingsWidget.h"
#include "PTGamepadUINavigator.h"
#include "../PTGamepad.h"
#include "../PTGameUserSettings.h"
#include "../PTTextTable.h"
#include "../Sculpt/PTSculptPlayerController.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

namespace
{
    const FLinearColor GPS_Ink(1.f, 0.97f, 0.92f, 1.f);
    const FLinearColor GPS_Muted(0.72f, 0.68f, 0.84f, 1.f);
    const FLinearColor GPS_Accent(1.f, 0.72f, 0.3f, 1.f);
    const FLinearColor GPS_Panel(0.03f, 0.02f, 0.06f, 0.96f);
    const FLinearColor GPS_ButtonFill(0.12f, 0.09f, 0.2f, 1.f);
    const FLinearColor GPS_ButtonHover(0.22f, 0.16f, 0.36f, 1.f);
}

void UPTGamepadRebindHandler::HandleClicked()
{
    if (UPTGamepadSettingsWidget* O = Owner.Get()) O->StartRebind(ActionId);
}

TSharedRef<SWidget> UPTGamepadSettingsWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget) BuildTree();
    return Super::RebuildWidget();
}

UTextBlock* UPTGamepadSettingsWidget::MakeText(int32 Size, const FLinearColor& Color, bool bBold)
{
    UTextBlock* T = WidgetTree->ConstructWidget<UTextBlock>();
    FSlateFontInfo F = T->GetFont();
    F.Size = Size;
    F.TypefaceFontName = bBold ? FName(TEXT("Bold")) : FName(TEXT("Regular"));
    T->SetFont(F);
    T->SetColorAndOpacity(FSlateColor(Color));
    return T;
}

UButton* UPTGamepadSettingsWidget::MakeButton(const FText& Label, FName Name, UTextBlock** OutText)
{
    UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
    FButtonStyle Style = B->GetStyle();
    Style.Normal  = FSlateRoundedBoxBrush(GPS_ButtonFill, 10.f);
    Style.Hovered = FSlateRoundedBoxBrush(GPS_ButtonHover, 10.f);
    Style.Pressed = FSlateRoundedBoxBrush(GPS_Accent, 10.f);
    Style.NormalPadding = FMargin(14.f, 6.f);
    Style.PressedPadding = FMargin(14.f, 6.f);
    B->SetStyle(Style);
    UTextBlock* T = MakeText(18, GPS_Ink, true);
    T->SetText(Label);
    B->AddChild(T);
    if (OutText) *OutText = T;
    return B;
}

USlider* UPTGamepadSettingsWidget::AddSliderRow(UVerticalBox* Box, const FText& Label, float Min, float Max, float Step, UTextBlock*& OutValue)
{
    UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
    UTextBlock* L = MakeText(18, GPS_Ink, false);
    L->SetText(Label);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(L))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetVerticalAlignment(VAlign_Center);
    }
    USizeBox* SliderBox = WidgetTree->ConstructWidget<USizeBox>();
    SliderBox->SetWidthOverride(260.f);
    USlider* Slider = WidgetTree->ConstructWidget<USlider>();
    Slider->SetMinValue(Min);
    Slider->SetMaxValue(Max);
    Slider->SetStepSize(Step);
    Slider->SetSliderBarColor(GPS_Muted);
    Slider->SetSliderHandleColor(GPS_Accent);
    SliderBox->SetContent(Slider);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(SliderBox)) S->SetVerticalAlignment(VAlign_Center);
    USizeBox* ValueBox = WidgetTree->ConstructWidget<USizeBox>();
    ValueBox->SetWidthOverride(70.f);
    OutValue = MakeText(18, GPS_Accent, true);
    OutValue->SetJustification(ETextJustify::Right);
    ValueBox->SetContent(OutValue);
    if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ValueBox)) S->SetVerticalAlignment(VAlign_Center);
    if (UVerticalBoxSlot* S = Box->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 4.f));
    return Slider;
}

void UPTGamepadSettingsWidget::BuildTree()
{
    UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("Root"));
    WidgetTree->RootWidget = Root;

    // Fondo que tapa (y bloquea) lo de atrás: así el joystick no navega hacia el menú de abajo.
    UBorder* Dim = WidgetTree->ConstructWidget<UBorder>();
    Dim->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 0.f));
    Dim->SetBrushColor(FLinearColor(0.f, 0.f, 0.f, 0.55f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Dim))
    {
        S->SetHorizontalAlignment(HAlign_Fill);
        S->SetVerticalAlignment(VAlign_Fill);
    }

    UBorder* Card = WidgetTree->ConstructWidget<UBorder>();
    Card->SetBrush(FSlateRoundedBoxBrush(FLinearColor::White, 24.f));
    Card->SetBrushColor(GPS_Panel);
    Card->SetPadding(FMargin(32.f, 26.f));
    if (UOverlaySlot* S = Root->AddChildToOverlay(Card))
    {
        S->SetHorizontalAlignment(HAlign_Center);
        S->SetVerticalAlignment(VAlign_Center);
    }
    USizeBox* CardSize = WidgetTree->ConstructWidget<USizeBox>();
    CardSize->SetWidthOverride(640.f);
    CardSize->SetMaxDesiredHeight(860.f);
    Card->SetContent(CardSize);

    UVerticalBox* Col = WidgetTree->ConstructWidget<UVerticalBox>();
    CardSize->SetContent(Col);

    UTextBlock* Title = MakeText(32, GPS_Accent, true);
    Title->SetText(PTText::Get(TEXT("GP_TITLE")));
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Title)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    LookSlider = AddSliderRow(Col, PTText::Get(TEXT("GP_LOOK_SENS")), 0.2f, 3.f, 0.1f, LookValue);
    MoveSlider = AddSliderRow(Col, PTText::Get(TEXT("GP_MOVE_SENS")), 0.3f, 1.f, 0.05f, MoveValue);
    DeadSlider = AddSliderRow(Col, PTText::Get(TEXT("GP_DEADZONE")), 0.05f, 0.5f, 0.01f, DeadValue);
    LookSlider->OnValueChanged.AddDynamic(this, &UPTGamepadSettingsWidget::OnLookChanged);
    MoveSlider->OnValueChanged.AddDynamic(this, &UPTGamepadSettingsWidget::OnMoveChanged);
    DeadSlider->OnValueChanged.AddDynamic(this, &UPTGamepadSettingsWidget::OnDeadZoneChanged);

    // Invertir Y.
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        UTextBlock* L = MakeText(18, GPS_Ink, false);
        L->SetText(PTText::Get(TEXT("GP_INVERT_Y")));
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(L))
        {
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            S->SetVerticalAlignment(VAlign_Center);
        }
        USizeBox* ToggleBox = WidgetTree->ConstructWidget<USizeBox>();
        ToggleBox->SetWidthOverride(100.f);
        UButton* Toggle = MakeButton(FText::GetEmpty(), TEXT("InvertToggle"), &InvertText);
        Toggle->OnClicked.AddDynamic(this, &UPTGamepadSettingsWidget::OnInvertClicked);
        ToggleBox->SetContent(Toggle);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(ToggleBox)) S->SetVerticalAlignment(VAlign_Center);
        if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 6.f));
    }

    UTextBlock* ButtonsTitle = MakeText(22, GPS_Accent, true);
    ButtonsTitle->SetText(PTText::Get(TEXT("GP_BUTTONS")));
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(ButtonsTitle)) S->SetPadding(FMargin(0.f, 14.f, 0.f, 6.f));

    // Lista de acciones (con scroll: son muchas).
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Scroll)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    for (const FPTGamepadAction& A : PTGamepad::GetActions())
    {
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();
        UTextBlock* L = MakeText(17, GPS_Ink, false);
        L->SetText(PTText::Get(A.LabelKey));
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(L))
        {
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            S->SetVerticalAlignment(VAlign_Center);
        }
        USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>();
        KeyBox->SetWidthOverride(220.f);
        UTextBlock* KeyText = nullptr;
        UButton* B = MakeButton(FText::GetEmpty(), *FString::Printf(TEXT("Rebind_%s"), *A.Id.ToString()), &KeyText);
        KeyBox->SetContent(B);
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(KeyBox)) S->SetVerticalAlignment(VAlign_Center);
        KeyTexts.Add(A.Id, KeyText);

        UPTGamepadRebindHandler* H = NewObject<UPTGamepadRebindHandler>(this);
        H->ActionId = A.Id;
        H->Owner = this;
        B->OnClicked.AddDynamic(H, &UPTGamepadRebindHandler::HandleClicked);
        Handlers.Add(H);

        Scroll->AddChild(Row);
        if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(Row->Slot)) S->SetPadding(FMargin(0.f, 3.f));
    }

    // Abajo: restaurar / volver.
    UHorizontalBox* Bottom = WidgetTree->ConstructWidget<UHorizontalBox>();
    UButton* Reset = MakeButton(PTText::Get(TEXT("GP_RESET")), TEXT("ResetButton"));
    Reset->OnClicked.AddDynamic(this, &UPTGamepadSettingsWidget::OnResetClicked);
    UButton* Back = MakeButton(PTText::Get(TEXT("GP_BACK")), TEXT("BackButton")); // "Back" → B lo encuentra solo
    Back->OnClicked.AddDynamic(this, &UPTGamepadSettingsWidget::OnBackClicked);
    if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(Reset)) S->SetPadding(FMargin(0.f, 0.f, 12.f, 0.f));
    UTextBlock* Hint = MakeText(14, GPS_Muted, false);
    Hint->SetText(PTText::Get(TEXT("GP_HINT")));
    if (UHorizontalBoxSlot* S = Bottom->AddChildToHorizontalBox(Hint))
    {
        S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        S->SetVerticalAlignment(VAlign_Center);
        S->SetHorizontalAlignment(HAlign_Center);
    }
    Bottom->AddChildToHorizontalBox(Back);
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Bottom)) S->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
}

void UPTGamepadSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    RefreshValues();
    PlayPopIn();

    // Con el panel abierto hace falta el cursor; al cerrar se vuelve a como estaba.
    if (APlayerController* PC = GetOwningPlayer() ? GetOwningPlayer() : (GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr))
    {
        bHadCursor = PC->bShowMouseCursor;
        PC->bShowMouseCursor = true;
        FInputModeGameAndUI Mode;
        Mode.SetHideCursorDuringCapture(false);
        PC->SetInputMode(Mode);
    }
}

void UPTGamepadSettingsWidget::NativeDestruct()
{
    if (!bHadCursor)
        if (APlayerController* PC = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr)
        {
            PC->bShowMouseCursor = false;
            PC->SetInputMode(FInputModeGameOnly());
        }
    Super::NativeDestruct();
}

void UPTGamepadSettingsWidget::RefreshValues()
{
    if (LookSlider) LookSlider->SetValue(PTGamepad::LookSensitivity());
    if (MoveSlider) MoveSlider->SetValue(PTGamepad::MoveSensitivity());
    if (DeadSlider) DeadSlider->SetValue(PTGamepad::DeadZone());
    if (LookValue)  LookValue->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), PTGamepad::LookSensitivity())));
    if (MoveValue)  MoveValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(PTGamepad::MoveSensitivity() * 100.f))));
    if (DeadValue)  DeadValue->SetText(FText::FromString(FString::Printf(TEXT("%d%%"), FMath::RoundToInt(PTGamepad::DeadZone() * 100.f))));
    if (InvertText) InvertText->SetText(PTText::Get(PTGamepad::InvertY() ? TEXT("GP_ON") : TEXT("GP_OFF")));
    for (const FPTGamepadAction& A : PTGamepad::GetActions())
        if (UTextBlock** T = KeyTexts.Find(A.Id))
            if (*T) (*T)->SetText(A.Id == RebindingId ? PTText::Get(TEXT("GP_PRESS")) : FText::FromString(PTGamepad::KeyLabel(A.Key)));
}

void UPTGamepadSettingsWidget::OnLookChanged(float V)
{
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get()) { S->SetGamepadLookSensitivity(V); S->SaveSettings(); }
    RefreshValues();
}

void UPTGamepadSettingsWidget::OnMoveChanged(float V)
{
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get()) { S->SetGamepadMoveSensitivity(V); S->SaveSettings(); }
    RefreshValues();
}

void UPTGamepadSettingsWidget::OnDeadZoneChanged(float V)
{
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get()) { S->SetGamepadDeadZone(V); S->SaveSettings(); }
    RefreshValues();
}

void UPTGamepadSettingsWidget::OnInvertClicked()
{
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get()) { S->SetGamepadInvertY(!S->GetGamepadInvertY()); S->SaveSettings(); }
    RefreshValues();
}

void UPTGamepadSettingsWidget::StartRebind(FName ActionId)
{
    UPTGamepadUINavigator* Nav = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTGamepadUINavigator>() : nullptr;
    if (!Nav) return;
    RebindingId = ActionId;
    RefreshValues();
    TWeakObjectPtr<UPTGamepadSettingsWidget> Weak(this);
    Nav->BeginKeyCapture([Weak, ActionId](const FKey& Key)
    {
        UPTGamepadSettingsWidget* Self = Weak.Get();
        if (!Self) return;
        Self->RebindingId = NAME_None;
        if (Key.IsValid() && PTGamepad::IsAssignableKey(Key))
        {
            PTGamepad::SetKey(ActionId, Key);
            Self->ApplyToControllers();
        }
        Self->RefreshValues();
    });
}

void UPTGamepadSettingsWidget::OnResetClicked()
{
    PTGamepad::ResetToDefaults();
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
    {
        S->SetGamepadLookSensitivity(1.f);
        S->SetGamepadMoveSensitivity(1.f);
        S->SetGamepadDeadZone(0.2f);
        S->SetGamepadInvertY(false);
        S->SaveSettings();
    }
    ApplyToControllers();
    RefreshValues();
}

void UPTGamepadSettingsWidget::ApplyToControllers()
{
    // Rebindear en vivo (si estás en una partida, el cambio vale ya, sin reiniciar).
    if (UWorld* W = GetWorld())
        for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
            if (APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(It->Get()))
                if (PC->IsLocalController()) PC->RebuildGamepadInput();
}

void UPTGamepadSettingsWidget::OnBackClicked()
{
    RemoveFromParent();
}
