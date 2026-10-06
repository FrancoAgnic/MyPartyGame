#include "PTGamepadUINavigator.h"
#include "PTGamepadFocusWidget.h"
#include "PTGamepadSettingsWidget.h"
#include "PTSettingsWidget.h"
#include "../Sculpt/PTSculptPlayerController.h"
#include "../Lobby/PTLobbyPlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/IInputProcessor.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/SlateBlueprintLibrary.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/Slider.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/EditableText.h"
#include "Components/ScrollBox.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogPTGamepadUI, Log, All);

// ── Preprocesador: ve el input antes que los widgets y el juego ─────────────
class FPTGamepadNavProcessor : public IInputProcessor
{
public:
    explicit FPTGamepadNavProcessor(UPTGamepadUINavigator* InOwner) : Owner(InOwner) {}

    virtual void Tick(const float, FSlateApplication&, TSharedRef<ICursor>) override {}

    virtual bool HandleKeyDownEvent(FSlateApplication&, const FKeyEvent& E) override
    {
        UPTGamepadUINavigator* O = Owner.Get();
        return O && O->HandleKey(E.GetKey(), true, E.IsRepeat());
    }
    virtual bool HandleKeyUpEvent(FSlateApplication&, const FKeyEvent& E) override
    {
        UPTGamepadUINavigator* O = Owner.Get();
        return O && O->HandleKey(E.GetKey(), false, false);
    }
    virtual bool HandleAnalogInputEvent(FSlateApplication&, const FAnalogInputEvent& E) override
    {
        if (UPTGamepadUINavigator* O = Owner.Get()) O->HandleAnalog(E.GetKey(), E.GetAnalogValue());
        return false; // los sticks siguen llegando al juego (menús ya frenan move/look por su lado)
    }
    virtual bool HandleMouseMoveEvent(FSlateApplication&, const FPointerEvent& E) override
    {
        if (!E.GetCursorDelta().IsNearlyZero(1.5f))
            if (UPTGamepadUINavigator* O = Owner.Get()) O->HandleMouseActivity();
        return false;
    }
    virtual bool HandleMouseButtonDownEvent(FSlateApplication&, const FPointerEvent&) override
    {
        if (UPTGamepadUINavigator* O = Owner.Get()) O->HandleMouseActivity();
        return false;
    }
    virtual const TCHAR* GetDebugName() const override { return TEXT("PTGamepadUINavigator"); }

private:
    TWeakObjectPtr<UPTGamepadUINavigator> Owner;
};

namespace
{
    bool IsInteractiveType(const UWidget* W)
    {
        return W->IsA<UButton>() || W->IsA<UCheckBox>() || W->IsA<USlider>() || W->IsA<UComboBoxString>()
            || W->IsA<UEditableTextBox>() || W->IsA<UEditableText>();
    }

    void CollectInteractive(UUserWidget* UW, TArray<UWidget*>& Out, int32 Depth = 0)
    {
        if (!UW || !UW->WidgetTree || Depth > 12) return;
        UW->WidgetTree->ForEachWidget([&Out, Depth](UWidget* W)
        {
            if (!W) return;
            if (UUserWidget* Child = Cast<UUserWidget>(W)) CollectInteractive(Child, Out, Depth + 1);
            else if (IsInteractiveType(W)) Out.Add(W);
        });
    }

    FVector2D AbsCenter(const UWidget* W)
    {
        const FGeometry& G = W->GetCachedGeometry();
        return G.GetAbsolutePositionAtCoordinates(FVector2D(0.5f, 0.5f));
    }

    bool NameLooksLikeBack(const FString& Name)
    {
        const FString N = Name.ToLower();
        // Nunca: salir del juego / volver al lobby o al menú (B no debería sacarte de la partida).
        for (const TCHAR* Bad : { TEXT("quit"), TEXT("exit"), TEXT("lobby"), TEXT("menu"), TEXT("leave"), TEXT("salir") })
            if (N.Contains(Bad)) return false;
        for (const TCHAR* Good : { TEXT("back"), TEXT("close"), TEXT("cancel"), TEXT("resume"), TEXT("volver"),
                                   TEXT("cerrar"), TEXT("cancelar"), TEXT("atras"), TEXT("return") })
            if (N.Contains(Good)) return true;
        return false;
    }

