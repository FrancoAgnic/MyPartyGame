// GameMode de la partida de Sculpturillo. Corre el loop por turnos (servidor manda):
// elige escultor al azar → 3 palabras → esculpe con reloj → rota. Loop infinito por ahora.

#pragma once
#include "CoreMinimal.h"
#include "../Lobby/PTLobbyGameMode.h"
#include "PTSculptGameMode.generated.h"

class APTPlayerState;
class APTSculptGameState;
class APTSculptVolume;

// Hereda de APTLobbyGameMode (AGameMode) para reusar su posesión/pawn/reconexión que
// SÍ funciona; solo cambia el PlayerController, el GameState y agrega el loop por turnos.
UCLASS()
class MYPARTYGAME_API APTSculptGameMode : public APTLobbyGameMode
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

    // ── Puntajes y rondas (estilo Skribbl) ──────────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 NumRounds          = 3;   // rondas antes de terminar
    // Puntos del que adivina: interpola de Max (adivina al instante) a Min (sobre la hora).
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 MaxGuessPoints     = 100;
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 MinGuessPoints     = 25;
    // El escultor gana esto por cada jugador que adivina su escultura.
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 SculptorPointsPerGuess = 25;
    // Mapa del lobby (para "Volver al lobby" al terminar la partida).
    UPROPERTY(EditDefaultsOnly, Category="Game") FString LobbyMapPath = TEXT("/Game/Template/levels/Lobby");

    // Banco de palabras (español). Si queda vacío se siembra con una lista por defecto.
    UPROPERTY(EditDefaultsOnly, Category="Game") TArray<FString> WordBank;

    // Llamado por el PlayerController del escultor cuando elige una de las 3 palabras.
    void HandleWordChosen(APTPlayerState* Chooser, int32 ChoiceIndex);

    // Llamado desde el sistema de chat (Fase 4) cuando alguien acierta.
    void HandlePlayerGuessedCorrectly(APTPlayerState* Guesser);

    // Procesa un mensaje de chat en el servidor: detecta aciertos (anti-spoiler) y
    // difunde las líneas visibles a todos. Llamado por Server_SendChat del PC.
    void HandleChat(APTPlayerState* Sender, const FString& Message);

    // ¿El texto coincide con la palabra secreta del turno? (normaliza mayúsc./tildes).
    // Server-only: la palabra real nunca sale de acá.
    bool DoesGuessMatch(const FString& Guess) const;

    // Al terminar la partida (fase GameOver), el HUD del anfitrión llama a una de estas
    // vía el PlayerController. Solo el host puede; se ignora si lo pide otro.
    void RequestPlayAgain(APTPlayerState* Requester);
    void RequestReturnToLobby(APTPlayerState* Requester);

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
    int32            TurnsLeftThisRound = 0; // turnos que faltan para cerrar la ronda actual

    APTSculptGameState* GS() const;
    TArray<APTPlayerState*> GetActivePlayers() const;
    void ResetSculpture(); // limpia el Volume en todos (Multicast) al empezar el turno

    void CheckStart();          // arranca si hay suficientes jugadores
    void StartGame();           // resetea puntajes/rondas y arranca el primer turno
    void StartChoosingPhase();  // elige escultor + 3 palabras
    void AutoChooseWord();      // si el escultor no elige a tiempo
    void BeginDrawing(int32 ChoiceIndex);
    void EndTurn();             // revela y agenda el próximo turno
    void AdvanceTurn();         // avanza ronda/turno o termina la partida
    void EndGame();             // fase GameOver: anuncia ganador y espera decisión del host
    void AwardGuessPoints(APTPlayerState* Guesser); // suma puntos al que adivina + al escultor
    void GoToWaiting();         // no hay suficientes jugadores

    static FString MakeMasked(const FString& Word);
    static FString Normalize(const FString& In); // minúsculas, sin tildes, trim
    void SeedDefaultWords();
};
