// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasSenuelo.h"
#include "Components/StaticMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

ASillasSenuelo::ASillasSenuelo()
{
    PrimaryActorTick.bCanEverTick = false;

    // Se spawnea en el servidor al iniciar la ronda y se replica a los clientes.
    bReplicates = true;
    SetReplicateMovement(false); // no se mueve nunca (D6: los señuelos son sólidos)

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);
    Mesh->SetCollisionProfileName(TEXT("BlockAllDynamic"));

    // Placeholder greybox: caja 50x50x100 — MISMAS dimensiones que el pawn
    // silla-jugador (ver ASillasPawnSilla). Fase 6: mesh de silla real compartido.
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cubo(
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (Cubo.Succeeded())
    {
        Mesh->SetStaticMesh(Cubo.Object);
        Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 1.0f));
    }
}
