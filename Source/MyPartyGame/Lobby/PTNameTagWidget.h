// Cartel con el nombre del jugador (sobre la cabeza). Toda la lógica en C++: el
// personaje le pasa el DisplayName. El WBP solo aporta el TextBlock "NameText".

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTNameTagWidget.generated.h"

class UTextBlock;
class UImage;
class UBorder;

UCLASS()
class MYPARTYGAME_API UPTNameTagWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Setea el texto del cartel (nombre, color original). Máximo 10 chars. */
    void SetPlayerName(const FString& InName);

    /** Muestra/oculta la corona de host (misma idea que la lista de jugadores). */
    void SetHost(bool bIsHost);

    /** Globo de chat normal (color original, tope 40 chars). */
    void ShowMessage(const FString& Msg);

    /** Globo de acierto: texto en VERDE (tope 40 chars). */
    void ShowGuessMessage(const FString& Msg);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* NameText;
    // Corona del host (opcional): nombrala EXACTO "HostCrown" en el WBP_NameTag. Solo visible si sos host.
    UPROPERTY(meta = (BindWidgetOptional)) UImage* HostCrown;
    // Marco del cartel (el UBorder llamado "Border" en WBP_NameTag). Se tiñe de verde (mismo
    // verde del acierto) para el look del lobby. Opcional: si el WBP no lo tiene, no pasa nada.
    UPROPERTY(meta = (BindWidgetOptional)) UBorder* Border;

    // Verde del cartel = el mismo que usa el globo de acierto (ShowGuessMessage).
    static const FLinearColor BorderGreen;

    // Color original del NameText (del WBP), para restaurarlo tras un globo verde.
    FLinearColor DefaultColor = FLinearColor::White;
};
