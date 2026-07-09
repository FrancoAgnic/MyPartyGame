#include "PTColorPickerWidget.h"
#include "Components/Image.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/PanelWidget.h"
#include "Components/SizeBox.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/ConfigCacheIni.h"
#include "Framework/Application/SlateApplication.h"
#include "../Sculpt/PTSculptPlayerController.h"

static FString ColorToHex(const FLinearColor& C)
{
    const FColor c = C.ToFColor(true);
    return FString::Printf(TEXT("#%02x%02x%02x"), c.R, c.G, c.B);
}

void UPTColorPickerWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (ValueSlider)
    {
        ValueSlider->SetMinValue(0.f);
        ValueSlider->SetMaxValue(1.f);
        ValueSlider->SetValue(1.f);
        ValueSlider->OnValueChanged.AddDynamic(this, &UPTColorPickerWidget::OnValueChanged);
    }
    if (ConfirmButton)
        ConfirmButton->OnClicked.AddDynamic(this, &UPTColorPickerWidget::Confirm);
    if (AddButton)
        AddButton->OnClicked.AddDynamic(this, &UPTColorPickerWidget::OnAddClicked);

    // Cargar y mostrar la paleta guardada.
    LoadPalette();
    BuildSwatches();

    // Inicializar con el color de pintura actual del controller.
    if (APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(GetOwningPlayer()))
        SetColor(PC->CurrentPaintColor);
    else
        RefreshUI();
}

// ─── Paleta persistente ───────────────────────────────────────────────────────

void UPTColorPickerWidget::LoadPalette()
{
    Palette.Reset();
    TArray<FString> Arr;
    GConfig->GetArray(TEXT("PTColorPicker"), TEXT("Palette"), Arr, GGameUserSettingsIni);
    for (const FString& S : Arr)
        Palette.Add(FLinearColor(FColor::FromHex(S)));
    // Por si el ini quedó con más de los permitidos, dejar los últimos MaxSwatches.
    while (Palette.Num() > MaxSwatches) Palette.RemoveAt(0);
}

void UPTColorPickerWidget::SavePalette() const
{
    TArray<FString> Arr;
    for (const FLinearColor& C : Palette)
        Arr.Add(C.ToFColor(true).ToHex());
    GConfig->SetArray(TEXT("PTColorPicker"), TEXT("Palette"), Arr, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni);
}

void UPTColorPickerWidget::BuildSwatches()
{
    for (const FLinearColor& C : Palette)
        AddSwatchButton(C);
}

void UPTColorPickerWidget::ClearSwatches()
{
    for (UWidget* W : SwatchWrappers)
        if (W) W->RemoveFromParent();
    SwatchWrappers.Reset();
    SwatchButtons.Reset();
}

void UPTColorPickerWidget::AddSwatchButton(const FLinearColor& C)
{
    if (!SwatchBox || !WidgetTree) return;

    UButton* B = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass());
    FButtonStyle St = B->GetStyle();
    St.Normal.TintColor  = FSlateColor(C);
    St.Hovered.TintColor = FSlateColor(C);
    St.Pressed.TintColor = FSlateColor(C);
    B->SetStyle(St);
    B->OnClicked.AddDynamic(this, &UPTColorPickerWidget::OnSwatchClicked);

    // Tamaño fijo para que se vea como cuadradito.
    USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
    Box->SetWidthOverride(40.f);
    Box->SetHeightOverride(40.f);
    Box->AddChild(B);

    SwatchBox->AddChild(Box);
    SwatchButtons.Add(B);
    SwatchWrappers.Add(Box);
}

void UPTColorPickerWidget::OnAddClicked()
{
    Palette.Add(CurrentColor);
    // Buffer rotativo: si se pasa del máximo, cae el más viejo (el primero).
    while (Palette.Num() > MaxSwatches) Palette.RemoveAt(0);
    SavePalette();

    // Reconstruir los swatches para reflejar el desplazamiento.
    ClearSwatches();
    BuildSwatches();
}

