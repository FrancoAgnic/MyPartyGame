#include "PTGamepadSettingsWidget.h"
#include "PTGamepadUINavigator.h"
#include "../PTGamepad.h"
#include "../PTInputBindings.h"
#include "../PTGameUserSettings.h"
#include "../PTTextTable.h"
#include "../Sculpt/PTSculptPlayerController.h"
#include "../Lobby/PTLobbyPlayerController.h"
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
    Title->SetText(PTText::Get(TEXT("CTRL_TITLE")));
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Title)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 10.f));

    // Barra de pestañas: Teclado y ratón / Joystick.
    UHorizontalBox* Tabs = WidgetTree->ConstructWidget<UHorizontalBox>();
    KbTabButton  = MakeButton(PTText::Get(TEXT("CTRL_TAB_KB")),  TEXT("KbTab"));
    PadTabButton = MakeButton(PTText::Get(TEXT("CTRL_TAB_PAD")), TEXT("PadTab"));
    KbTabButton->OnClicked.AddDynamic(this, &UPTGamepadSettingsWidget::OnKbTabClicked);
    PadTabButton->OnClicked.AddDynamic(this, &UPTGamepadSettingsWidget::OnPadTabClicked);
    if (UHorizontalBoxSlot* S = Tabs->AddChildToHorizontalBox(KbTabButton))  { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(0.f, 0.f, 6.f, 0.f)); }
    if (UHorizontalBoxSlot* S = Tabs->AddChildToHorizontalBox(PadTabButton)) { S->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); S->SetPadding(FMargin(6.f, 0.f, 0.f, 0.f)); }
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(Tabs)) S->SetPadding(FMargin(0.f, 0.f, 0.f, 12.f));

    // Cuerpo (se rearma al cambiar de pestaña).
    BodyBox = WidgetTree->ConstructWidget<UVerticalBox>();
    if (UVerticalBoxSlot* S = Col->AddChildToVerticalBox(BodyBox)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));

    // Abajo (compartido): restaurar / volver.
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

    ShowTab(ActiveTab);
}

void UPTGamepadSettingsWidget::ShowTab(EPTControlsTab Tab)
{
    ActiveTab   = Tab;
    RebindingId = NAME_None;
    // Los widgets del cuerpo anterior se destruyen: invalidar los punteros para que RefreshValues no los toque.
    LookSlider = MoveSlider = DeadSlider = MouseSensSlider = nullptr;
    LookValue  = MoveValue  = DeadValue  = InvertText = MouseSensValue = nullptr;
    KeyTexts.Reset();
    Handlers.Reset();
    if (BodyBox) BodyBox->ClearChildren();

    if (Tab == EPTControlsTab::Gamepad) BuildGamepadBody(BodyBox);
    else                                BuildKeyboardBody(BodyBox);

    ApplyTabVisual();
    RefreshValues();
}

void UPTGamepadSettingsWidget::ApplyTabVisual()
{
    const bool bKb = (ActiveTab == EPTControlsTab::Keyboard);
    if (KbTabButton)  KbTabButton->SetBackgroundColor(bKb ? GPS_Accent : GPS_ButtonFill);
    if (PadTabButton) PadTabButton->SetBackgroundColor(bKb ? GPS_ButtonFill : GPS_Accent);
}

void UPTGamepadSettingsWidget::OnKbTabClicked()  { ShowTab(EPTControlsTab::Keyboard); }
void UPTGamepadSettingsWidget::OnPadTabClicked() { ShowTab(EPTControlsTab::Gamepad); }

void UPTGamepadSettingsWidget::BuildGamepadBody(UVerticalBox* Body)
{
    if (!Body) return;

    LookSlider = AddSliderRow(Body, PTText::Get(TEXT("GP_LOOK_SENS")), 0.2f, 3.f, 0.1f, LookValue);
    MoveSlider = AddSliderRow(Body, PTText::Get(TEXT("GP_MOVE_SENS")), 0.3f, 1.f, 0.05f, MoveValue);
    DeadSlider = AddSliderRow(Body, PTText::Get(TEXT("GP_DEADZONE")), 0.05f, 0.5f, 0.01f, DeadValue);
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
        if (UVerticalBoxSlot* S = Body->AddChildToVerticalBox(Row)) S->SetPadding(FMargin(0.f, 6.f));
    }

    UTextBlock* ButtonsTitle = MakeText(22, GPS_Accent, true);
    ButtonsTitle->SetText(PTText::Get(TEXT("GP_BUTTONS")));
    if (UVerticalBoxSlot* S = Body->AddChildToVerticalBox(ButtonsTitle)) S->SetPadding(FMargin(0.f, 14.f, 0.f, 6.f));

    // Lista de acciones (con scroll: son muchas).
    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Body->AddChildToVerticalBox(Scroll)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
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
}

