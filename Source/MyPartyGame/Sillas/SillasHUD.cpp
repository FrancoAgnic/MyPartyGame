// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasHUD.h"
#include "SillasAbilityComponent.h"
#include "SillasBalanceData.h"
#include "SillasGameState.h"
#include "SillasPawnCazador.h"
#include "SillasPawnSilla.h"
#include "SillasPlayerState.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"

namespace
{
    FString NombreDeFase(ESillasFase Fase)
    {
        switch (Fase)
        {
        case ESillasFase::Esperando:  return TEXT("Esperando jugadores...");
        case ESillasFase::IntroRonda: return TEXT("¡Preparense!");
        case ESillasFase::Musica:     return TEXT("♪ MUSICA ♪  (el cazador baila)");
        case ESillasFase::Silencio:   return TEXT("SILENCIO  (¡caza abierta!)");
        case ESillasFase::FinRonda:   return TEXT("Fin de la ronda");
        case ESillasFase::FinMatch:   return TEXT("FIN DEL MATCH");
        }
        return TEXT("");
    }
}

void ASillasHUD::DrawHUD()
{
    Super::DrawHUD();
    if (!Canvas) return;

    const ASillasGameState* GS = GetWorld()->GetGameState<ASillasGameState>();
    if (!GS) return;

    DibujarFaseYRonda(GS);
    DibujarFeed(GS);

    if (GS->Fase == ESillasFase::FinRonda || GS->Fase == ESillasFase::FinMatch)
    {
        DibujarScoreboard(GS, GS->Fase == ESillasFase::FinMatch);
    }
    else
    {
        DibujarEstadoLocal(GS);
    }
}

void ASillasHUD::DibujarFaseYRonda(const ASillasGameState* GS)
{
    const float CX = Canvas->SizeX * 0.5f;
    UFont* Grande = GEngine->GetLargeFont();
    UFont* Medio  = GEngine->GetMediumFont();

    const FLinearColor ColorFase =
        GS->Fase == ESillasFase::Silencio ? FLinearColor(1.f, 0.35f, 0.25f) :
        GS->Fase == ESillasFase::Musica   ? FLinearColor(0.4f, 0.9f, 1.f)   :
                                            FLinearColor::White;

    TextoCentrado(NombreDeFase(GS->Fase), CX, 24.f, ColorFase, Grande, 1.6f);

    if (GS->Fase == ESillasFase::Musica || GS->Fase == ESillasFase::Silencio ||
        GS->Fase == ESillasFase::IntroRonda)
    {
        TextoCentrado(FString::Printf(TEXT("%.0f"), GS->GetSegundosRestantesDeFase()),
                      CX, 58.f, FLinearColor::White, Grande, 1.3f);
    }

    // Esquina superior izquierda: ronda + sillas vivas.
    if (GS->RondaActual > 0)
    {
        DrawText(FString::Printf(TEXT("Ronda %d/%d"), GS->RondaActual, GS->RondasTotales),
                 FLinearColor::White, 20.f, 20.f, Medio, 1.2f);
        DrawText(FString::Printf(TEXT("Sillas vivas: %d"), GS->SillasVivas),
                 FLinearColor(1.f, 0.85f, 0.4f), 20.f, 44.f, Medio, 1.2f);
    }
}

void ASillasHUD::DibujarFeed(const ASillasGameState* GS)
{
    UFont* Medio = GEngine->GetMediumFont();
    const float Ahora = GS->GetServerWorldTimeSeconds();
    const bool bPantallaFinal =
        GS->Fase == ESillasFase::FinRonda || GS->Fase == ESillasFase::FinMatch;

    float Y = Canvas->SizeY - 40.f;
    for (int32 i = GS->Feed.Num() - 1; i >= 0; --i)
    {
        const FSillasFeedEntry& E = GS->Feed[i];
        if (!bPantallaFinal && Ahora - E.ServerTime > 8.f) continue; // línea vieja

        DrawText(E.Texto, FLinearColor(0.95f, 0.95f, 0.95f, 0.9f), 20.f, Y, Medio, 1.1f);
        Y -= 22.f;
        if (Y < Canvas->SizeY - 180.f) break;
    }
}

