// HUD de la partida de Sculpturillo. Toda la lógica vive acá (C++): estado por fase,
// reloj, elección de palabra, chat con anti-spoiler y modo de input por fase.
// El WBP_GameplayHUD solo aporta los widgets (con estos nombres) y el layout.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTSculptGameState.h" // EPTTurnPhase, EPTChatType
#include "PTGameplayHUDWidget.generated.h"

class UTextBlock;
class UButton;
class UEditableTextBox;
class UScrollBox;
class APTSculptPlayerController;

UCLASS()
class MYPARTYGAME_API UPTGameplayHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Llamar desde el PlayerController al crear el HUD (mismo patrón que el lobby). */
    UFUNCTION(BlueprintCallable, Category="Game")
    void ShowHUD();

    /** Abre/enfoca el chat (lo llama el PlayerController con Enter). Muestra cursor y
     *  da foco al input; al enviar vuelve solo a Game Only. */
    void FocusChat();

protected:
    virtual bool Initialize() override;
    virtual void NativeDestruct() override;

    // ── Widgets del WBP (todos opcionales: el WBP compila aunque falte alguno) ──
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtSculptor;   // "X está esculpiendo"
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtWord;       // "_ _ _ _" o la palabra
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtTimer;      // segundos restantes
    UPROPERTY(meta=(BindWidgetOptional)) UWidget*         WordPickPanel; // panel de las 3 palabras
    UPROPERTY(meta=(BindWidgetOptional)) UButton*         BtnWord0;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*         BtnWord1;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*         BtnWord2;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtWord0;      // texto dentro de BtnWord0
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtWord1;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtWord2;
    UPROPERTY(meta=(BindWidgetOptional)) UScrollBox*          ChatScroll;
    // RichTextBlock: el nombre usa el estilo "name" (color); el mensaje queda en el default.
    UPROPERTY(meta=(BindWidgetOptional)) class URichTextBlock* TxtChat;    // log de chat (Auto Wrap)
    UPROPERTY(meta=(BindWidgetOptional)) UEditableTextBox* ChatInput;

    // ── Rondas + marcador + pantalla de fin (todos opcionales) ──
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtRound;       // "Ronda 2 / 3"
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtScoreboard;  // marcador en vivo (multilínea)
    UPROPERTY(meta=(BindWidgetOptional)) UWidget*    ResultsPanel;   // se muestra solo en GameOver
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtResults;     // ranking final (multilínea)
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    BtnPlayAgain;   // solo host
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    BtnReturnLobby; // solo host

    UFUNCTION() void OnBtnWord0();
    UFUNCTION() void OnBtnWord1();
    UFUNCTION() void OnBtnWord2();
    UFUNCTION() void OnChatCommitted(const FText& Text, ETextCommit::Type CommitMethod);
    UFUNCTION() void OnChatLine(const FString& Name, const FString& Message, EPTChatType Type);
    UFUNCTION() void OnBtnPlayAgain();
    UFUNCTION() void OnBtnReturnLobby();

private:
    FTimerHandle RefreshTimer;
    FString      ChatLog;
    bool         bChatBound     = false;
    bool         bChatOpen      = false;
    bool         bInputModeInit = false;
    bool         bWantsGameOnly = false;

    APTSculptGameState*        GetGS() const;
    APTSculptPlayerController* GetSculptPC() const;

    void RefreshTick();          // polling: pinta estado/reloj/panel/input según la fase
    void ChooseWord(int32 Index);
    void ApplyInputMode(bool bGameOnly);
    FString BuildScoreboard() const; // "Nombre: pts" ordenado por puntaje desc.
    bool    IsLocalPlayerHost() const;
};
