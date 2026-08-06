// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasInversasGameMode.h"
#include "SillasBalanceData.h"
#include "SillasGameState.h"
#include "SillasPawnCazador.h"
#include "SillasPawnSilla.h"
#include "SillasPlayerState.h"
#include "SillasSenuelo.h"

void ASillasInversasGameMode::AsignarRoles()
{
    ASillasGameState* GS = SillasGS();
    if (!GS) return;

    // Ascenso de la ronda anterior: el primer sentado se suma a las sillas.
    if (PrimerSentadoDeLaRonda.IsValid())
    {
        SillasDelMatch.Add(PrimerSentadoDeLaRonda);
        GS->AgregarFeed(FString::Printf(TEXT("%s asciende a SILLA"),
                        *PrimerSentadoDeLaRonda->DisplayName));
    }
    PrimerSentadoDeLaRonda.Reset();

    // Juntar jugadores y resetear ronda (todos arrancan como Silla por el reset).
    TArray<ASillasPlayerState*> Jugadores;
    for (APlayerState* PS : GS->PlayerArray)
    {
        if (ASillasPlayerState* SPS = Cast<ASillasPlayerState>(PS))
        {
            SPS->Server_ResetRonda();
            Jugadores.Add(SPS);
        }
    }
    if (Jugadores.Num() == 0) return;

    // Limpiar sillas que se desconectaron y garantizar al menos 1 cazador.
    for (auto It = SillasDelMatch.CreateIterator(); It; ++It)
    {
        if (!It->IsValid() || !Jugadores.Contains(It->Get())) It.RemoveCurrent();
    }
    while (SillasDelMatch.Num() >= Jugadores.Num() && SillasDelMatch.Num() > 0)
    {
        auto It = SillasDelMatch.CreateIterator();
        It.RemoveCurrent();
    }

    // Arranque del match: una única silla al azar (el espejo exacto del Modo 1).
    if (SillasDelMatch.Num() == 0)
    {
        SillasDelMatch.Add(Jugadores[FMath::RandRange(0, Jugadores.Num() - 1)]);
    }

    // Roles: silla si está en la lista del match; cazador el resto.
    int32 NumSillas = 0;
    for (ASillasPlayerState* SPS : Jugadores)
    {
        if (SillasDelMatch.Contains(SPS))
        {
            ++NumSillas; // el reset ya lo dejó como Silla
        }
        else
        {
            SPS->Server_SetRol(ESillasRole::Cazador);
        }
    }

    GS->SillasVivas           = NumSillas;
    GS->SillasAlInicioDeRonda = NumSillas;
}

void ASillasInversasGameMode::ResolverSentado(ASillasPawnCazador* Cazador, AActor* Objetivo)
{
    if (!HasAuthority() || !Cazador || !Objetivo) return;

    // Señuelo: mismo castigo D6 que el Modo 1 (lo resuelve la base).
    if (Cast<ASillasSenuelo>(Objetivo))
    {
        Super::ResolverSentado(Cazador, Objetivo);
        return;
    }

    // M1 — silla-jugador: MONTURA en vez de rotura.
    ASillasPawnSilla* Silla = Cast<ASillasPawnSilla>(Objetivo);
    ASillasGameState* GS = SillasGS();
    if (!Silla || Silla->EstaMontada() || !GS) return;

    MetricasRonda.Capturas++; // en este modo "captura" = sentada lograda

    ASillasPlayerState* PSJinete = Cazador->GetPlayerState<ASillasPlayerState>();
    ASillasPlayerState* PSSilla  = Silla->GetPlayerState<ASillasPlayerState>();

    Silla->MontarServer(Cazador);

    if (PSJinete)
    {
        PSJinete->Server_SumarPuntos(Balance->PuntosPorCaptura);
        SeSentaronAlgunaVez.Add(PSJinete);

        if (!PrimerSentadoDeLaRonda.IsValid())
        {
            PrimerSentadoDeLaRonda = PSJinete;
            GS->AgregarFeed(FString::Printf(
                TEXT("%s se sentó PRIMERO en %s (+%d) — asciende la próxima ronda"),
                *PSJinete->DisplayName, PSSilla ? *PSSilla->DisplayName : TEXT("¿?"),
                Balance->PuntosPorCaptura));
        }
        else
        {
            GS->AgregarFeed(FString::Printf(TEXT("%s se sentó en %s (+%d)"),
                *PSJinete->DisplayName, PSSilla ? *PSSilla->DisplayName : TEXT("¿?"),
                Balance->PuntosPorCaptura));
        }
    }

    // La silla montada deja de puntuar supervivencia y no cobra el bonus final
    // (pero sigue jugando: ahora es la mitad de abajo del tándem que estorba).
    if (PSSilla)
    {
        PSSilla->Server_MarcarEliminado();
    }

    GS->SillasVivas = FMath::Max(0, GS->SillasVivas - 1);
    if (GS->SillasVivas <= 0)
    {
        GS->AgregarFeed(TEXT("Todas las sillas fueron montadas — se acabó la ronda"));
        TerminarRonda();
    }
}

void ASillasInversasGameMode::TerminarMatch()
{
    // El perdedor único: cazador que jamás logró sentarse en todo el match.
    if (ASillasGameState* GS = SillasGS())
    {
        for (APlayerState* PS : GS->PlayerArray)
        {
            ASillasPlayerState* SPS = Cast<ASillasPlayerState>(PS);
            if (SPS && SPS->Rol == ESillasRole::Cazador && !SeSentaronAlgunaVez.Contains(SPS))
            {
                GS->AgregarFeed(FString::Printf(
                    TEXT("%s NUNCA consiguió sentarse... el gran perdedor"),
                    *SPS->DisplayName));
            }
        }
    }

    Super::TerminarMatch();
}