void UPTGamepadSettingsWidget::BuildKeyboardBody(UVerticalBox* Body)
{
    if (!Body) return;

    // Sensibilidad de la cámara con MOUSE (reemplaza las filas de "Mover" y "Cámara").
    MouseSensSlider = AddSliderRow(Body, PTText::Get(TEXT("CTRL_MOUSE_SENS")), 0.2f, 3.f, 0.1f, MouseSensValue);
    MouseSensSlider->OnValueChanged.AddDynamic(this, &UPTGamepadSettingsWidget::OnMouseSensChanged);

    UTextBlock* Hdr = MakeText(22, GPS_Accent, true);
    Hdr->SetText(PTText::Get(TEXT("CTRL_KB_HEADER")));
    if (UVerticalBoxSlot* S = Body->AddChildToVerticalBox(Hdr)) S->SetPadding(FMargin(0.f, 12.f, 0.f, 6.f));

    UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>();
    if (UVerticalBoxSlot* S = Body->AddChildToVerticalBox(Scroll)) S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    for (const FPTKeyBinding& Bn : PTInput::GetBindings())
    {
        // "Mover" y "Cámara" se sacan de la lista (el mouse se ajusta con el slider de arriba).
        if (Bn.Id == FName(TEXT("Move")) || Bn.Id == FName(TEXT("Look"))) continue;
        UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>();

        // Nombre + (si tiene) una nota de uso debajo: "Mantener", "Doble clic reinicia"...
        UVerticalBox* NameCol = WidgetTree->ConstructWidget<UVerticalBox>();
        UTextBlock* L = MakeText(17, Bn.bRebindable ? GPS_Ink : GPS_Muted, false);
        L->SetText(Bn.Label);
        NameCol->AddChildToVerticalBox(L);
        if (!Bn.Note.IsEmpty())
        {
            UTextBlock* NoteT = MakeText(12, GPS_Muted, false);
            NoteT->SetText(Bn.Note);
            NameCol->AddChildToVerticalBox(NoteT);
        }
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(NameCol))
        {
            S->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            S->SetVerticalAlignment(VAlign_Center);
        }
        USizeBox* KeyBox = WidgetTree->ConstructWidget<USizeBox>();
        KeyBox->SetWidthOverride(220.f);
        if (Bn.bRebindable)
        {
            UTextBlock* KeyText = nullptr;
            UButton* B = MakeButton(FText::GetEmpty(), *FString::Printf(TEXT("KbRebind_%s"), *Bn.Id.ToString()), &KeyText);
            KeyBox->SetContent(B);
            KeyTexts.Add(Bn.Id, KeyText);

            UPTGamepadRebindHandler* H = NewObject<UPTGamepadRebindHandler>(this);
            H->ActionId = Bn.Id;
            H->Owner = this;
            B->OnClicked.AddDynamic(H, &UPTGamepadRebindHandler::HandleClicked);
            Handlers.Add(H);
        }
        else
        {
            // Teclas fijas (WASD/cámara/volar/esculpir): se muestran como info, sin botón.
            UTextBlock* KeyText = MakeText(17, GPS_Muted, true);
            KeyText->SetJustification(ETextJustify::Center);
            KeyBox->SetContent(KeyText);
            KeyTexts.Add(Bn.Id, KeyText);
        }
        if (UHorizontalBoxSlot* S = Row->AddChildToHorizontalBox(KeyBox)) S->SetVerticalAlignment(VAlign_Center);

        Scroll->AddChild(Row);
        if (UScrollBoxSlot* S = Cast<UScrollBoxSlot>(Row->Slot)) S->SetPadding(FMargin(0.f, 3.f));
    }
}

