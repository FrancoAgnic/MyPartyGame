// Panel "Joystick": sensibilidad, velocidad, zona muerta, invertir Y y reasignar cada acción.
// Armado en C++ (no necesita WBP). Se abre desde Ajustes (botón GamepadButton, o Y con el joystick)
// o con el comando de consola "PTGamepad". Todo se guarda en UPTGameUserSettings.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTGamepadSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class USlider;
class UTextBlock;
class UVerticalBox;
class UPTGamepadSettingsWidget;

// Pestañas del panel "Controles".
UENUM()
enum class EPTControlsTab : uint8 { Keyboard, Gamepad };

// Un handler por fila de botón (los delegates dinámicos no llevan parámetros extra).
UCLASS()
class UPTGamepadRebindHandler : public UObject
{
    GENERATED_BODY()
public:
    FName ActionId;
    TWeakObjectPtr<UPTGamepadSettingsWidget> Owner;
    UFUNCTION() void HandleClicked();
};

UCLASS()
class MYPARTYGAME_API UPTGamepadSettingsWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    void StartRebind(FName ActionId);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    // Captura de TECLADO/RATÓN para reasignar: se hace en el propio widget (que tiene foco), no por el
    // preprocessor del joystick (que no entrega las teclas del teclado en este estado de UI).
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& G, const FKeyEvent& E) override;
    virtual FReply NativeOnPreviewMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override;

private:
    void BuildTree();
    void ShowTab(EPTControlsTab Tab);      // reconstruye el cuerpo según la pestaña
    void BuildGamepadBody(UVerticalBox* Body);  // joystick: sensibilidades + botones
    void BuildKeyboardBody(UVerticalBox* Body);  // teclado + ratón: lista de teclas
    void ApplyTabVisual();
    void RefreshValues();
    void ApplyToControllers();         // rebindea el joystick en vivo
    void ApplyKeyboardToControllers(); // rebindea el teclado/ratón en vivo
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);
    UButton* MakeButton(const FText& Label, FName Name, UTextBlock** OutText = nullptr);
    USlider* AddSliderRow(UVerticalBox* Box, const FText& Label, float Min, float Max, float Step, UTextBlock*& OutValue);

    UFUNCTION() void OnLookChanged(float V);
    UFUNCTION() void OnMoveChanged(float V);
    UFUNCTION() void OnDeadZoneChanged(float V);
    UFUNCTION() void OnInvertClicked();
    UFUNCTION() void OnResetClicked();
    UFUNCTION() void OnBackClicked();
    UFUNCTION() void OnKbTabClicked();
    UFUNCTION() void OnPadTabClicked();
    UFUNCTION() void OnMouseSensChanged(float V); // sensibilidad de cámara con mouse (pestaña Teclado)

    EPTControlsTab ActiveTab = EPTControlsTab::Keyboard;

    UPROPERTY() UVerticalBox* BodyBox   = nullptr; // contenedor del cuerpo (se rearma por pestaña)
    UPROPERTY() UButton*    KbTabButton  = nullptr;
    UPROPERTY() UButton*    PadTabButton = nullptr;
    UPROPERTY() USlider*    LookSlider = nullptr;
    UPROPERTY() USlider*    MoveSlider = nullptr;
    UPROPERTY() USlider*    DeadSlider = nullptr;
    UPROPERTY() USlider*    MouseSensSlider = nullptr; // sensibilidad de cámara con mouse (pestaña Teclado)
    UPROPERTY() UTextBlock* MouseSensValue  = nullptr;
    UPROPERTY() UTextBlock* LookValue = nullptr;
    UPROPERTY() UTextBlock* MoveValue = nullptr;
    UPROPERTY() UTextBlock* DeadValue = nullptr;
    UPROPERTY() UTextBlock* InvertText = nullptr;
    UPROPERTY() TMap<FName, UTextBlock*> KeyTexts;
    UPROPERTY() TArray<UPTGamepadRebindHandler*> Handlers;

    FName RebindingId;
    bool  bHadCursor = true;

    // Captura de teclado/ratón en curso (pestaña Teclado): el próximo key/click se asigna a KbCaptureId.
    bool  bCapturingKb = false;
    FName KbCaptureId;
    void  BeginKeyboardCapture(FName ActionId);
    bool  FinishKeyboardCapture(const FKey& Key); // true si consumió el evento
};
