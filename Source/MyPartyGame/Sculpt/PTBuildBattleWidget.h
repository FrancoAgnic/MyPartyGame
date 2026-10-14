// Overlay del modo BUILD BATTLE (prototipo). Armado en C++ (sin WBP), como los otros paneles del juego.
// Lo crea el PlayerController cuando la partida es Build Battle y se maneja solo leyendo el GameState:
//  · Esculpiendo (Drawing): cartel de presentación del modo los primeros segundos de cada ronda.
//  · Exhibición (Showcase): "Escultura 2 de 5 · 12 s" + las 5 opciones de voto (teclas 1-5), marcando la tuya.
//  · Resultados (RoundResults): tabla de la ronda (promedio, votos y puntos), la ganadora primero.
// La palabra y el reloj de esculpido ya los muestra el HUD de siempre.

#pragma once
#include "CoreMinimal.h"
#include "../UI/PTUserWidget.h"
#include "PTSculptGameState.h"
#include "PTBuildBattleWidget.generated.h"

class UBorder;
class UTextBlock;
class UVerticalBox;

UCLASS()
class MYPARTYGAME_API UPTBuildBattleWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    // Segundos que se ve el cartel de presentación al empezar cada ronda.
    UPROPERTY(EditAnywhere, Category="BuildBattle") float IntroSeconds = 7.f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

private:
    void BuildTree();
    void Refresh(); // 10 Hz por timer (no depende de NativeTick)

    UTextBlock* MakeText(int32 Size, bool bBold, const FLinearColor& Color);
    UBorder*    MakeCard(UWidget* Content);
    void        RefreshVoteCard(const APTSculptGameState* G);
    void        RefreshResultsCard(const APTSculptGameState* G);

    FTimerHandle RefreshTimer;

    // Presentación del modo.
    UPROPERTY() UBorder*    IntroCard  = nullptr;
    UPROPERTY() UTextBlock* IntroBody  = nullptr;
    UPROPERTY() UTextBlock* IntroWord  = nullptr;

    // Votación.
    UPROPERTY() UBorder*    VoteCard   = nullptr;
    UPROPERTY() UTextBlock* VoteHeader = nullptr;
    UPROPERTY() UTextBlock* VotePrompt = nullptr;
    UPROPERTY() TArray<UBorder*>    VoteChips;
    UPROPERTY() TArray<UTextBlock*> VoteChipNums;
    UPROPERTY() TArray<UTextBlock*> VoteChipLabels;

    // Resultados.
    UPROPERTY() UBorder*      ResultsCard   = nullptr;
    UPROPERTY() UTextBlock*   ResultsTitle  = nullptr;
    UPROPERTY() UTextBlock*   ResultsWinner = nullptr;
    UPROPERTY() UVerticalBox* ResultsList   = nullptr;
    UPROPERTY() UTextBlock*   ResultsFooter = nullptr;
    FString CachedResultsSig; // la tabla solo se rearma si cambió

    EPTTurnPhase LastPhase      = EPTTurnPhase::WaitingForPlayers;
    int32        LastRound      = -1;
    double       DrawingStarted = -1.0; // tiempo local en que empezó a esculpirse la ronda actual
};