    bool IsGamepadButton(const FKey& K) { return K.IsGamepadKey() && !K.IsAxis1D() && !K.IsAxis2D(); }
}

// ── Ciclo de vida ───────────────────────────────────────────────────────────

void UPTGamepadUINavigator::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (FSlateApplication::IsInitialized())
    {
        Processor = MakeShared<FPTGamepadNavProcessor>(this);
        FSlateApplication::Get().RegisterInputPreProcessor(Processor, 0);
    }
}

void UPTGamepadUINavigator::Deinitialize()
{
    if (Processor && FSlateApplication::IsInitialized())
        FSlateApplication::Get().UnregisterInputPreProcessor(Processor);
    Processor.Reset();
    Super::Deinitialize();
}

UWorld* UPTGamepadUINavigator::GetGameWorld() const
{
    const UGameInstance* GI = GetGameInstance();
    return GI ? GI->GetWorld() : nullptr;
}

// ── Input ───────────────────────────────────────────────────────────────────

void UPTGamepadUINavigator::HandleMouseActivity()
{
    if (!bUsingGamepad) return;
    bUsingGamepad = false;
    if (UButton* B = Cast<UButton>(Hovered.Get())) B->OnUnhovered.Broadcast();
    Hovered.Reset();
    if (Highlight) Highlight->HideTarget();
}

void UPTGamepadUINavigator::HandleAnalog(const FKey& Key, float Value)
{
    if      (Key == EKeys::Gamepad_LeftX)  LeftStick.X  = Value;
    else if (Key == EKeys::Gamepad_LeftY)  LeftStick.Y  = Value;
    else if (Key == EKeys::Gamepad_RightX) RightStick.X = Value;
    else if (Key == EKeys::Gamepad_RightY) RightStick.Y = Value;
    else return;
    if (FMath::Abs(Value) > 0.4f) bUsingGamepad = true;
}

bool UPTGamepadUINavigator::HandleKey(const FKey& Key, bool bDown, bool bRepeat)
{
    // Captura para reasignar: el próximo botón del joystick (o Esc para cancelar).
    if (CaptureCallback && bDown && !bRepeat)
    {
        if (IsGamepadButton(Key) || Key == EKeys::Escape)
        {
            TFunction<void(const FKey&)> Cb = MoveTemp(CaptureCallback);
            CaptureCallback = nullptr;
            ConsumedDown.Add(Key);
            Cb(Key == EKeys::Escape ? EKeys::Invalid : Key);
            return true;
        }
    }

    if (!bDown)
    {
        // Tragar el "soltar" de lo que se tragó al apretar (si no, el juego vería un release suelto).
        return ConsumedDown.Remove(Key) > 0;
    }

    if (!IsGamepadButton(Key)) return false;
    bUsingGamepad = true;

    // Re-evaluar YA (no esperar al próximo refresco): el primer toque tiene que funcionar.
    if (!bMenuActive)
    {
        bMenuActive = ComputeMenuContext();
        if (bMenuActive) RefreshCandidates();
    }
    if (!bMenuActive || Candidates.Num() == 0) return false;

    // Start/Menu pasa de largo: el juego lo usa para abrir/cerrar la pausa.
    if (Key == EKeys::Gamepad_Special_Right) return false;

    ConsumedDown.Add(Key);

    // El primer toque solo "muestra" el foco (no activa nada a ciegas).
    if (!Focused.IsValid())
    {
        SetFocus(PickDefault());
        return true;
    }

    if      (Key == EKeys::Gamepad_DPad_Up)    Navigate(FVector2D(0.f, -1.f));
    else if (Key == EKeys::Gamepad_DPad_Down)  Navigate(FVector2D(0.f, 1.f));
    else if (Key == EKeys::Gamepad_DPad_Left)  { if (!Adjust(-1)) Navigate(FVector2D(-1.f, 0.f)); }
    else if (Key == EKeys::Gamepad_DPad_Right) { if (!Adjust(+1)) Navigate(FVector2D(1.f, 0.f)); }
    else if (!bRepeat && Key == EKeys::Gamepad_FaceButton_Bottom) Activate();
    else if (!bRepeat && Key == EKeys::Gamepad_FaceButton_Right)  Back();
    else if (!bRepeat && Key == EKeys::Gamepad_FaceButton_Top && IsSettingsPanelActive()) OpenGamepadSettings();
    else if (Key == EKeys::Gamepad_LeftShoulder)  Adjust(-1);
    else if (Key == EKeys::Gamepad_RightShoulder) Adjust(+1);
    return true;
}

