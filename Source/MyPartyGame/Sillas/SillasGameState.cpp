// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasGameState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Net/UnrealNetwork.h"

void ASillasGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ASillasGameState, Balance);
    DOREPLIFETIME(ASillasGameState, Fase);
    DOREPLIFETIME(ASillasGameState, FaseTerminaEnServerTime);
    DOREPLIFETIME(ASillasGameState, RondaActual);
    DOREPLIFETIME(ASillasGameState, SillasVivas);
    DOREPLIFETIME(ASillasGameState, SillasAlInicioDeRonda);
}

float ASillasGameState::GetSegundosRestantesDeFase() const
{
    return FMath::Max(0.f, FaseTerminaEnServerTime - GetServerWorldTimeSeconds());
}

void ASillasGameState::OnRep_Fase()
{
    // Hook para clientes: acá va a reaccionar el audio (Fase 4: música/silencio)
    // y el HUD (Fase 5). En Fase 1 no hay nada que hacer todavía.
}

void ASillasGameState::Multicast_EfectoRoturaSilla_Implementation(FVector Epicentro)
{
    // Pedazos placeholder: cubitos con física que salen despedidos y desaparecen.
    // Corre en cada máquina por separado (el resultado exacto puede diferir entre
    // clientes — da igual, es puro teatro; la eliminación real ya la decidió el server).
    UStaticMesh* Cubo = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!Cubo) return;

    for (int32 i = 0; i < 6; ++i)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        AStaticMeshActor* Pedazo = GetWorld()->SpawnActor<AStaticMeshActor>(
            Epicentro + FVector(0.f, 0.f, 30.f + 15.f * i),
            FRotator(FMath::FRandRange(0.f, 360.f), FMath::FRandRange(0.f, 360.f), 0.f),
            Params);
        if (!Pedazo) continue;

        UStaticMeshComponent* Mesh = Pedazo->GetStaticMeshComponent();
        Mesh->SetMobility(EComponentMobility::Movable);
        Mesh->SetStaticMesh(Cubo);
        Pedazo->SetActorScale3D(FVector(FMath::FRandRange(0.12f, 0.22f)));
        Mesh->SetSimulatePhysics(true);
        Mesh->AddImpulse(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f),
                                 FMath::FRandRange(0.6f, 1.2f)) * 220.f, NAME_None, /*bVelChange=*/true);
        Pedazo->SetLifeSpan(4.f);
    }
}
