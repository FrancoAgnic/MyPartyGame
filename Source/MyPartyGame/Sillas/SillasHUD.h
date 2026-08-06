// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 5 — UI mínima funcional, dibujada a canvas en C++ puro (cero assets):
// fase + countdown, ronda, sillas vivas, feed de eventos, estado del rol local
// (barras de stamina/aguante, cooldown de habilidad, estado del cazador),
// scoreboard entre rondas y podio final. La UI con estilo llega en Fase 6;
// esta existe para que el playtest del match completo sea legible.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SillasHUD.generated.h"

class ASillasGameState;

UCLASS()
class MYPARTYGAME_API ASillasHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;

private:
    void DibujarFaseYRonda(const ASillasGameState* GS);
    void DibujarFeed(const ASillasGameState* GS);
    void DibujarEstadoLocal(const ASillasGameState* GS);
    void DibujarScoreboard(const ASillasGameState* GS, bool bPodio);

    // Texto centrado horizontalmente en X (con sombra para legibilidad greybox).
    void TextoCentrado(const FString& Texto, float CentroX, float Y,
                       const FLinearColor& Color, UFont* Fuente, float Escala);
    void Barra(float X, float Y, float Ancho, float Alto, float Valor01,
               const FLinearColor& Color);
};