void UPTColorPickerWidget::OnSwatchClicked()
{
    // OnClicked no trae el emisor: el botón clickeado es el que quedó hovered.
    for (int32 i = 0; i < SwatchButtons.Num(); ++i)
        if (SwatchButtons[i] && SwatchButtons[i]->IsHovered() && Palette.IsValidIndex(i))
        {
            SetColor(Palette[i]);
            return;
        }
}

void UPTColorPickerWidget::OnValueChanged(float V)
{
    Val = FMath::Clamp(V, 0.f, 1.f);
    RecomputeColor();
}

bool UPTColorPickerWidget::UpdateFromMouse(const FPointerEvent& InMouseEvent)
{
    return UpdateFromAbsolute(InMouseEvent.GetScreenSpacePosition());
}

bool UPTColorPickerWidget::UpdateFromAbsolute(FVector2D AbsPos)
{
    if (!Wheel) return false;

    const FGeometry& G = Wheel->GetCachedGeometry();
    const FVector2D Size  = G.GetLocalSize();
    if (Size.X <= 0.f || Size.Y <= 0.f) return false;

    const FVector2D Local  = G.AbsoluteToLocal(AbsPos);
    const FVector2D Center = Size * 0.5f;
    const FVector2D D      = Local - Center;
    const float     Radius = FMath::Min(Size.X, Size.Y) * 0.5f;

    const float r = (Radius > 0.f) ? FMath::Clamp(D.Size() / Radius, 0.f, 1.f) : 0.f;
    // Ángulo → matiz [0,1). Y de pantalla crece hacia abajo → se invierte.
    float ang = FMath::Atan2(-D.Y, D.X); // [-pi, pi]
    if (ang < 0.f) ang += 2.f * PI;

    Hue = ang / (2.f * PI);
    Sat = r;
    RecomputeColor();
    return true;
}

void UPTColorPickerWidget::QuickPickTick()
{
    // Sigue el cursor real (posición absoluta de Slate, coherente con el geometry del wheel).
    if (FSlateApplication::IsInitialized())
        UpdateFromAbsolute(FSlateApplication::Get().GetCursorPos());
}

void UPTColorPickerWidget::QuickAdjustValue(float Delta)
{
    Val = FMath::Clamp(Val + Delta, 0.f, 1.f);
    if (ValueSlider) ValueSlider->SetValue(Val); // reflejar en el slider si existe
    RecomputeColor();
}

void UPTColorPickerWidget::RecomputeColor()
{
    CurrentColor = FLinearColor::MakeFromHSV8(
        (uint8)FMath::Clamp(Hue * 255.f, 0.f, 255.f),
        (uint8)FMath::Clamp(Sat * 255.f, 0.f, 255.f),
        (uint8)FMath::Clamp(Val * 255.f, 0.f, 255.f));
    RefreshUI();
}

void UPTColorPickerWidget::RefreshUI()
{
    if (HexText)       HexText->SetText(FText::FromString(ColorToHex(CurrentColor)));
    if (PreviewSwatch) PreviewSwatch->SetBrushColor(CurrentColor);
}

void UPTColorPickerWidget::SetColor(FLinearColor NewColor)
{
    CurrentColor = NewColor;
    // Descomponer a HSV para que la rueda y el slider queden coherentes.
    const FLinearColor HSV = NewColor.LinearRGBToHSV();
    Hue = HSV.R / 360.f;
    Sat = HSV.G;
    Val = HSV.B;
    if (ValueSlider) ValueSlider->SetValue(Val);
    RefreshUI();
}

void UPTColorPickerWidget::Confirm()
{
    if (APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(GetOwningPlayer()))
        PC->OnColorConfirmed(CurrentColor); // setea color, modo Paint y cierra
}

FReply UPTColorPickerWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (UpdateFromMouse(InMouseEvent))
    {
        bDragging = true;
        return FReply::Handled().CaptureMouse(TakeWidget());
    }
    return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UPTColorPickerWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        UpdateFromMouse(InMouseEvent);
        return FReply::Handled();
    }
    return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

FReply UPTColorPickerWidget::NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    if (bDragging)
    {
        bDragging = false;
        return FReply::Handled().ReleaseMouseCapture();
    }
    return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}
