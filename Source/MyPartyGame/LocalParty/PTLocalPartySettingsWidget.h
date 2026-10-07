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
    virtual void NativeDestruct() override;

private:
    void BuildTree();
    void BuildStreamerSection(UVerticalBox* Col);
    void RefreshValues();
    // Corre por TIMER (no NativeTick): un widget colapsado no tickea, y el panel tiene que volver a
    // mostrarse solo al cerrar el banco de palabras / el Workshop o al volver a la espera.
    void UpdateState();
    void UpdateStreamerSection();
    bool IsRevealHeld() const;
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);
    UButton* MakeButton(const FText& Label, FName Name, int32 FontSize, UTextBlock** OutText = nullptr);
    USlider* AddSliderRow(UVerticalBox* Box, UTextBlock*& OutLabel, float Min, float Max, float Step, UTextBlock*& OutValue);

    UFUNCTION() void OnTimeChanged(float V);
    UFUNCTION() void OnRoundsChanged(float V);
    UFUNCTION() void OnRevealChanged(float V);
    UFUNCTION() void OnWordsClicked();
    UFUNCTION() void OnStartClicked();
    UFUNCTION() void OnToggleHostQr();
    UFUNCTION() void OnOpenHostOnPC();
    // Abre el modal de links (privado del streamer vs. público para el chat): nunca copia directo.
    UFUNCTION() void OnCopyHostLink();

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

    // ── Audiencia: conectar al streamer (QR PRIVADO en su propia zona fija, nunca en el QR público) ──
    UPROPERTY() UWidget*    StreamerSection = nullptr;
    UPROPERTY() UTextBlock* StreamerStatus = nullptr;
    UPROPERTY() UWidget*    PrivateZone = nullptr;    // recuadro marcado: acá (y solo acá) aparece el QR
    UPROPERTY() class UImage* PrivateQr = nullptr;
    UPROPERTY() UTextBlock* PrivateZoneHint = nullptr;
    UPROPERTY() UWidget*    StreamerButtons = nullptr;
    UPROPERTY() UTextBlock* ToggleQrText = nullptr;
    UPROPERTY() class UPTStreamerLinkModal* LinkModal = nullptr;
    UPROPERTY() class UTexture2D* PrivateQrTexture = nullptr;
    FString PrivateQrUrl;
    // Último título de banco escrito: solo se reescribe si cambia (si no, se pisa con la traducción
    // automática de PTText y el texto alterna "Películas" ↔ "Movies").
    FString ShownPackTitle = TEXT("?");
    bool    bHostQrShown = false;
    FTimerHandle StateTimer;
};