// ── Tick ────────────────────────────────────────────────────────────────────

void UPTGamepadUINavigator::Tick(float DeltaTime)
{
    // Captura vencida.
    if (CaptureCallback && FPlatformTime::Seconds() - CaptureStart > 6.0)
    {
        TFunction<void(const FKey&)> Cb = MoveTemp(CaptureCallback);
        CaptureCallback = nullptr;
        Cb(EKeys::Invalid);
    }

    RefreshAccum += DeltaTime;
    if (RefreshAccum >= 0.1f)
    {
        RefreshAccum = 0.f;
        bMenuActive = bUsingGamepad && ComputeMenuContext();
        if (bMenuActive) RefreshCandidates();
        else
        {
            Candidates.Reset();
            if (UButton* B = Cast<UButton>(Hovered.Get())) B->OnUnhovered.Broadcast();
            Hovered.Reset();
            Focused.Reset();
        }
    }

    if (bMenuActive && Candidates.Num() > 0)
    {
        // El foco desapareció (se cerró el panel / lo tapó un popup) → elegir uno nuevo.
        if (!Focused.IsValid() || !Candidates.Contains(Focused)) SetFocus(PickDefault());

        // Stick izquierdo = cruceta, con repetición al mantener. Pero si el personaje puede caminar
        // (lobby "diorama": UI y personaje a la vez), el stick es para caminar y la UI va con la cruceta.
        const APlayerController* PC = GetGameWorld() ? GetGameWorld()->GetFirstPlayerController() : nullptr;
        const bool bStickNav = !PC || !PC->GetPawn() || PC->IsMoveInputIgnored();
        FIntPoint Dir = FIntPoint::ZeroValue;
        if (!bStickNav) {}
        else if (FMath::Abs(LeftStick.X) > 0.6f && FMath::Abs(LeftStick.X) >= FMath::Abs(LeftStick.Y)) Dir.X = LeftStick.X > 0.f ? 1 : -1;
        else if (FMath::Abs(LeftStick.Y) > 0.6f) Dir.Y = LeftStick.Y > 0.f ? -1 : 1; // stick arriba = Y+ ; pantalla arriba = Y-
        if (Dir != StickDir)
        {
            StickDir = Dir;
            StickRepeat = 0.4f;
            if (Dir != FIntPoint::ZeroValue)
            {
                if (Dir.X == 0 || !Adjust(Dir.X)) Navigate(FVector2D(Dir.X, Dir.Y));
            }
        }
        else if (Dir != FIntPoint::ZeroValue)
        {
            StickRepeat -= DeltaTime;
            if (StickRepeat <= 0.f)
            {
                StickRepeat = 0.12f;
                if (Dir.X == 0 || !Adjust(Dir.X)) Navigate(FVector2D(Dir.X, Dir.Y));
            }
        }

        // Stick derecho = scroll.
        if (FMath::Abs(RightStick.Y) > 0.25f) ScrollBy(-RightStick.Y * 900.f * DeltaTime);
    }

    UpdateHighlight(DeltaTime);
}

