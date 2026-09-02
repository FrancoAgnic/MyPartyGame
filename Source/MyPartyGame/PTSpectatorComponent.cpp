// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTSpectatorComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Lobby/PTPlayerState.h"

UPTSpectatorComponent::UPTSpectatorComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = false; // solo tickea mientras está activo
}

APlayerController* UPTSpectatorComponent::PC() const
{
    return Cast<APlayerController>(GetOwner());
}

void UPTSpectatorComponent::Toggle()
{
    APlayerController* C = PC();
    if (!C) return;
    if (bActive) Deactivate2(C); else Activate2(C);
}

void UPTSpectatorComponent::Activate2(APlayerController* C)
{
    // Semilla desde la cámara actual (para que la libre arranque donde estabas mirando).
    FVector Loc; FRotator Rot;
    C->GetPlayerViewPoint(Loc, Rot);
    CamLoc = Loc; CamRot = Rot;
    TargetLoc = Loc; TargetRot = Rot;

    CamActor = GetWorld()->SpawnActor<ACameraActor>(CamLoc, CamRot);
    if (!CamActor) return;

    PrevViewTarget = C->GetViewTarget();
    C->SetViewTargetWithBlend(CamActor, 0.15f);

    // Cortar el input del juego (que la cámara no mueva/esculpa el pawn) y capturar el mouse para mirar.
    C->SetIgnoreMoveInput(true);
    C->SetIgnoreLookInput(true);
    bPrevShowCursor = C->bShowMouseCursor;
    C->bShowMouseCursor = false;
    C->SetInputMode(FInputModeGameOnly());

    bActive = true;
    PovIndex = -1;
    SetComponentTickEnabled(true);
}

void UPTSpectatorComponent::Deactivate2(APlayerController* C)
{
    SetComponentTickEnabled(false);
    bActive = false;
    PovIndex = -1;

    if (PrevViewTarget.IsValid()) C->SetViewTargetWithBlend(PrevViewTarget.Get(), 0.15f);
    else if (C->GetPawn())        C->SetViewTargetWithBlend(C->GetPawn(), 0.15f);

    C->SetIgnoreMoveInput(false);
    C->SetIgnoreLookInput(false);
    C->bShowMouseCursor = bPrevShowCursor;

    if (CamActor) { CamActor->Destroy(); CamActor = nullptr; }
}

TArray<APawn*> UPTSpectatorComponent::GatherPovPawns(APlayerController* C) const
{
    TArray<APawn*> Pawns;
    const UWorld* W = GetWorld();
    const AGameStateBase* GS = W ? W->GetGameState() : nullptr;
    if (!GS || !C) return Pawns;

    // Orden estable por PlayerId; se excluyen espectadores y el propio jugador (que está oculto).
    TArray<APlayerState*> Players;
    for (APlayerState* PS : GS->PlayerArray)
        if (PS) Players.Add(PS);
    Players.Sort([](const APlayerState& A, const APlayerState& B){ return A.GetPlayerId() < B.GetPlayerId(); });

    for (APlayerState* PS : Players)
    {
        if (PS == C->PlayerState) continue;                 // no verme a mí mismo
        if (const APTPlayerState* PT = Cast<APTPlayerState>(PS))
            if (PT->bIsDevSpectator) continue;              // ni a otros espectadores
        if (APawn* P = PS->GetPawn()) Pawns.Add(P);
    }
    return Pawns;
}

void UPTSpectatorComponent::EnterFreeFly(APlayerController* C)
{
    PovIndex = -1;
    // Re-sembrar la cámara libre desde donde estábamos mirando (POV del jugador que dejamos).
    FVector Loc; FRotator Rot;
    C->GetPlayerViewPoint(Loc, Rot);
    CamLoc = Loc; CamRot = Rot; CamRot.Roll = 0.f;
    TargetLoc = CamLoc; TargetRot = CamRot;
    if (CamActor) CamActor->SetActorLocationAndRotation(CamLoc, CamRot);
    C->SetViewTargetWithBlend(CamActor, 0.2f);
}

