// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasGameState.h"
#include "SillasBalanceData.h"
#include "Components/AudioComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    // Atenuación esférica simple armada inline (los assets de atenuación llegan
    // con el sound design real de Fase 6).
    FSoundAttenuationSettings AtenuacionEsferica(float Radio)
    {
        FSoundAttenuationSettings S;
        S.bAttenuate = true;
        S.AttenuationShape = EAttenuationShape::Sphere;
        S.AttenuationShapeExtents = FVector(Radio * 0.3f, 0.f, 0.f);
        S.FalloffDistance = Radio * 0.7f;
        return S;
    }
}

ASillasGameState::ASillasGameState()
{
    // Placeholder generado por Tools (ver Fase 4); reemplazable en BP_SillasGameState.
    static ConstructorHelpers::FObjectFinder<USoundBase> Musica(
        TEXT("/Game/Sillas/Audio/A_Musica_Loop.A_Musica_Loop"));
    if (Musica.Succeeded()) MusicaSound = Musica.Object;
}

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
    // FASE 4 — la música ES gameplay (D3): suena exactamente durante la fase
    // Musica en cada cliente. El HUD (Fase 5) también va a colgarse de acá.
    if (IsNetMode(NM_DedicatedServer)) return;

    if (Fase == ESillasFase::Musica && MusicaSound)
    {
        if (!MusicaComp)
        {
            MusicaComp = UGameplayStatics::SpawnSound2D(
                this, MusicaSound, 1.f, 1.f, 0.f, nullptr,
                /*bPersistAcrossLevelTransition=*/false, /*bAutoDestroy=*/false);
        }
        if (MusicaComp)
        {
            // D12 sonoro: al caer sillas la música se acelera (pitch).
            const USillasBalanceData* B = Balance ? Balance.Get() : GetDefault<USillasBalanceData>();
            const float f = SillasAlInicioDeRonda > 0
                ? 1.f - (float)SillasVivas / (float)SillasAlInicioDeRonda : 0.f;
            MusicaComp->SetPitchMultiplier(FMath::Lerp(1.f, B->MusicaPitchIntensificacionMax, f));
            MusicaComp->Play();
        }
    }
    else if (MusicaComp && MusicaComp->IsPlaying())
    {
        MusicaComp->FadeOut(0.25f, 0.f); // el corte seco lo decide el sound design real (Fase 6)
    }
}

void ASillasGameState::Multicast_SonidoEnPosicion_Implementation(
    FVector Posicion, USoundBase* Sonido, float RadioAudible)
{
    if (IsNetMode(NM_DedicatedServer) || !Sonido) return;

    if (UAudioComponent* C = UGameplayStatics::SpawnSoundAtLocation(this, Sonido, Posicion))
    {
        C->bOverrideAttenuation  = true;
        C->AttenuationOverrides  = AtenuacionEsferica(RadioAudible);
    }
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