bool UPTGamepadUINavigator::ComputeMenuContext() const
{
    UWorld* W = GetGameWorld();
    if (!W) return false;
    APlayerController* PC = W->GetFirstPlayerController();
    if (!PC) return true; // sin controller (boot/menú): toda la pantalla es UI

    // Los menús radiales y las ruedas de color usan los sticks: no son "menú" para esto.
    if (const APTSculptPlayerController* S = Cast<APTSculptPlayerController>(PC))
        if (S->IsColorPickerOpen() || S->IsShapeRadialOpen()) return false;
    if (const APTLobbyPlayerController* L = Cast<APTLobbyPlayerController>(PC))
        if (L->IsHeadColorPickerOpen() || L->IsShapeRadialActive()) return false;

    // Los menús muestran el cursor (todos lo hacen para el mouse). Sin cursor = se está jugando.
    return PC->bShowMouseCursor || SettingsPanel != nullptr && SettingsPanel->IsInViewport();
}

bool UPTGamepadUINavigator::IsUsable(UWidget* W) const
{
    if (!W || !W->GetIsEnabled()) return false;
    const TSharedPtr<SWidget> S = W->GetCachedWidget();
    if (!S.IsValid()) return false;
    const FGeometry& G = W->GetCachedGeometry();
    const FVector2D Size = G.GetLocalSize();
    if (Size.X < 4.f || Size.Y < 4.f) return false;

    // ¿Es lo que está ARRIBA en su centro? (descarta colapsados, tapados por popups, deshabilitados).
    if (!FSlateApplication::IsInitialized()) return false;
    TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(S.ToSharedRef());
    if (!Window.IsValid()) return false;
    TArray<TSharedRef<SWindow>> Windows;
    Windows.Add(Window.ToSharedRef());
    const FWidgetPath Path = FSlateApplication::Get().LocateWindowUnderMouse(AbsCenter(W), Windows, /*bIgnoreEnabledStatus=*/false, 0);
    return Path.IsValid() && Path.ContainsWidget(S.Get());
}

void UPTGamepadUINavigator::RefreshCandidates()
{
    TArray<UWidget*> All;
    if (UWorld* World = GetGameWorld())
    {
        TArray<UUserWidget*> Tops;
        UWidgetBlueprintLibrary::GetAllWidgetsOfClass(World, Tops, UUserWidget::StaticClass(), /*TopLevelOnly=*/true);
        for (UUserWidget* T : Tops)
            if (T && T != Highlight) CollectInteractive(T, All);
    }

    TSet<const void*> NowSet;
    Candidates.Reset();
    for (UWidget* W : All)
        if (IsUsable(W))
        {
            Candidates.Add(W);
            NowSet.Add(W->GetCachedWidget().Get());
        }

    // Si el foco quedó afuera, PickDefault prefiere lo que APARECIÓ ahora (el popup recién abierto).
    if (Focused.IsValid() && !Candidates.Contains(Focused))
    {
        if (UButton* B = Cast<UButton>(Hovered.Get())) B->OnUnhovered.Broadcast();
        Hovered.Reset();
        Focused.Reset();
    }
    // Guardar el set DESPUÉS de elegir (PickDefault lo compara contra el anterior).
    if (!Focused.IsValid() && Candidates.Num() > 0) SetFocus(PickDefault());
    PrevCandidateSet = MoveTemp(NowSet);
}

UWidget* UPTGamepadUINavigator::PickDefault() const
{
    // Preferir lo nuevo (recién aparecido); si no hay nada nuevo, todo. Dentro de eso: arriba-izquierda.
    TArray<UWidget*> Pool;
    for (const TWeakObjectPtr<UWidget>& W : Candidates)
        if (W.IsValid() && !PrevCandidateSet.Contains(W->GetCachedWidget().Get())) Pool.Add(W.Get());
    if (Pool.Num() == 0)
        for (const TWeakObjectPtr<UWidget>& W : Candidates) if (W.IsValid()) Pool.Add(W.Get());
    if (Pool.Num() == 0) return nullptr;

    // Los cuadros de texto no son buen primer foco (abrirían el teclado): preferir botones.
    UWidget* Best = nullptr;
    FVector2D BestPos;
    for (UWidget* W : Pool)
    {
        const bool bText = W->IsA<UEditableTextBox>() || W->IsA<UEditableText>();
        const FVector2D P = AbsCenter(W);
        if (!Best) { Best = W; BestPos = P; continue; }
        const bool bBestText = Best->IsA<UEditableTextBox>() || Best->IsA<UEditableText>();
        if (bBestText && !bText) { Best = W; BestPos = P; continue; }
        if (bText && !bBestText) continue;
        if (P.Y < BestPos.Y - 20.f || (FMath::Abs(P.Y - BestPos.Y) <= 20.f && P.X < BestPos.X)) { Best = W; BestPos = P; }
    }
    return Best;
}

