// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSculptPlane.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Pawn.h"
#include "EngineUtils.h" // TActorIterator
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

APTSculptPlane::APTSculptPlane()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = true;
    SetReplicateMovement(false); // el servidor lo spawnea ya colocado; no se mueve después

    PlaneMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PlaneMesh"));
    SetRootComponent(PlaneMesh);
    // Bloquea el movimiento de pawns; la colisión real (forma/grosor) viene del mesh del BP.
    PlaneMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PlaneMesh->SetCollisionObjectType(ECC_WorldStatic);
    PlaneMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
    PlaneMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
    PlaneMesh->SetCastShadow(false);
    PlaneMesh->SetMobility(EComponentMobility::Movable);
}

void APTSculptPlane::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(APTSculptPlane, TargetPawn);
}

void APTSculptPlane::BeginPlay()
{
    Super::BeginPlay();
    RefreshMoveIgnores();

    // Los pawns pueden replicar/spawnear después que el plano: reintentar un rato para
    // asegurar que los nuevos también lo ignoren (salvo el escultor).
    GetWorldTimerManager().SetTimer(RefreshTimer, this, &APTSculptPlane::RefreshMoveIgnores, 1.0f, true);
}

void APTSculptPlane::SetTargetPawn(APawn* InPawn)
{
    if (!HasAuthority()) return;
    TargetPawn = InPawn;
    OnRep_TargetPawn(); // el servidor no recibe su propio OnRep
}

void APTSculptPlane::OnRep_TargetPawn()
{
    RefreshMoveIgnores();

    // Visual: solo el escultor ve su plano (a los que adivinan no les tapa la escultura).
    const APawn* LocalPawn = GetWorld() && GetWorld()->GetFirstPlayerController()
        ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    SetActorHiddenInGame(LocalPawn != TargetPawn);
}

void APTSculptPlane::RefreshMoveIgnores()
{
    if (!GetWorld()) return;

    // Regla determinística (misma en servidor y clientes): todos ignoran este plano al moverse,
    // menos el pawn del escultor. Así solo a él lo frena, y a los demás ni los roza.
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        APawn* P = *It;
        if (!P) continue;
        const bool bShouldIgnore = (P != TargetPawn);
        if (UPrimitiveComponent* Root = Cast<UPrimitiveComponent>(P->GetRootComponent()))
            Root->IgnoreActorWhenMoving(this, bShouldIgnore);
    }

    // Refrescar también la visibilidad por si el pawn local llegó después.
    const APawn* LocalPawn = GetWorld()->GetFirstPlayerController()
        ? GetWorld()->GetFirstPlayerController()->GetPawn() : nullptr;
    SetActorHiddenInGame(LocalPawn != TargetPawn);
}
