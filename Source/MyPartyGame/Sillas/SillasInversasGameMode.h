// Copyright Epic Games, Inc. All Rights Reserved.
// MODO INVERSO (Variante C + M1) — 1 silla vs. todos los cazadores.
// El sentado NO rompe: el cazador queda MONTADO en la silla-jugador y la pareja
// se mueve a los saltitos (la silla maneja XY, el jinete impulsa Z con Espacio)
// para estorbar a los cazadores que aún no se sentaron. El primer sentado de
// cada ronda asciende a silla la ronda siguiente (las sillas se ACUMULAN).
// Ronda termina cuando todas las sillas están montadas o expira el reloj (D2b):
// las sillas libres al expirar cobran el bonus. El cazador que nunca logró
// sentarse en todo el match es el perdedor único.
// Hereda TODO lo demás del Modo 1: fases, caminata de cola, señuelos, audio,
// puntos, HUD, métricas. Ver docs/diseno-modo-sillas-inversas_generado-por-Claude.md.

#pragma once
#include "CoreMinimal.h"
#include "SillasGameMode.h"
#include "SillasInversasGameMode.generated.h"

UCLASS()
class MYPARTYGAME_API ASillasInversasGameMode : public ASillasGameMode
{
    GENERATED_BODY()

public:
    virtual void ResolverSentado(ASillasPawnCazador* Cazador, AActor* Objetivo) override;

protected:
    virtual void AsignarRoles() override;
    virtual void TerminarMatch() override;

private:
    // Quiénes son sillas este match (ascienden y no bajan).
    TSet<TWeakObjectPtr<ASillasPlayerState>> SillasDelMatch;

    // El primer cazador que se sentó esta ronda (asciende al empezar la próxima).
    TWeakObjectPtr<ASillasPlayerState> PrimerSentadoDeLaRonda;

    // Para coronar al perdedor único: quiénes lograron sentarse alguna vez.
    TSet<TWeakObjectPtr<ASillasPlayerState>> SeSentaronAlgunaVez;
};