void UPTGamepadUINavigator::SetFocus(UWidget* W)
{
    if (Focused.Get() == W) return;
    if (UButton* Old = Cast<UButton>(Hovered.Get())) Old->OnUnhovered.Broadcast();
    Hovered.Reset();
    Focused = W;
    if (UButton* B = Cast<UButton>(W))
    {
        B->OnHovered.Broadcast(); // sonido de hover + efectos de hover de los WBP
        Hovered = B;
    }
    if (W) ScrollIntoView(W);
}

void UPTGamepadUINavigator::Navigate(const FVector2D& Dir)
{
    UWidget* From = Focused.Get();
    if (!From) { SetFocus(PickDefault()); return; }
    const FVector2D C = AbsCenter(From);

    UWidget* Best = nullptr;
    float BestScore = TNumericLimits<float>::Max();
    for (const TWeakObjectPtr<UWidget>& WP : Candidates)
    {
        UWidget* W = WP.Get();
        if (!W || W == From) continue;
        const FVector2D V = AbsCenter(W) - C;
        const float Along = FVector2D::DotProduct(V, Dir);
        if (Along < 4.f) continue;                               // tiene que estar hacia ese lado
        const float Perp = FMath::Abs(FVector2D::CrossProduct(Dir, V));
        if (Perp > Along * 2.5f) continue;                       // cono de ~68°
        const float Score = Along + Perp * 2.f;
        if (Score < BestScore) { BestScore = Score; Best = W; }
    }
    if (Best) SetFocus(Best);
}

void UPTGamepadUINavigator::Activate()
{
    UWidget* W = Focused.Get();
    if (!W) return;
    if (UButton* B = Cast<UButton>(W))
    {
        TWeakObjectPtr<UButton> Weak(B);
        B->OnPressed.Broadcast();
        if (Weak.IsValid()) Weak->OnReleased.Broadcast();
        if (Weak.IsValid()) Weak->OnClicked.Broadcast(); // puede destruir el widget: todo por weak
    }
    else if (UCheckBox* CB = Cast<UCheckBox>(W))
    {
        CB->SetIsChecked(!CB->IsChecked());
        CB->OnCheckStateChanged.Broadcast(CB->IsChecked());
    }
    else if (W->IsA<UComboBoxString>())
    {
        Adjust(+1);
    }
    else if (W->IsA<UEditableTextBox>() || W->IsA<UEditableText>())
    {
        W->SetKeyboardFocus(); // se escribe con el teclado
    }
}

bool UPTGamepadUINavigator::Adjust(int32 Sign)
{
    UWidget* W = Focused.Get();
    if (USlider* S = Cast<USlider>(W))
    {
        const float Min = S->GetMinValue(), Max = S->GetMaxValue();
        const float Step = S->GetStepSize() > KINDA_SMALL_NUMBER ? FMath::Max(S->GetStepSize(), (Max - Min) / 50.f) : (Max - Min) / 20.f;
        const float V = FMath::Clamp(S->GetValue() + Sign * Step, Min, Max);
        S->SetValue(V);
        S->OnValueChanged.Broadcast(V);
        return true;
    }
    if (UComboBoxString* C = Cast<UComboBoxString>(W))
    {
        const int32 N = C->GetOptionCount();
        if (N <= 0) return true;
        const int32 Cur = FMath::Max(0, C->GetSelectedIndex());
        C->SetSelectedIndex((Cur + Sign + N) % N); // dispara OnSelectionChanged
        return true;
    }
    return false;
}

