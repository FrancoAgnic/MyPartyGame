// Panel de CONFIGURACIÓN de la partida en la TV (modos local y audiencia), mientras se espera a los
// jugadores: tiempo por turno, rondas (palabras en audiencia), letras reveladas, banco de palabras y
// "Empezar partida". Armado en C++ (sin WBP); se maneja con mouse o joystick (UPTGamepadUINavigator).
// Escribe en GI->PendingMatchSettings, que el GameMode aplica al empezar cada partida.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTLocalPartySettingsWidget.generated.h"

class UButton;
class USlider;
class UTextBlock;
class UVerticalBox;

UCLASS()
class MYPARTYGAME_API UPTLocalPartySettingsWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** WBP del selector de bancos de palabras (el mismo del lobby). */
    UPROPERTY(EditAnywhere, Category="LocalParty")
    FSoftClassPath WordPackClass = FSoftClassPath(TEXT("/Game/Template/WidgetBlueprints/WBP_WordPack.WBP_WordPack_C"));

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildTree();
    void RefreshValues();
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);
    UButton* MakeButton(const FText& Label, FName Name, int32 FontSize, UTextBlock** OutText = nullptr);
    USlider* AddSliderRow(UVerticalBox* Box, UTextBlock*& OutLabel, float Min, float Max, float Step, UTextBlock*& OutValue);

    UFUNCTION() void OnTimeChanged(float V);
    UFUNCTION() void OnRoundsChanged(float V);
    UFUNCTION() void OnRevealChanged(float V);
    UFUNCTION() void OnWordsClicked();
    UFUNCTION() void OnStartClicked();

    UPROPERTY() USlider*    TimeSlider = nullptr;
    UPROPERTY() USlider*    RoundsSlider = nullptr;
    UPROPERTY() USlider*    RevealSlider = nullptr;
    UPROPERTY() UTextBlock* TimeLabel = nullptr;
    UPROPERTY() UTextBlock* RoundsLabel = nullptr;
    UPROPERTY() UTextBlock* RevealLabel = nullptr;
    UPROPERTY() UTextBlock* TimeValue = nullptr;
    UPROPERTY() UTextBlock* RoundsValue = nullptr;
    UPROPERTY() UTextBlock* RevealValue = nullptr;
    UPROPERTY() UTextBlock* WordsValue = nullptr;
    UPROPERTY() UButton*    StartButton = nullptr;
    UPROPERTY() UTextBlock* StartHint = nullptr;
    UPROPERTY() class UPTWordPackWidget* WordPack = nullptr;

    float RefreshAccum = 1.f;
};
