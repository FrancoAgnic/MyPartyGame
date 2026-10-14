// Pestañas del chat en el modo clásico con CHAT DEL STREAM: "Juego · Shift+1" y "Twitch · Shift+2", con
// contador de mensajes sin leer en la que no está activa. Armado en C++ (sin WBP). Lo crea y lo ubica el HUD
// de la partida justo encima del panel del chat (sea cual sea el layout del WBP).

#pragma once
#include "CoreMinimal.h"
#include "../UI/PTUserWidget.h"
#include "PTChatTabsWidget.generated.h"

class UBorder;
class UTextBlock;

UCLASS()
class MYPARTYGAME_API UPTChatTabsWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Qué pestaña está activa, los no leídos de cada una y el nombre de la del stream ("Twitch", "Kick"...). */
    void SetState(bool bStreamActive, int32 UnreadGame, int32 UnreadStream, const FText& StreamLabel);

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    void BuildTree();
    UBorder* MakePill(UTextBlock*& OutText);
    void StylePill(UBorder* Pill, UTextBlock* Text, bool bActive, const FText& Label) const;

    UPROPERTY() UBorder*    GamePill   = nullptr;
    UPROPERTY() UTextBlock* GameText   = nullptr;
    UPROPERTY() UBorder*    StreamPill = nullptr;
    UPROPERTY() UTextBlock* StreamText = nullptr;
};