void UPTGamepadUINavigator::Back()
{
    // 1) Un botón "volver/cerrar/cancelar" visible (preferir el más cercano al foco).
    const FVector2D C = Focused.IsValid() ? AbsCenter(Focused.Get()) : FVector2D::ZeroVector;
    UButton* Best = nullptr;
    float BestD = TNumericLimits<float>::Max();
    for (const TWeakObjectPtr<UWidget>& WP : Candidates)
    {
        UButton* B = Cast<UButton>(WP.Get());
        if (!B || !NameLooksLikeBack(B->GetName())) continue;
        const float D = FVector2D::DistSquared(AbsCenter(B), C);
        if (D < BestD) { BestD = D; Best = B; }
    }
    if (Best)
    {
        SetFocus(Best);
        Activate();
        return;
    }

    // 2) Si no hay, simular Esc (casi todos los paneles se cierran con Esc).
    if (FSlateApplication::IsInitialized())
    {
        const FKeyEvent Down(EKeys::Escape, FModifierKeysState(), 0, false, 0, 0);
        FSlateApplication::Get().ProcessKeyDownEvent(Down);
        FSlateApplication::Get().ProcessKeyUpEvent(Down);
    }
}

void UPTGamepadUINavigator::ScrollIntoView(UWidget* W)
{
    for (UWidget* P = W ? W->GetParent() : nullptr; P; P = P->GetParent())
        if (UScrollBox* SB = Cast<UScrollBox>(P)) { SB->ScrollWidgetIntoView(W, true, EDescendantScrollDestination::IntoView); return; }
}

void UPTGamepadUINavigator::ScrollBy(float Amount)
{
    UWidget* W = Focused.Get();
    for (UWidget* P = W ? W->GetParent() : nullptr; P; P = P->GetParent())
        if (UScrollBox* SB = Cast<UScrollBox>(P))
        {
            SB->SetScrollOffset(FMath::Clamp(SB->GetScrollOffset() + Amount, 0.f, SB->GetScrollOffsetOfEnd()));
            return;
        }
}

void UPTGamepadUINavigator::UpdateHighlight(float DeltaTime)
{
    UWorld* World = GetGameWorld();
    if (!World || !World->GetGameViewport()) return;

    if (!Highlight || !Highlight->IsInViewport())
    {
        // El viewport se vacía en cada cambio de mapa: recrear el borde.
        Highlight = CreateWidget<UPTGamepadFocusWidget>(GetGameInstance(), UPTGamepadFocusWidget::StaticClass());
        if (Highlight) Highlight->AddToViewport(10000);
    }
    if (!Highlight) return;

    UWidget* W = Focused.Get();
    if (!bMenuActive || !bUsingGamepad || !W) { Highlight->HideTarget(); return; }

    const FGeometry& G = W->GetCachedGeometry();
    FVector2D Pixel, TopLeft, BottomRight;
    USlateBlueprintLibrary::AbsoluteToViewport(World, G.GetAbsolutePositionAtCoordinates(FVector2D(0.f, 0.f)), Pixel, TopLeft);
    USlateBlueprintLibrary::AbsoluteToViewport(World, G.GetAbsolutePositionAtCoordinates(FVector2D(1.f, 1.f)), Pixel, BottomRight);
    Highlight->SetTarget(TopLeft, BottomRight - TopLeft, World->GetRealTimeSeconds());
}

// ── Reasignación / panel de configuración ───────────────────────────────────

void UPTGamepadUINavigator::BeginKeyCapture(TFunction<void(const FKey&)> OnKey)
{
    CaptureCallback = MoveTemp(OnKey);
    CaptureStart = FPlatformTime::Seconds();
}

bool UPTGamepadUINavigator::IsSettingsPanelActive() const
{
    for (const TWeakObjectPtr<UWidget>& WP : Candidates)
        if (const UWidget* W = WP.Get())
            if (W->GetTypedOuter<UPTSettingsWidget>()) return true;
    return false;
}

void UPTGamepadUINavigator::OpenGamepadSettings()
{
    if (SettingsPanel && SettingsPanel->IsInViewport()) return;
    SettingsPanel = CreateWidget<UPTGamepadSettingsWidget>(GetGameInstance(), UPTGamepadSettingsWidget::StaticClass());
    if (SettingsPanel)
    {
        SettingsPanel->AddToViewport(900); // el panel maneja el cursor/input mode (ver NativeConstruct)
        Focused.Reset();
    }
}
