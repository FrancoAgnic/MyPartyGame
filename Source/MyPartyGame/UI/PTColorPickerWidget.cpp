#include "PTColorPickerWidget.h"
#include "../PTTextTable.h"
#include "PTColorRingWidget.h"
#include "Components/Image.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Misc/ConfigCacheIni.h"
#include "Framework/Application/SlateApplication.h"
#include "../Sculpt/PTSculptPlayerController.h"
#include "../PTGameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Engine/World.h"

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
    if (SaveHintText)
        SaveHintText->SetText(PTText::Get(TEXT("PICKER_SAVE_COLOR")));

    // Cargar y mostrar la paleta guardada en el anillo.
    LoadPalette();
    RefreshRing();

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

void UPTColorPickerWidget::RefreshRing()
{
    if (SwatchRing) SwatchRing->SetColors(Palette);
}

void UPTColorPickerWidget::PlayPickSound()
{
    UWorld* W = GetWorld();
    if (!W) return;
    const float Now = W->GetTimeSeconds();
    if (Now - LastPickSoundTime < 0.06f) return; // dedupe: confirmar auto-guarda → no sonar dos veces
    LastPickSoundTime = Now;
    if (UPTGameInstance* Ins = Cast<UPTGameInstance>(W->GetGameInstance()))
        if (Ins->SndColorPick) UGameplayStatics::PlaySound2D(this, Ins->SndColorPick);
}

void UPTColorPickerWidget::SaveCurrentColor()
{
    PlayPickSound();

    // Dedupe: si el color ya está (comparando en bytes sRGB, que es como se guarda), quitar la copia
    // vieja para que no se acumulen repetidos y el color recién usado quede como el más nuevo.
    const FColor Cur = CurrentColor.ToFColor(true);
    Palette.RemoveAll([Cur](const FLinearColor& C){ return C.ToFColor(true) == Cur; });

    Palette.Add(CurrentColor);
    // Buffer rotativo: si se pasa del máximo, cae el más viejo (el primero).
    while (Palette.Num() > MaxSwatches) Palette.RemoveAt(0);
    SavePalette();

    // Actualizar el anillo con la paleta nueva.
    RefreshRing();
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
    if (!FSlateApplication::IsInitialized()) return;
    const FVector2D CursorPos = FSlateApplication::Get().GetCursorPos();

    // Si el cursor está sobre un segmento del anillo: resaltarlo y mostrar ESE color
    // guardado en la brocha. Si no, muestrear la rueda (color más cercano).
    const int32 Seg = SwatchRing ? SwatchRing->SegmentAt(CursorPos) : -1;
    if (SwatchRing) SwatchRing->SetHovered(Seg);

    if (Seg >= 0 && Palette.IsValidIndex(Seg))
        SetColor(Palette[Seg]);
    else
        UpdateFromAbsolute(CursorPos);

    PushLiveColorToPC(); // color en vivo en la brocha
}

void UPTColorPickerWidget::QuickAdjustValue(float Delta)
{
    Val = FMath::Clamp(Val + Delta, 0.f, 1.f);
    if (ValueSlider) ValueSlider->SetValue(Val); // reflejar en el slider si existe
    RecomputeColor();
    PushLiveColorToPC(); // color en vivo en la brocha
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
    if (PreviewSwatch) PreviewSwatch->SetBrushColor(CurrentColor);
}

void UPTColorPickerWidget::PushLiveColorToPC()
{
    // Aplica el color a la brocha en tiempo real mientras arrastrás (sin cambiar de modo;
    // el cambio de modo Erase→Paint recién ocurre al soltar, en OnColorConfirmed).
    if (APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(GetOwningPlayer()))
        PC->CurrentPaintColor = CurrentColor;
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
        PC->OnColorConfirmed(CurrentColor); // aplica el color y cierra
}

void UPTColorPickerWidget::ConfirmQuickPick()
{
    PlayPickSound(); // sonido al confirmar/elegir color
    const FVector2D CursorPos = FSlateApplication::IsInitialized()
        ? FSlateApplication::Get().GetCursorPos() : FVector2D::ZeroVector;

    // ¿Soltó sobre un segmento del anillo? → elegir ese color guardado (ya está en la paleta: no re-guardar).
    FLinearColor Picked;
    if (SwatchRing && SwatchRing->GetColorAt(CursorPos, Picked))
    {
        SetColor(Picked);
        Confirm();
        return;
    }

    // Rueda: elegir un color NUEVO lo auto-guarda en el radial (con dedupe). Antes había que apretar E.
    SaveCurrentColor();
    Confirm();
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