void UPTSpectatorComponent::CyclePov(APlayerController* C)
{
    const TArray<APawn*> Pawns = GatherPovPawns(C);
    const int32 Next = PovIndex + 1; // -1 (libre) → 0 → 1 → ...
    if (!Pawns.IsValidIndex(Next))
    {
        EnterFreeFly(C); // pasamos el último → volver al vuelo libre
        return;
    }
    PovIndex = Next;
    C->SetViewTargetWithBlend(Pawns[PovIndex], 0.3f);
}

void UPTSpectatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    APlayerController* C = PC();
    if (!bActive || !C || !CamActor) return;

    // TAB: ciclar POV de jugadores ↔ vuelo libre.
    if (C->WasInputKeyJustPressed(EKeys::Tab))
        CyclePov(C);

    // Rueda del mouse: subir/bajar la velocidad de la cámara (multiplicativo, se siente parejo).
    const float Wheel = C->GetInputAnalogKeyState(EKeys::MouseWheelAxis);
    if (!FMath::IsNearlyZero(Wheel))
        SpeedScale = FMath::Clamp(SpeedScale * FMath::Pow(1.15f, Wheel), 0.1f, 30.f);

    // Viendo el POV de un jugador: el que controla es ese jugador; acá no movemos nada.
    if (PovIndex >= 0)
    {
        // Si el pawn desapareció (se fue / murió), volver al vuelo libre.
        const TArray<APawn*> Pawns = GatherPovPawns(C);
        if (!Pawns.IsValidIndex(PovIndex) || C->GetViewTarget() != Pawns[PovIndex])
        {
            if (Pawns.IsValidIndex(PovIndex)) C->SetViewTargetWithBlend(Pawns[PovIndex], 0.1f);
            else EnterFreeFly(C);
        }
        return;
    }

    // ── Vuelo libre ──
    // El mouse mueve el OBJETIVO de rotación; la cámara real interpola hacia él (giro suave).
    float DX = 0.f, DY = 0.f;
    C->GetInputMouseDelta(DX, DY);
    TargetRot.Yaw   += DX * LookSensitivity;
    TargetRot.Pitch  = FMath::Clamp(TargetRot.Pitch + DY * LookSensitivity, -89.f, 89.f);
    TargetRot.Roll   = 0.f;

    // Mover con WASD + Q/E (o Ctrl/Espacio): dirección relativa al OBJETIVO de rotación.
    const FVector Fwd   = TargetRot.Vector();
    const FVector Right = FRotationMatrix(TargetRot).GetScaledAxis(EAxis::Y);
    const FVector Up    = FVector::UpVector;
    FVector Dir = FVector::ZeroVector;
    if (C->IsInputKeyDown(EKeys::W)) Dir += Fwd;
    if (C->IsInputKeyDown(EKeys::S)) Dir -= Fwd;
    if (C->IsInputKeyDown(EKeys::D)) Dir += Right;
    if (C->IsInputKeyDown(EKeys::A)) Dir -= Right;
    if (C->IsInputKeyDown(EKeys::E) || C->IsInputKeyDown(EKeys::SpaceBar))    Dir += Up;
    if (C->IsInputKeyDown(EKeys::Q) || C->IsInputKeyDown(EKeys::LeftControl)) Dir -= Up;

    const float Turbo = (C->IsInputKeyDown(EKeys::LeftShift)) ? TurboMultiplier : 1.f;
    TargetLoc += Dir.GetSafeNormal() * MoveSpeed * SpeedScale * Turbo * DeltaTime;

    // Suavizado (lag): la cámara real persigue al objetivo → movimiento fluido, sin el pulso del mouse.
    CamLoc = FMath::VInterpTo(CamLoc, TargetLoc, DeltaTime, CamLagSpeed);
    CamRot = FMath::RInterpTo(CamRot, TargetRot, DeltaTime, CamLagSpeed);
    CamActor->SetActorLocationAndRotation(CamLoc, CamRot);
}
