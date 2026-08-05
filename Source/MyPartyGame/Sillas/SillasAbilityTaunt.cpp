// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasAbilityTaunt.h"
#include "SillasGameState.h"
#include "GameFramework/Pawn.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

USillasAbilityTaunt::USillasAbilityTaunt()
{
    Nombre      = NSLOCTEXT("Sillas", "TauntNombre", "Burla");
    CooldownSeg = 8.f;

    static ConstructorHelpers::FObjectFinder<USoundBase> Snd(
        TEXT("/Game/Sillas/Audio/A_Taunt.A_Taunt"));
    if (Snd.Succeeded()) Sonido = Snd.Object;
}

void USillasAbilityTaunt::Ejecutar_Implementation(APawn* Duenio) const
{
    // Corre en el servidor (contrato de USillasAbilityData); el sonido sale
    // por multicast del GameState para que lo oigan todos, cazador incluido.
    if (!Duenio || !Sonido) return;

    if (ASillasGameState* GS = Duenio->GetWorld()->GetGameState<ASillasGameState>())
    {
        GS->Multicast_SonidoEnPosicion(Duenio->GetActorLocation(), Sonido, RadioAudible);
    }
}
