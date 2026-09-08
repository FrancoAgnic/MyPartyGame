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

    /** Cambia el marco del cartel según el estado de listo del jugador:
     *  listo → ReadyBrush (tu marco/material verde); no listo → el marco original del WBP. */
    void SetReadyState(bool bReady);

protected:
    virtual bool Initialize() override;

    UPROPERTY(meta = (BindWidgetOptional)) UTextBlock* NameText;
    // Corona del host (opcional): nombrala EXACTO "HostCrown" en el WBP_NameTag. Solo visible si sos host.
    UPROPERTY(meta = (BindWidgetOptional)) UImage* HostCrown;
    // Marco del cartel: el UBorder llamado EXACTO "BorderNameTag" en el WBP_NameTag.
    UPROPERTY(meta = (BindWidgetOptional)) UBorder* BorderNameTag;

    // Marco a usar cuando el jugador está LISTO. Asignalo en el WBP_NameTag (Class Defaults →
    // categoría "NameTag" → Ready Brush) con TU marco/material verde (Image = tu material o textura).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "NameTag") FSlateBrush ReadyBrush;

    // Marco original (no listo), capturado del WBP en Initialize para poder restaurarlo.
    FSlateBrush DefaultBrush;
    bool bDefaultBrushCaptured = false;

    // Color original del NameText (del WBP), para restaurarlo tras un globo verde.
    FLinearColor DefaultColor = FLinearColor::White;
};