void UPTGamepadSettingsWidget::NativeConstruct()
{
    Super::NativeConstruct();
    SetIsFocusable(true); // para recibir las teclas al reasignar (captura de teclado en el widget)
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
    if (ActiveTab == EPTControlsTab::Gamepad)
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
    else // Teclado + ratón
    {
        if (const UPTGameUserSettings* S = UPTGameUserSettings::Get())
        {
            const float MS = S->GetMouseLookSensitivity();
            if (MouseSensSlider) MouseSensSlider->SetValue(MS);
            if (MouseSensValue)  MouseSensValue->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), MS)));
        }
        for (const FPTKeyBinding& Bn : PTInput::GetBindings())
            if (UTextBlock** T = KeyTexts.Find(Bn.Id))
                if (*T) (*T)->SetText((Bn.bRebindable && Bn.Id == RebindingId)
                    ? PTText::Get(TEXT("GP_PRESS"))
                    : Bn.Key.GetDisplayName(/*bLongDisplayName=*/false));
    }
}

void UPTGamepadSettingsWidget::OnMouseSensChanged(float V)
{
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get()) { S->SetMouseLookSensitivity(V); S->SaveSettings(); }
    if (MouseSensValue) MouseSensValue->SetText(FText::FromString(FString::Printf(TEXT("%.1fx"), V)));
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
    // TECLADO/RATÓN: capturamos en el propio widget (tiene foco). El preprocessor del joystick no
    // entrega las teclas del teclado en este estado de UI, por eso no alcanzaba con BeginKeyCapture.
    if (ActiveTab == EPTControlsTab::Keyboard)
    {
        BeginKeyboardCapture(ActionId);
        return;
    }

    // JOYSTICK: el próximo botón del joystick (vía el navegador/preprocessor).
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

void UPTGamepadSettingsWidget::BeginKeyboardCapture(FName ActionId)
{
    bCapturingKb = true;
    KbCaptureId  = ActionId;
    RebindingId  = ActionId;
    RefreshValues(); // muestra "Presiona una tecla…" en esa fila
    // Asegurar que el widget reciba las teclas (los Preview* se enrutan por el widget con foco).
    SetKeyboardFocus();
}

bool UPTGamepadSettingsWidget::FinishKeyboardCapture(const FKey& Key)
{
    if (!bCapturingKb) return false;
    const FName ActionId = KbCaptureId;
    bCapturingKb = false;
    KbCaptureId  = NAME_None;
    RebindingId  = NAME_None;

    // Escape (o tecla inválida) = cancelar, sin cambiar nada.
    if (Key.IsValid() && Key != EKeys::Escape)
    {
        PTInput::SetKey(ActionId, Key);
        ApplyKeyboardToControllers();
    }
    RefreshValues();
    return true;
}

FReply UPTGamepadSettingsWidget::NativeOnPreviewKeyDown(const FGeometry& G, const FKeyEvent& E)
{
    if (bCapturingKb && FinishKeyboardCapture(E.GetKey()))
        return FReply::Handled();
    return Super::NativeOnPreviewKeyDown(G, E);
}

FReply UPTGamepadSettingsWidget::NativeOnPreviewMouseButtonDown(const FGeometry& G, const FPointerEvent& E)
{
    // Permitir asignar botones del ratón (incluido click sobre un botón de la lista): Preview corre ANTES
    // que el hijo, así que consumimos el click y lo usamos como la tecla a asignar.
    if (bCapturingKb && FinishKeyboardCapture(E.GetEffectingButton()))
        return FReply::Handled();
    return Super::NativeOnPreviewMouseButtonDown(G, E);
}

void UPTGamepadSettingsWidget::OnResetClicked()
{
    if (ActiveTab == EPTControlsTab::Gamepad)
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
    }
    else // Teclado + ratón: borrar los overrides, resetear la sensibilidad y volver a los defaults.
    {
        if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
        {
            S->ClearKeyOverrides();
            S->SetMouseLookSensitivity(1.f);
            S->SaveSettings();
        }
        PTInput::RefreshFromSettings();
        ApplyKeyboardToControllers();
    }
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

void UPTGamepadSettingsWidget::ApplyKeyboardToControllers()
{
    // El rebind de teclado/ratón vale en TODOS los modos de esculpido: gameplay y level creator
    // (APTSculptPlayerController) y el esculpido de cabeza del Locker (APTLobbyPlayerController).
    if (UWorld* W = GetWorld())
        for (FConstPlayerControllerIterator It = W->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (!PC || !PC->IsLocalController()) continue;
            if (APTSculptPlayerController* S = Cast<APTSculptPlayerController>(PC)) S->RebuildKeyboardInput();
            else if (APTLobbyPlayerController* L = Cast<APTLobbyPlayerController>(PC)) L->RebuildHeadSculptInput();
        }
}

void UPTGamepadSettingsWidget::OnBackClicked()
{
    RemoveFromParent();
}
