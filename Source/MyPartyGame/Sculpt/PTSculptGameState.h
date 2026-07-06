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
    TurnEnd           UMETA(DisplayName="Fin de turno")
};

// Tipo de línea de chat, para que el HUD la formatee:
//  Normal  → "Nombre: Mensaje"
//  Correct → "Nombre adivinó la palabra!" (Message se ignora, anti-spoiler)
//  System  → "Message" (Nombre se ignora; ej: "La palabra era: perro")
UENUM(BlueprintType)
enum class EPTChatType : uint8 { Normal, Correct, System };

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPTOnTurnPhaseChanged);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FPTOnChatLine, const FString&, Name, const FString&, Message, EPTChatType, Type);

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

    // Palabra enmascarada para TODOS ("_ _ _ _"). Anti-spoiler: la real no se replica.
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Game")
    FString MaskedWord;

    // Segundos que faltan en el turno de dibujo (0 fuera de Drawing). Para el HUD.
    UFUNCTION(BlueprintPure, Category="Game")
    float GetTurnSecondsRemaining() const;

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
};
