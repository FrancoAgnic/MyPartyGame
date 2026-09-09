// Copyright Epic Games, Inc. All Rights Reserved.
// Diagnóstico de red del jugador local: ping (ms) + packet loss (%) + estado de conexión.
// Header-only para usarlo desde cualquier HUD sin duplicar la lógica. El HUD lo consulta y muestra
// ICONOS chiquitos (arriba a la izquierda) solo cuando hay un problema (no un texto de debug).

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"

namespace PTNetStats
{
    // Umbrales (compartidos por todos los HUDs).
    constexpr int32  HighPingMs   = 500;   // > esto = latencia alta
    constexpr float  LossWarnPct  = 2.0f;  // >= esto (%) = paquetes perdidos
    constexpr double LostSeconds  = 3.0;   // sin recibir datos por más de esto = conexión caída

    struct FStatus
    {
        int32 PingMs      = 0;
        float LossPct     = 0.f;     // el peor de in/out
        bool  bRemote     = false;   // sos cliente conectado a un server (en host/standalone = false)
        bool  bHighPing   = false;   // ping > HighPingMs
        bool  bPacketLoss = false;   // loss >= LossWarnPct
        bool  bLost       = false;   // conexión caída / se fue internet / cayó el server
    };

    inline FStatus Query(const APlayerController* PC)
    {
        FStatus S;
        if (!PC) return S;

        if (const APlayerState* PS = PC->PlayerState)
            S.PingMs = FMath::RoundToInt(PS->GetPingInMilliseconds());

        const UWorld* W = PC->GetWorld();
        UNetDriver* ND = (W ? W->GetNetDriver() : nullptr);
        UNetConnection* Conn = ND ? ND->ServerConnection : nullptr;
        if (Conn)
        {
            S.bRemote = true;
            const float In  = Conn->GetInLossPercentage().GetAvgLossPercentage()  * 100.f;
            const float Out = Conn->GetOutLossPercentage().GetAvgLossPercentage() * 100.f;
            S.LossPct = FMath::Max(In, Out);

            // "Caída": la conexión cerró, o hace rato que no llega NADA del server (se fue internet / se
            // cayó el server, antes de que el motor dispare el network failure).
            const double SinceRecv = ND->GetElapsedTime() - Conn->LastReceiveTime;
            S.bLost = (Conn->GetConnectionState() == USOCK_Closed) || (SinceRecv > LostSeconds);
        }

        S.bHighPing   = S.PingMs  >  HighPingMs;
        S.bPacketLoss = S.LossPct >= LossWarnPct;
        return S;
    }
}
