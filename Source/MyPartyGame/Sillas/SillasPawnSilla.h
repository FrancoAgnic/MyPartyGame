// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 1/3 — Pawn de la silla-jugador.
// A la vista debe ser IDÉNTICO a ASillasSenuelo — el camuflaje es la mecánica
// central (D9). El "mind game" de D4: moverse está siempre permitido, ser visto
// es sentencia. Kit D10 (Fase 3): sprint con stamina (Shift), salto (Espacio),
// empujón silla→silla (clic izquierdo — la traición física) y slot de habilidad
// con cooldown (Q; contenido pendiente de P10b). Cada habilidad delata: usarlas
// frente al cazador es suicidio, a sus espaldas es jugar bien.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SillasPawnSilla.generated.h"

class UAudioComponent;
class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class USillasAbilityComponent;
class USoundBase;
struct FInputActionValue;

UCLASS()
class MYPARTYGAME_API ASillasPawnSilla : public ACharacter
{
    GENERATED_BODY()

public:
    ASillasPawnSilla();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // --- Lecturas para el HUD (Fase 5) ---
    float GetStamina01() const;
    float GetAguante01() const;
    bool  EstaAguantando() const { return bAguantando; }
    USillasAbilityComponent* GetHabilidad() const { return Habilidad; }

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Caja idéntica al señuelo, colgada de la cápsula.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sillas")
    TObjectPtr<UStaticMeshComponent> ChairMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> Camera;

    // Slot de habilidad (D10/P10b): asignar el DataAsset en el BP cuando exista contenido.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sillas")
    TObjectPtr<USillasAbilityComponent> Habilidad;

    // FASE 4 (D17) — la respiración delatora: loop atenuado corto que SOLO los
    // cazadores oyen (el gating por rol es local, en ActualizarAudio).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sillas|Audio")
    TObjectPtr<UAudioComponent> RespiracionAudio;

    // FASE 4 (D17) — crujido de movimiento: lo oyen todos, volumen por velocidad
    // (el sprint es ruidoso por diseño).
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sillas|Audio")
    TObjectPtr<UAudioComponent> CrujidoAudio;

    // Jadeo delator al agotar el aguante (D18). Editable en BP.
    UPROPERTY(EditDefaultsOnly, Category="Sillas|Audio")
    TObjectPtr<USoundBase> JadeoSound;

    // Acciones de input (defaults: assets de /Game/Input del template). Los
    // mapping contexts base los agrega ASillasPlayerController; el pawn solo
    // agrega su contexto runtime de kit (sprint/empujón/habilidad).
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> MouseLookAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> JumpAction;

    // Kit D10 — sin assets todavía: si quedan null se crean en runtime
    // (Shift=sprint, clic izquierdo=empujón, Q=habilidad). Fase 6: assets.
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> SprintAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> EmpujonAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> HabilidadAction;

    // D18 (apagado por defecto): aguantar la respiración (Ctrl izquierdo).
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> AguantarAction;

private:
    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> KitIMC;

    // D10: sprint gastando stamina. La stamina la lleva el server; la velocidad
    // se aplica en ambos lados vía OnRep para que la predicción no divague.
    UPROPERTY(ReplicatedUsing=OnRep_Sprint)
    bool bSprint = false;

    // Segundos de sprint restantes (replicado para el HUD de Fase 5).
    UPROPERTY(Replicated)
    float StaminaActual = 0.f;

    // D18 — aguantando la respiración (server-auth con predicción local).
    UPROPERTY(Replicated)
    bool bAguantando = false;

    // Segundos de aguante restantes (replicado para el HUD de Fase 5).
    UPROPERTY(Replicated)
    float AguanteActual = 0.f;

    float UltimoEmpujonServerTime = -1000.f;
    float JadeoBloqueoHastaServerTime = -1000.f;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void OnSprintPressed();
    void OnSprintReleased();
    void OnEmpujon();
    void OnHabilidad();
    void OnAguantarPressed();
    void OnAguantarReleased();

    UFUNCTION(Server, Reliable) void Server_SetSprint(bool bNuevo);
    UFUNCTION(Server, Reliable) void Server_Empujar();
    UFUNCTION(Server, Reliable) void Server_SetAguantar(bool bNuevo);

    // Volúmenes locales por frame: crujido según velocidad; respiración solo
    // audible si el jugador LOCAL es cazador (D17) y la silla no está aguantando.
    void ActualizarAudio();

    UFUNCTION() void OnRep_Sprint();
    void AplicarVelocidad();

    const class USillasBalanceData* GetBalance() const;
};
