// "Cuerpo" de un jugador del celular en los modos local y audiencia: un personaje de IA con el nombre
// y el color del jugador, que flota alrededor de la escultura mirándola, como un espectador más.
// Lo que el jugador hace en el celular (adivinar, escribir) se ve sobre su cabeza: el GameMode usa
// PlayerState->GetPawn() para los globos de chat y el confeti, igual que con un jugador normal.
//
// El PlayerState NO es del controller (es la del celular): se le asigna al pawn con SetPlayerState.

#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "PTPartyBotController.generated.h"

UCLASS()
class MYPARTYGAME_API APTPartyBotController : public AAIController
{
    GENERATED_BODY()

public:
    APTPartyBotController();

    // Distancia extra (afuera del cubo de esculpir) a la que flotan.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PartyBot") float SeatDistance = 260.f;
    // Altura respecto del centro del cubo (rango al azar).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PartyBot") float SeatHeightMin = -150.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PartyBot") float SeatHeightMax = 250.f;
    // Cada cuánto cambian de lugar (segundos, al azar en el rango).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PartyBot") float ReseatMin = 10.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PartyBot") float ReseatMax = 22.f;
    // Separación mínima entre bots al elegir lugar.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PartyBot") float MinSpacing = 160.f;
    // Flotado suave (cm) para que no queden quietos como estatuas.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="PartyBot") float BobAmplitude = 18.f;

    /** Primer lugar: al aparecer el bot ya está en su asiento (no cruza la escultura volando). */
    FVector PickSeat() const;
    void SetSeat(const FVector& InSeat) { Seat = InSeat; bHasSeat = true; }

    virtual void Tick(float DeltaSeconds) override;

private:
    FVector Seat = FVector::ZeroVector;
    bool    bHasSeat = false;
    float   ReseatTimer = 0.f;
    float   BobPhase = 0.f;

    bool GetCanvas(FVector& OutCenter, FVector& OutHalfExtent) const;
};
