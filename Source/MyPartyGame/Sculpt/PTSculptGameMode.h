// GameMode de la partida de Sculpturillo. Corre el loop por turnos (servidor manda):
// elige escultor al azar → 3 palabras → esculpe con reloj → rota. Loop infinito por ahora.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PTSculptGameMode.generated.h"

class APTPlayerState;
class APTSculptGameState;

UCLASS()
class MYPARTYGAME_API APTSculptGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    APTSculptGameMode();

    // ── Tuneables (editar en BP_SculptGameMode) ─────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 MinPlayersToStart = 2;
    UPROPERTY(EditDefaultsOnly, Category="Game") float TurnDuration      = 90.f; // seg de dibujo
    UPROPERTY(EditDefaultsOnly, Category="Game") float ChooseDuration    = 15.f; // seg para elegir palabra
    UPROPERTY(EditDefaultsOnly, Category="Game") float TurnEndDuration   = 5.f;  // pausa de reveal
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 WordChoiceCount   = 3;    // palabras ofrecidas

    // Banco de palabras (español). Si queda vacío se siembra con una lista por defecto.
    UPROPERTY(EditDefaultsOnly, Category="Game") TArray<FString> WordBank;

    // Llamado por el PlayerController del escultor cuando elige una de las 3 palabras.
    void HandleWordChosen(APTPlayerState* Chooser, int32 ChoiceIndex);

    // Llamado desde el sistema de chat (Fase 4) cuando alguien acierta.
    void HandlePlayerGuessedCorrectly(APTPlayerState* Guesser);

    // ¿El texto coincide con la palabra secreta del turno? (normaliza mayúsc./tildes).
    // Server-only: la palabra real nunca sale de acá.
    bool DoesGuessMatch(const FString& Guess) const;

protected:
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void HandleSeamlessTravelPlayer(AController*& C) override;
    virtual void Logout(AController* Exiting) override;

private:
    FTimerHandle PhaseTimer;

    // Estado del turno (solo servidor). La palabra real vive acá, jamás se replica.
    FString          CurrentWord;
    TArray<FString>  CurrentChoices;

    APTSculptGameState* GS() const;
    TArray<APTPlayerState*> GetActivePlayers() const;

    void CheckStart();          // arranca si hay suficientes jugadores
    void StartChoosingPhase();  // elige escultor + 3 palabras
    void AutoChooseWord();      // si el escultor no elige a tiempo
    void BeginDrawing(int32 ChoiceIndex);
    void EndTurn();             // revela y agenda el próximo turno
    void GoToWaiting();         // no hay suficientes jugadores

    static FString MakeMasked(const FString& Word);
    static FString Normalize(const FString& In); // minúsculas, sin tildes, trim
    void SeedDefaultWords();
};
