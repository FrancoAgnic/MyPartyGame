// Modal "Links de la partida" del modo AUDIENCIA (lo abre "Copiar link" del panel de configuración).
// Separa el link PRIVADO del streamer (?h=CLAVE, nunca al chat) del link PÚBLICO para la audiencia,
// con un aviso bien visible. Armado en C++ (sin WBP), va encima de todo y se maneja con mouse o
// joystick (UPTGamepadUINavigator: el botón "CloseLinkModalButton" es el que cierra con B).
// Nunca muestra el link privado escrito: solo lo copia al portapapeles.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "PTStreamerLinkModal.generated.h"

class UBorder;
class UButton;
class UTextBlock;

UCLASS()
class MYPARTYGAME_API UPTStreamerLinkModal : public UPTUserWidget
{
    GENERATED_BODY()

public:
    void Open();
    void Close();
    bool IsOpen() const { return IsInViewport(); }

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    void BuildTree();
    void RefreshLabels();
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);
    UButton* MakeButton(const FText& Label, FName Name, const FLinearColor& Fill, UTextBlock** OutText);

    UFUNCTION() void OnCopyPrivate();
    UFUNCTION() void OnCopyChat();
    UFUNCTION() void OnCloseClicked();

    UPROPERTY() UBorder*    Card = nullptr;
    UPROPERTY() UTextBlock* PrivateText = nullptr;
    UPROPERTY() UTextBlock* ChatText = nullptr;
    UPROPERTY() UTextBlock* ChatUrlText = nullptr;

    double PrivateCopiedUntil = 0.0;
    double ChatCopiedUntil = 0.0;
    // Último estado escrito: los textos se reescriben solo si cambian (PTLocalizationSubsystem los
    // re-traduce en pantalla y reescribirlos cada tick los hace parpadear).
    int32  ShownState = -1;
    FString ShownChatUrl = TEXT("?");
};
