// Popup de "elegí la palabra" para el STREAMER (modo audiencia). Aparece en la pantalla de juego
// durante la fase de elegir palabra (ChoosingWord) con un contador y un botón "Seleccionar palabra"
// que abre la página PRIVADA del streamer en esta PC (donde ve y elige, off-stream). No muestra ninguna
// palabra ni código, así que es seguro que se vea en el stream.
//
// Se crea en modo audiencia (ver APTSculptPlayerController::CreateLocalPartyTV) y se auto-muestra/oculta
// solo, mirando el GameState cada frame. No toca la lógica del juego.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTStreamerChoosePopup.generated.h"

class UTextBlock;
class UButton;
class UWidget;

UCLASS()
class MYPARTYGAME_API UPTStreamerChoosePopup : public UUserWidget
{
    GENERATED_BODY()

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UFUNCTION() void OnSelectClicked(); // abre la página privada en esta PC

    UPROPERTY() UWidget*    RootCard  = nullptr; // se colapsa cuando no es el momento de elegir
    UPROPERTY() UTextBlock* TimerText = nullptr;
};
