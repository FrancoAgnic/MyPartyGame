// Zona de la CÁMARA del streamer (modo clásico con chat del stream). Arriba a la derecha, tamaño fijo: el
// streamer pone ahí su cámara en el stream y tapa lo que solo él tiene que ver, así su chat (que juega) no lo ve:
//   · cuando no hay nada que tapar: "tu cámara va acá" (tenue; siempre el mismo lugar y tamaño);
//   · cuando le toca esculpir: las 3 palabras para elegir y después la palabra elegida;
//   · cuando adivina: la palabra que adivinó;
//   · mientras escribe en el chat: lo que está escribiendo (en la caja del chat salen puntitos).
// Armado en C++ (sin WBP). Lo crea y lo maneja el HUD de la partida.

#pragma once
#include "CoreMinimal.h"
#include "../UI/PTUserWidget.h"
#include "PTStreamerZoneWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;
class UVerticalBox;

DECLARE_DELEGATE_OneParam(FPTOnStreamerZoneChoice, int32);

UCLASS()
class MYPARTYGAME_API UPTStreamerZoneWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    // Tamaño de la zona (unidades de UI a 1080p) y margen a la esquina. Fijo, para que la cámara la tape siempre.
    UPROPERTY(EditAnywhere, Category="Streamer") FVector2D ZoneSize = FVector2D(380.f, 250.f);
    UPROPERTY(EditAnywhere, Category="Streamer") float     ZoneMargin = 12.f;

    /** Elegió una de las palabras (índice 0..2). */
    FPTOnStreamerZoneChoice OnChoose;

    void ShowPlaceholder();
    void ShowChoices(const TArray<FString>& Choices, int32 SecondsLeft);
    void ShowWord(const FText& Title, const FString& Word);
    void HideZone();
    /** Lo que el streamer está escribiendo en el chat (vacío = no está escribiendo). */
    void SetTyping(const FString& Text);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;

private:
    void BuildTree();
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);
    void SetTextIfChanged(UTextBlock* T, const FText& Text);

    UFUNCTION() void OnChoice0();
    UFUNCTION() void OnChoice1();
    UFUNCTION() void OnChoice2();

    UPROPERTY() UBorder*      Card       = nullptr;
    UPROPERTY() UTextBlock*   TagText    = nullptr;
    UPROPERTY() UTextBlock*   TitleText  = nullptr;
    UPROPERTY() UTextBlock*   WordText   = nullptr;
    UPROPERTY() UTextBlock*   TypingText = nullptr;
    UPROPERTY() UVerticalBox* ChoiceBox  = nullptr;
    UPROPERTY() TArray<UButton*>    ChoiceButtons;
    UPROPERTY() TArray<UTextBlock*> ChoiceTexts;
};
