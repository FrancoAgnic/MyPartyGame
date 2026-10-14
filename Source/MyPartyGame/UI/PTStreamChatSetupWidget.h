// Popup "Chat del stream" del panel de ajustes del host (modo clásico): switch on/off, canales de Twitch / Kick
// (los mismos del modo audiencia), estado de la conexión y cuántos del chat se unieron. Armado en C++ (sin WBP).
// Lo abre el botón "Chat del stream" de UPTGameSettingsWidget.

#pragma once
#include "CoreMinimal.h"
#include "PTUserWidget.h"
#include "../LocalParty/PTStreamChat.h"
#include "PTStreamChatSetupWidget.generated.h"

class UBorder;
class UButton;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;
class UPTStreamChatSubsystem;

DECLARE_MULTICAST_DELEGATE(FPTOnStreamChatSetupClosed);

UCLASS()
class MYPARTYGAME_API UPTStreamChatSetupWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Se dispara al cerrar (el panel de ajustes refresca la etiqueta de su botón). */
    FPTOnStreamChatSetupClosed OnClosed;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    void BuildTree();
    void Refresh();
    UPTStreamChatSubsystem* Sub() const;
    UTextBlock* MakeText(int32 Size, const FLinearColor& Color, bool bBold);
    UButton* MakeButton(const FText& Label, FName Name, UTextBlock** OutText);
    UEditableTextBox* AddChannelRow(UVerticalBox* Box, const FText& Platform, const FLinearColor& Color, FName Name,
                                    UTextBlock*& OutStatus);
    void SetStatus(UTextBlock* Status, EPTChatPlatform Platform) const;

    UFUNCTION() void OnToggleClicked();
    UFUNCTION() void OnDoneClicked();
    UFUNCTION() void OnTwitchCommitted(const FText& Text, ETextCommit::Type Method);
    UFUNCTION() void OnKickCommitted(const FText& Text, ETextCommit::Type Method);

    FTimerHandle RefreshTimer;
    UPROPERTY() UButton*          ToggleButton = nullptr;
    UPROPERTY() UTextBlock*       ToggleText   = nullptr;
    UPROPERTY() UEditableTextBox* TwitchInput  = nullptr;
    UPROPERTY() UEditableTextBox* KickInput    = nullptr;
    UPROPERTY() UTextBlock*       TwitchStatus = nullptr;
    UPROPERTY() UTextBlock*       KickStatus   = nullptr;
    UPROPERTY() UTextBlock*       ViewersText  = nullptr;
};
