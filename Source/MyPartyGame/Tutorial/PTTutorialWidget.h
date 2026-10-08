// Interfaz del tutorial de Sculpi, armada en C++ (sin WBP), como los paneles del modo local:
//  · cuadro de diálogo abajo: "SCULPI" + texto que se escribe letra por letra (con "bla bla"), las
//    teclas de la lección (PC o joystick) y la barra de progreso;
//  · panel de pausa (con el menú ESC abierto): "Saltar tutorial" y, en la última palabra, "¡Listo!";
//  · cuenta regresiva grande + flash para la foto;
//  · la foto final en un marco tipo polaroid ("Mi perro" + logo) con "Continuar".
// Lo maneja APTTutorialDirector.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTTutorialWidget.generated.h"

class UBorder;
class UButton;
class UImage;
class UProgressBar;
class UTextBlock;
class UVerticalBox;
class UWrapBox;
class USoundBase;
class UTexture2D;

DECLARE_MULTICAST_DELEGATE(FPTTutorialUIEvent);

/** Una tecla de la lección: íconos (teclas / botones del joystick) o, si no hay ícono, el texto. */
struct FPTTutHint
{
    TArray<UTexture2D*> Icons;
    FText Key;
    FText Label;
};

/** El marco de la foto (polaroid): se muestra en pantalla y se dibuja a una textura para Steam. */
UCLASS()
class MYPARTYGAME_API UPTTutorialPolaroid : public UPTUserWidget
{
    GENERATED_BODY()
public:
    void Setup(UTexture2D* Photo, const FText& Caption, UTexture2D* Logo, float PhotoWidth);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    void BuildTree();
    UPROPERTY() UImage*     PhotoImage = nullptr;
    UPROPERTY() UImage*     LogoImage = nullptr;
    UPROPERTY() UTextBlock* CaptionText = nullptr;
    UPROPERTY() class USizeBox* PhotoBox = nullptr;
};

UCLASS()
class MYPARTYGAME_API UPTTutorialWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Sculpi dice algo (se escribe letra por letra). */
    void Say(const FText& Text);
    bool IsTyping() const { return Revealed < FullText.Len(); }
    /** Termina de escribir de una (si el jugador ya hizo la lección mientras hablaba). */
    void FinishTyping();

    /** Teclas de la lección: pares {tecla, para qué}. Vacío = sin teclas. */
    void SetHints(const TArray<FPTTutHint>& Hints);
    /** Progreso 0..1 (negativo = ocultar la barra). Label = texto a la derecha ("Iglú 64%"). */
    void SetProgress(float Fraction, const FText& Label);
    void SetDialogVisible(bool bVisible);
    /** Menú de pausa abierto: mostrar "Saltar tutorial" (y "¡Listo!" si bShowDone). */
    void SetPauseOptions(bool bShow, bool bShowDone);
    void ShowCountdown(int32 N); // 0 = ocultar
    void Flash();
    /** Muestra la foto en el marco + mensaje (dónde quedó guardada) + "Continuar". */
    void ShowPhoto(UTexture2D* Photo, const FText& Caption, const FText& SavedMsg);
    void HidePhoto();
    bool IsPhotoShown() const;

    /** Sonido de la "voz" (se elige uno al azar por sílaba). */
    UPROPERTY(EditAnywhere, Category="Tutorial") TArray<TSoftObjectPtr<USoundBase>> VoiceSounds;
    UPROPERTY(EditAnywhere, Category="Tutorial") TSoftObjectPtr<UTexture2D> LogoTexture;
    UPROPERTY(EditAnywhere, Category="Tutorial") float CharsPerSecond = 42.f;

    FPTTutorialUIEvent OnSkipClicked;
    FPTTutorialUIEvent OnDoneClicked;
    FPTTutorialUIEvent OnContinueClicked;

    /** La polaroid de la foto (para dibujarla a textura). */
    UPTTutorialPolaroid* GetPolaroid() const { return Polaroid; }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildTree();
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);
    UButton* MakeButton(const FText& Label, FName Name, int32 FontSize, const FLinearColor& Fill);
    void PlayVoice();

    UFUNCTION() void HandleSkip();
    UFUNCTION() void HandleDone();
    UFUNCTION() void HandleContinue();

    UPROPERTY() UWidget*      DialogCard = nullptr;
    UPROPERTY() UTextBlock*   BodyText = nullptr;
    UPROPERTY() UWrapBox*     HintsBox = nullptr;
    UPROPERTY() UWidget*      ProgressRow = nullptr;
    UPROPERTY() UProgressBar* Progress = nullptr;
    UPROPERTY() UTextBlock*   ProgressLabel = nullptr;
    UPROPERTY() UWidget*      PausePanel = nullptr;
    UPROPERTY() UButton*      DoneButton = nullptr;
    UPROPERTY() UTextBlock*   CountdownText = nullptr;
    UPROPERTY() UBorder*      FlashOverlay = nullptr;
    UPROPERTY() UWidget*      PhotoPanel = nullptr;
    UPROPERTY() UPTTutorialPolaroid* Polaroid = nullptr;
    UPROPERTY() UTextBlock*   SavedText = nullptr;
    UPROPERTY() UTexture2D*   LoadedLogo = nullptr;

    FString FullText;
    float   Revealed = 0.f;     // caracteres mostrados (fraccionario)
    int32   ShownChars = -1;
    int32   VoiceCounter = 0;
    double  LastVoiceTime = 0.0;
    float   FlashAlpha = 0.f;
    FString HintsSig;
};
