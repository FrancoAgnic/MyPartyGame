// TRAILER-ONLY (rama trailer-bots): controller de un bot de decoración.
// El bot NO esculpe ni juega; solo deambula alrededor del cubo y —vía el GameMode— escribe en el chat
// y a veces "adivina". Tiene PlayerState (bWantsPlayerState) para aparecer en el scoreboard.
// El pawn es el mismo personaje del juego, en modo vuelo (ApplyGameplayMovementMode).

#pragma once
#include "CoreMinimal.h"
#include "AIController.h"
#include "PTBotController.generated.h"

UCLASS()
class MYPARTYGAME_API APTBotController : public AAIController
{
    GENERATED_BODY()

public:
    APTBotController();

    /** Configura el deambular alrededor de un centro (el cubo). Radios en unidades de mundo. */
    void InitWander(const FVector& InCenter, float InMinRadius, float InMaxRadius);

protected:
    virtual void Tick(float DeltaSeconds) override;

private:
    FVector Center       = FVector::ZeroVector;
    float   MinRadius    = 350.f;
    float   MaxRadius    = 850.f;
    // Piso duro: los bots nunca bajan de esta Z de mundo (si no, atraviesan el suelo del nivel y se
    // van para abajo). Margen para que no queden clavados justo en el plano del piso.
    float   FloorZ       = 0.f;
    static constexpr float FloorMargin = 60.f;
    FVector CurrentTarget = FVector::ZeroVector;
    float   RepathAccum  = 0.f;   // tiempo desde el último re-target
    float   RepathEvery  = 0.f;   // cada cuánto forzar nuevo destino (random por tramo)
    bool    bHasTarget   = false;

    void PickNewTarget();
};
