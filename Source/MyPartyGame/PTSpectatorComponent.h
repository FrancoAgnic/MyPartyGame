// Copyright Epic Games, Inc. All Rights Reserved.
// Cámara ESPECTADOR de vuelo libre (WASD + mouse) para grabar trailer / sacar capturas. Dev-only:
// se activa por comando de consola. Es un componente compartido que se cuelga del PlayerController
// (funciona igual en el lobby y en Lvl-01). Todo LOCAL: no toca la partida ni a los demás jugadores.
//
// Controles mientras está activo: WASD mover, Q/E (o Ctrl/Espacio) bajar/subir, Shift = turbo,
// mouse = mirar, RUEDA del mouse = subir/bajar la velocidad. TAB = ciclar por el POV de cada jugador
// (y después de todos, vuelve al vuelo libre).

#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PTSpectatorComponent.generated.h"

UCLASS()
class MYPARTYGAME_API UPTSpectatorComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UPTSpectatorComponent();

    /** Prende/apaga la cámara libre. */
    void Toggle();
    bool IsActive() const { return bActive; }

    /** Pawn cuyo POV estás mirando (nullptr si estás en vuelo libre / inactivo). Para que el HUD
     *  muestre el hotbar del jugador que esculpe cuando lo espectás. */
    class APawn* GetPovPawn() const;

    /** Multiplica la velocidad base (lo usa el comando PTSpecSpeed). */
    void SetSpeedScale(float InScale) { SpeedScale = FMath::Max(0.05f, InScale); }
    /** Suavizado de la cámara (lag): más BAJO = más suave/con más lag; más ALTO = más directo. */
    void SetSmoothSpeed(float S) { CamLagSpeed = FMath::Clamp(S, 1.f, 60.f); }

    UPROPERTY(EditAnywhere, Category="Spectator") float MoveSpeed       = 1200.f; // cm/s base
    UPROPERTY(EditAnywhere, Category="Spectator") float TurboMultiplier = 3.0f;   // con Shift
    UPROPERTY(EditAnywhere, Category="Spectator") float LookSensitivity = 1.0f;
    // Suavizado del movimiento y del giro (interpolación). Bajo = cámara "pesada" (bueno para video).
    UPROPERTY(EditAnywhere, Category="Spectator") float CamLagSpeed     = 8.0f;

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
    class APlayerController* PC() const;
    void Activate2(class APlayerController* C);
    void Deactivate2(class APlayerController* C);
    // TAB: cicla vuelo libre → POV jugador 0 → POV jugador 1 → ... → vuelta a vuelo libre.
    void CyclePov(class APlayerController* C);
    // Pawns de los jugadores (excluye espectadores y a mí mismo), orden estable por PlayerId.
    TArray<class APawn*> GatherPovPawns(class APlayerController* C) const;
    void EnterFreeFly(class APlayerController* C); // vuelve al vuelo libre (re-siembra la cámara)

    // POV: al mirar el POV de un jugador, traducir la UI a SU idioma y mostrar su bandera grande
    // (centro-derecha). Al volver al vuelo libre / salir, se restaura tu idioma.
    void ApplyPovLanguageAndFlag(class APlayerController* C, class APawn* PovPawn);
    void ClearPovOverrides();

    bool bActive = false;
    UPROPERTY(Transient) AActor* CamActor = nullptr;
    UPROPERTY(Transient) class UPTSpectatorHUDWidget* FlagHUD = nullptr;
    FString  SavedLangCode;         // tu idioma, para restaurarlo al salir del POV
    bool     bLangOverridden = false;
    TWeakObjectPtr<class APTLobbyCharacter> HiddenPovPawn; // pawn cuyo cuerpo ocultamos localmente en POV
    TWeakObjectPtr<AActor> PrevViewTarget;
    // CamLoc/CamRot = transform REAL de la cámara (lo que se aplica). Target* = objetivo que responde
    // al instante al input; la real interpola hacia el objetivo → suavizado/lag para video.
    FVector  CamLoc = FVector::ZeroVector;
    FRotator CamRot = FRotator::ZeroRotator;
    FVector  TargetLoc = FVector::ZeroVector;
    FRotator TargetRot = FRotator::ZeroRotator;
    float    SpeedScale = 1.f;
    bool     bPrevShowCursor = false;
    int32    PovIndex = -1; // -1 = vuelo libre; >=0 = viendo el POV de ese jugador
};
