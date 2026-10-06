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

private:
    void BuildTree();
    void RefreshValues();
    void ApplyToControllers();
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);
    UButton* MakeButton(const FText& Label, FName Name, UTextBlock** OutText = nullptr);
    USlider* AddSliderRow(UVerticalBox* Box, const FText& Label, float Min, float Max, float Step, UTextBlock*& OutValue);

    UFUNCTION() void OnLookChanged(float V);
    UFUNCTION() void OnMoveChanged(float V);
    UFUNCTION() void OnDeadZoneChanged(float V);
    UFUNCTION() void OnInvertClicked();
    UFUNCTION() void OnResetClicked();
    UFUNCTION() void OnBackClicked();

    UPROPERTY() USlider*    LookSlider = nullptr;
    UPROPERTY() USlider*    MoveSlider = nullptr;
    UPROPERTY() USlider*    DeadSlider = nullptr;
    UPROPERTY() UTextBlock* LookValue = nullptr;
    UPROPERTY() UTextBlock* MoveValue = nullptr;
    UPROPERTY() UTextBlock* DeadValue = nullptr;
    UPROPERTY() UTextBlock* InvertText = nullptr;
    UPROPERTY() TMap<FName, UTextBlock*> KeyTexts;
    UPROPERTY() TArray<UPTGamepadRebindHandler*> Handlers;

    FName RebindingId;
    bool  bHadCursor = true;
};
