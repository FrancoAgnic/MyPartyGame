// Cartel con el nombre del jugador (sobre la cabeza). Toda la lógica en C++: el
// personaje le pasa el DisplayName. El WBP solo aporta el TextBlock "NameText".

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTNameTagWidget.generated.h"

class UTextBlock;

UCLASS()
class MYPARTYGAME_API UPTNameTagWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Setea el texto del cartel (nombre, color original). Máximo 10 chars. */
    void SetPlayerName(const FString& InName);

    /** Globo de chat normal (color original, tope 40 chars). */
    void ShowMessage(const FString& Msg);

    /** Globo de acierto: texto en VERDE (tope 40 chars). */
    void ShowGuessMessage(const FString& Msg);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* NameText;

    // Color original del NameText (del WBP), para restaurarlo tras un globo verde.
    FLinearColor DefaultColor = FLinearColor::White;
};
