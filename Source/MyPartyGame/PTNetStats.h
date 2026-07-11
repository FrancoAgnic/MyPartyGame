// Copyright Epic Games, Inc. All Rights Reserved.
// Helper de diagnóstico de red del jugador local: ping (ms) + packet loss (%).
// Header-only para poder usarlo desde cualquier HUD sin duplicar la lógica.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"

namespace PTNetStats
{
    struct FLine
    {
        FString Text;
        FColor  Color = FColor::Cyan;
    };

    // Arma una línea "Ping X ms  Loss in Y% / out Z%" del jugador local.
    //  - Ping: lo mide el servidor y lo replica al PlayerState (así que es "tu lag" real).
    //  - Packet loss: de la conexión cliente→servidor. Viene como fracción 0..1 → *100 para %.
    //  - En host/standalone no hay ServerConnection (todo local) → solo ping (0).
    // Color tipo semáforo según lo peor entre ping y loss (verde/amarillo/rojo).
    inline FLine Build(const APlayerController* PC)
    {
        FLine Out;
        if (!PC) return Out;

        int32 PingMs = 0;
        if (const APlayerState* PS = PC->PlayerState)
            PingMs = FMath::RoundToInt(PS->GetPingInMilliseconds());

        const UWorld* W = PC->GetWorld();
        UNetConnection* Conn = (W && W->GetNetDriver()) ? W->GetNetDriver()->ServerConnection : nullptr;

        float InLoss = 0.f, OutLoss = 0.f;
        if (Conn)
        {
            InLoss  = Conn->GetInLossPercentage().GetAvgLossPercentage()  * 100.f;
            OutLoss = Conn->GetOutLossPercentage().GetAvgLossPercentage() * 100.f;
            Out.Text = FString::Printf(TEXT("Ping %d ms   Loss in %.1f%% / out %.1f%%"),
                                       PingMs, InLoss, OutLoss);
        }
        else
        {
            Out.Text = FString::Printf(TEXT("Ping %d ms   (host local)"), PingMs);
        }

        const float WorstLoss = FMath::Max(InLoss, OutLoss);
        if      (PingMs >= 150 || WorstLoss >= 5.f) Out.Color = FColor(230,  70,  70); // rojo
        else if (PingMs >=  80 || WorstLoss >= 2.f) Out.Color = FColor(235, 200,  70); // amarillo
        else                                        Out.Color = FColor( 90, 210, 120); // verde
        return Out;
    }
}