void ASillasHUD::DibujarEstadoLocal(const ASillasGameState* GS)
{
    APlayerController* PC = GetOwningPlayerController();
    if (!PC) return;
    UFont* Medio = GEngine->GetMediumFont();
    const float CX = Canvas->SizeX * 0.5f;
    const float YBase = Canvas->SizeY - 88.f;

    if (const ASillasPawnSilla* Silla = Cast<ASillasPawnSilla>(PC->GetPawn()))
    {
        TextoCentrado(TEXT("SOS UNA SILLA — camuflate (Shift sprint · clic empujon · Q burla)"),
                      CX, YBase - 26.f, FLinearColor(0.5f, 1.f, 0.5f), Medio, 1.1f);

        // Stamina de sprint.
        Barra(CX - 120.f, YBase, 240.f, 10.f, Silla->GetStamina01(),
              FLinearColor(0.3f, 0.85f, 1.f));

        // Aguante (solo si D18 está activo en balance).
        if (GS->Balance && GS->Balance->bAguantarRespiracionActivo)
        {
            Barra(CX - 120.f, YBase + 16.f, 240.f, 8.f, Silla->GetAguante01(),
                  Silla->EstaAguantando() ? FLinearColor(1.f, 0.9f, 0.2f)
                                          : FLinearColor(0.7f, 0.7f, 0.7f));
        }

        // Cooldown de la habilidad (taunt).
        if (const USillasAbilityComponent* Hab = Silla->GetHabilidad())
        {
            const float CD = Hab->GetCooldownRestante();
            if (CD > 0.f)
            {
                TextoCentrado(FString::Printf(TEXT("Burla en %.0fs"), CD),
                              CX, YBase + 30.f, FLinearColor(0.8f, 0.8f, 0.8f), Medio, 1.f);
            }
        }
    }
    else if (const ASillasPawnCazador* Caz = Cast<ASillasPawnCazador>(PC->GetPawn()))
    {
        FString Estado;
        FLinearColor Color = FLinearColor(1.f, 0.55f, 0.3f);
        if (Caz->EstaBailando())        { Estado = TEXT("BAILANDO — no podes cazar"); }
        else if (Caz->EstaAdolorido())  { Estado = TEXT("¡AY! Era un señuelo..."); Color = FLinearColor::Red; }
        else if (Caz->EstaCapturando()) { Estado = TEXT("CAMINATA DE COLA — solta el clic para cancelar"); }
        else                            { Estado = TEXT("SOS EL CAZADOR — escucha la respiracion (clic: caminata de cola)"); }

        TextoCentrado(Estado, CX, YBase, Color, Medio, 1.2f);
    }
}

void ASillasHUD::DibujarScoreboard(const ASillasGameState* GS, bool bPodio)
{
    UFont* Grande = GEngine->GetLargeFont();
    UFont* Medio  = GEngine->GetMediumFont();
    const float CX = Canvas->SizeX * 0.5f;

    // Ordenar por puntos, de mayor a menor.
    TArray<ASillasPlayerState*> Tabla;
    for (APlayerState* PS : GS->PlayerArray)
    {
        if (ASillasPlayerState* SPS = Cast<ASillasPlayerState>(PS)) Tabla.Add(SPS);
    }
    Tabla.Sort([](const ASillasPlayerState& A, const ASillasPlayerState& B)
               { return A.PuntosMatch > B.PuntosMatch; });

    float Y = Canvas->SizeY * 0.3f;
    TextoCentrado(bPodio ? TEXT("*** PODIO FINAL ***") : TEXT("PUNTAJES"),
                  CX, Y, FLinearColor(1.f, 0.85f, 0.2f), Grande, 1.6f);
    Y += 44.f;

    for (int32 i = 0; i < Tabla.Num(); ++i)
    {
        const ASillasPlayerState* SPS = Tabla[i];
        const bool bTop3 = bPodio && i < 3;
        const FString Fila = FString::Printf(TEXT("%d.  %s — %d pts"),
                                             i + 1, *SPS->DisplayName, SPS->PuntosMatch);
        TextoCentrado(Fila, CX, Y,
                      bTop3 ? FLinearColor(1.f, 0.85f, 0.2f) : FLinearColor::White,
                      bTop3 ? Grande : Medio, bTop3 ? 1.3f : 1.1f);
        Y += bTop3 ? 32.f : 24.f;
    }

    if (bPodio)
    {
        TextoCentrado(TEXT("Volviendo al lobby..."), CX, Y + 24.f,
                      FLinearColor(0.7f, 0.7f, 0.7f), Medio, 1.f);
    }
}

void ASillasHUD::TextoCentrado(const FString& Texto, float CentroX, float Y,
                               const FLinearColor& Color, UFont* Fuente, float Escala)
{
    float W = 0.f, H = 0.f;
    GetTextSize(Texto, W, H, Fuente, Escala);
    DrawText(Texto, FLinearColor(0.f, 0.f, 0.f, 0.7f), CentroX - W * 0.5f + 1.f, Y + 1.f, Fuente, Escala); // sombra
    DrawText(Texto, Color, CentroX - W * 0.5f, Y, Fuente, Escala);
}

void ASillasHUD::Barra(float X, float Y, float Ancho, float Alto, float Valor01,
                       const FLinearColor& Color)
{
    DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.55f), X - 1.f, Y - 1.f, Ancho + 2.f, Alto + 2.f);
    DrawRect(Color, X, Y, Ancho * FMath::Clamp(Valor01, 0.f, 1.f), Alto);
}
