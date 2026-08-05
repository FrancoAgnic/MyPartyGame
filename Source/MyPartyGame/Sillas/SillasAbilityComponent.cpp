// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasAbilityComponent.h"
#include "SillasAbilityData.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"

USillasAbilityComponent::USillasAbilityComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void USillasAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(USillasAbilityComponent, CooldownHastaServerTime);
}

void USillasAbilityComponent::IntentarActivar()
{
    // Chequeo local solo para no spamear RPCs; la validación real es del server.
    if (!Habilidad || GetCooldownRestante() > 0.f) return;
    Server_Activar();
}

float USillasAbilityComponent::GetCooldownRestante() const
{
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    if (!GS) return 0.f;
    return FMath::Max(0.f, CooldownHastaServerTime - GS->GetServerWorldTimeSeconds());
}

void USillasAbilityComponent::Server_Activar_Implementation()
{
    const AGameStateBase* GS = GetWorld() ? GetWorld()->GetGameState() : nullptr;
    APawn* Duenio = Cast<APawn>(GetOwner());
    if (!Habilidad || !GS || !Duenio) return;

    const float Ahora = GS->GetServerWorldTimeSeconds();
    if (Ahora < CooldownHastaServerTime) return; // cooldown: rechazado

    CooldownHastaServerTime = Ahora + Habilidad->CooldownSeg;
    Habilidad->Ejecutar(Duenio);
}
