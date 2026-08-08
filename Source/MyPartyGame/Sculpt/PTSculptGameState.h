// Estado replicado de la partida de Sculpturillo (fase del turno, escultor actual,
// reloj y palabra enmascarada). La palabra REAL nunca vive acá: viaja solo al escultor.

#pragma once
#include "CoreMinimal.h"
#include "../Lobby/PTGameState.h"
#include "PTSculptGameState.generated.h"

class APTPlayerState;

UENUM(BlueprintType)
enum class EPTTurnPhase : uint8
{
    WaitingForPlayers UMETA(DisplayName="Esperando jugadores"),
    ChoosingWord      UMETA(DisplayName="Eligiendo palabra"),
    Drawing           UMETA(DisplayName="Esculpiendo"),
    TurnEnd           UMETA(DisplayName="Fin de turno"),
    GameOver          UMETA(DisplayName="Fin de la partida")
};

// Tipo de línea de chat, para que el HUD la formatee:
//  Normal  → "Nombre: Mensaje"
//  Correct → "Nombre adivinó la palabra!" (Message se ignora, anti-spoiler)
//  System  → "Message" (Nombre se ignora; ej: "La palabra era: perro")
UENUM(BlueprintType)
enum class EPTChatType : uint8 { Normal, Correct, System };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPTOnTurnPhaseChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPTOnChatLine, const FString&, Name, const FString&, Message, EPTChatType, Type);
// Feedback al adivinar:
//  · YouGuessed  → SOLO al que adivina: la palabra (en su idioma) + los puntos que sumó (popup grande).
//  · SomeoneGuessed → a todos: quién adivinó (PlayerState) + puntos, para un mini-popup junto a su nombre.
//  · AllGuessed  → SOLO al escultor: aviso de que ya adivinaron todos (el turno está por cerrarse).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPTOnYouGuessed, const FString&, Word, int32, Points);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FPTOnSomeoneGuessed, APTPlayerState*, Guesser, int32, Points);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPTOnAllGuessed);

UCLASS()
class MYPARTYGAME_API APTSculptGameState : public APTGameState
{
    GENERATED_BODY()

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UPROPERTY(ReplicatedUsing=OnRep_TurnPhase, BlueprintReadOnly, Category="Game")
    EPTTurnPhase TurnPhase = EPTTurnPhase::WaitingForPlayers;

    // Quién esculpe en el turno actual (null mientras se espera). Los PlayerState se
    // replican a todos, así que esta referencia resuelve en cada cliente.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    APTPlayerState* CurrentSculptor = nullptr;

    // Momento (en tiempo de servidor, GetServerWorldTimeSeconds) en que termina el turno
    // de dibujo. Los clientes calculan el restante solos → reloj sincronizado sin spamear RPCs.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    double TurnEndServerTime = 0.0;

    // Palabra enmascarada en CADA idioma ("_ u _ o" / "_ a _" / ...), indexada igual que
    // PTText::GetAvailableLanguages(). Anti-spoiler: son máscaras, la palabra real no se replica.
    // Se replican todas y cada cliente muestra la de SU idioma (ver MaskedWord).
    UPROPERTY(ReplicatedUsing=OnRep_Masked, BlueprintReadOnly, Category="Game")
    TArray<FString> MaskedWords;

    // Máscara en el idioma del JUGADOR LOCAL (la que muestra el HUD). NO se replica: se calcula
    // en cada cliente a partir del array de arriba. El HUD lee esta y no se entera de los idiomas.
    UPROPERTY(BlueprintReadOnly, Category="Game")
    FString MaskedWord;

    UFUNCTION() void OnRep_Masked();
    // Recalcula MaskedWord (idioma local) desde MaskedWords. La llama el server tras setearlas
    // (no recibe OnRep) y el cliente en OnRep_Masked. Refresca el HUD.
    void RefreshLocalMasked();

    // Ronda actual (1-based) y total de rondas de la partida (para el HUD "Ronda 2/3").
    // Una ronda = todos los jugadores esculpen una vez.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    int32 CurrentRound = 0;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    int32 TotalRounds = 0;

    // Segundos que faltan en el turno de dibujo (0 fuera de Drawing). Para el HUD.
    UFUNCTION(BlueprintPure, Category="Game")
    float GetTurnSecondsRemaining() const;

    // Segundos que faltan hasta el deadline de la FASE actual (sirve p/ ChoosingWord, sin el filtro
    // de Drawing). Para el contador de "elegir palabra". 0 si no hay deadline seteado.
    UFUNCTION(BlueprintPure, Category="Game")
    float GetPhaseSecondsRemaining() const
    {
        if (TurnEndServerTime <= 0.0) return 0.f;
        return FMath::Max(0.f, (float)(TurnEndServerTime - GetServerWorldTimeSeconds()));
    }

    // ¿El jugador local es el escultor de este turno?
    UFUNCTION(BlueprintPure, Category="Game")
    bool IsLocalPlayerSculptor() const;

    // El HUD (BP) se suscribe para redibujar cuando cambia la fase.
    UPROPERTY(BlueprintAssignable, Category="Game")
    FPTOnTurnPhaseChanged OnTurnPhaseChanged;

    UFUNCTION() void OnRep_TurnPhase();

    // ── Chat ────────────────────────────────────────────────────────────────
    // El HUD (BP) se suscribe para agregar la línea a la lista de chat.
    UPROPERTY(BlueprintAssignable, Category="Game")
    FPTOnChatLine OnChatLine;

    // El servidor difunde una línea de chat a todos. La palabra correcta nunca viaja
    // como texto (los aciertos llegan como EPTChatType::Correct sin Message).
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_ChatLine(const FString& Name, const FString& Message, EPTChatType Type);

    // Línea de sistema LOCALIZADA. Viaja la CLAVE + los datos, no el texto armado: si mandáramos
    // el texto, todos lo verían en el idioma del host. Cada cliente lo traduce al suyo y lo emite
    // por el mismo OnChatLine (así el HUD y los Blueprints ya enganchados no cambian).
    // La clave usa {0} para Arg0 (nombre/palabra) y {1} para Arg1 (número).
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SystemLine(FName Key, const FString& Arg0, int32 Arg1);

    // ── Feedback al adivinar (los WBP enganchan estos delegates para mostrar los popups) ──
    /** Solo el que adivina (via Client RPC del PlayerController): palabra + puntos → popup grande. */
    UPROPERTY(BlueprintAssignable, Category="Game") FPTOnYouGuessed OnYouGuessed;
    /** A todos: quién adivinó + puntos → mini-popup junto a su nombre en el marcador. */
    UPROPERTY(BlueprintAssignable, Category="Game") FPTOnSomeoneGuessed OnSomeoneGuessed;
    /** Solo el escultor (via Client RPC): ya adivinaron todos, el turno se cierra. */
    UPROPERTY(BlueprintAssignable, Category="Game") FPTOnAllGuessed OnAllGuessed;

    /** Difunde a todos que 'Guesser' adivinó y cuántos puntos sumó (sin la palabra: anti-spoiler). */
    UFUNCTION(NetMulticast, Reliable)
    void Multicast_SomeoneGuessed(APTPlayerState* Guesser, int32 Points);

    /** Emite una línea de sistema SOLO en esta instancia (sin red): arma el texto con PTText (en
     *  el idioma local) y lo tira por OnChatLine. Lo usa Multicast_SystemLine y también los Client
     *  RPC que mandan un texto por jugador (p. ej. "la palabra era X" en el idioma de cada uno). */
    void EmitLocalSystemLine(FName Key, const FString& Arg0, int32 Arg1);
};
