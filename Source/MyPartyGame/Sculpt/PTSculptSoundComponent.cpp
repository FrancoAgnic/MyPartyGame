// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSculptSoundComponent.h"
#include "../PTGameInstance.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UPTGameInstance* UPTSculptSoundComponent::GI() const
{
    return GetWorld() ? Cast<UPTGameInstance>(GetWorld()->GetGameInstance()) : nullptr;
}

USoundBase* UPTSculptSoundComponent::LoopSoundFor(EPTEditMode Mode, bool bEyes) const
{
    if (bEyes) return nullptr; // ojos = one-shot, no loop
    UPTGameInstance* G = GI();
    if (!G) return nullptr;
    switch (Mode)
    {
    case EPTEditMode::Add:   return G->SndAddLoop;
    case EPTEditMode::Erase: return G->SndEraseLoop;
    case EPTEditMode::Paint: return G->SndPaintLoop;
    default:                 return nullptr; // Smooth u otros: sin loop
    }
}

void UPTSculptSoundComponent::SetActiveTool(EPTEditMode Mode, bool bEyes, bool bActive, const FVector& WorldPos)
{
    USoundBase* Want = bActive ? LoopSoundFor(Mode, bEyes) : nullptr;
    if (!Want) { StopLoop(); return; }

    // ¿Hay que (re)crear el loop? (no existe, o cambió la herramienta)
    if (!Loop || LoopMode != Mode || bLoopIsEyes != bEyes || !Loop->IsPlaying())
    {
        StopLoop();
        UPTGameInstance* G = GI();
        Loop = NewObject<UAudioComponent>(GetOwner());
        if (!Loop) return;
        Loop->bAutoActivate      = false;
        Loop->bAllowSpatialization = true;
        Loop->bStopWhenOwnerDestroyed = true;
        if (G && G->SculptAttenuation) Loop->AttenuationSettings = G->SculptAttenuation;
        Loop->RegisterComponent();       // sin adjuntar: se posiciona en mundo (el controller no tiene root)
        Loop->SetSound(Want);            // el asset debe ser LOOPING
        Loop->SetWorldLocation(WorldPos);
        Loop->Play();
        LoopMode    = Mode;
        bLoopIsEyes = bEyes;
    }
    else
    {
        Loop->SetWorldLocation(WorldPos); // seguir el pincel
    }
}

void UPTSculptSoundComponent::StopLoop()
{
    if (Loop) { Loop->Stop(); Loop->DestroyComponent(); Loop = nullptr; }
    LoopMode = EPTEditMode::Smooth;
}

void UPTSculptSoundComponent::PlayOneShot3D(USoundBase* S, const FVector& Pos)
{
    if (!S) return;
    UPTGameInstance* G = GI();
    UGameplayStatics::PlaySoundAtLocation(this, S, Pos, FRotator::ZeroRotator, 1.f, 1.f, 0.f,
                                          G ? G->SculptAttenuation : nullptr);
}

void UPTSculptSoundComponent::PlayEyes(const FVector& WorldPos)          { if (UPTGameInstance* G = GI()) PlayOneShot3D(G->SndEyes, WorldPos); }
void UPTSculptSoundComponent::PlayUndoSimple(const FVector& WorldPos)    { if (UPTGameInstance* G = GI()) PlayOneShot3D(G->SndUndoSimple, WorldPos); }
void UPTSculptSoundComponent::PlayUndoClearAll(const FVector& WorldPos)  { if (UPTGameInstance* G = GI()) PlayOneShot3D(G->SndUndoClearAll, WorldPos); }

void UPTSculptSoundComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    StopLoop();
    Super::EndPlay(Reason);
}
