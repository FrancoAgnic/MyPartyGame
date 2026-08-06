// Copyright Epic Games, Inc. All Rights Reserved.
// FASE 2 — Pawn del cazador y LA CAMINATA DE COLA (D5, la firma del juego).
// Clic mantenido = modo captura (solo en fase Silencio, D5+D11): velocidad de
// caminata de cola (BalanceData) y el pawn camina DE ESPALDAS — la cola apunta
// hacia donde avanza. El "sentado" es automático al tener una silla a distancia
// y ángulo válidos detrás: el server resuelve rotura (silla-jugador) o dolor
// (señuelo) vía ASillasGameMode::ResolverSentado. Soltar el clic cancela.
// Validación SIEMPRE en el servidor; el cliente solo predice su propio estado
// visual (bCapturando local + corrección por OnRep).
// Greybox: caja alta 40x40x180 como cuerpo. El baile (D13) llega en Fase 3;
// la animación real de la caminata, en Fase 6.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SillasPawnCazador.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

UCLASS()
class MYPARTYGAME_API ASillasPawnCazador : public ACharacter
{
    GENERATED_BODY()

public:
    ASillasPawnCazador();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    bool EstaCapturando() const { return bCapturando; }
    bool EstaBailando() const   { return bBailando; }
    bool EstaAdolorido() const  { return bAdolorido; }

    // Solo servidor. Cancela el modo captura (fin de Silencio, o tras resolver un sentado).
    void CancelarCapturaServer();

    // Solo servidor. Castigo D6 por sentarse en un señuelo: 1.8s lento y sin poder capturar.
    void AplicarDolorServer();

    // Solo servidor. FASE 3 (D13): durante la Música el cazador baila — control
    // bloqueado y la cámara barre un patrón fijo y aprendible. Las sillas leen
    // el baile como los corredores leen al vigilante en luz roja/luz verde.
    void EmpezarBaileServer();
    void TerminarBaileServer();

protected:
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

    // Cuerpo greybox: caja alta, silueta bien distinta de una silla.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Sillas")
    TObjectPtr<UStaticMeshComponent> CuerpoMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<USpringArmComponent> SpringArm;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Camera")
    TObjectPtr<UCameraComponent> Camera;

    // Acciones de input (defaults: /Game/Input del template; el BP puede pisarlas).
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> LookAction;

    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> MouseLookAction;

    // Clic mantenido de captura. Sin asset todavía: si queda null se crea en
    // runtime un InputAction + contexto con clic izquierdo (Fase 6 lo vuelve asset).
    UPROPERTY(EditDefaultsOnly, Category="Input")
    TObjectPtr<UInputAction> CaptureAction;

private:
    // Contexto runtime del clic de captura (solo en el cliente que controla).
    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> CapturaIMC;

    UPROPERTY(ReplicatedUsing=OnRep_Estado)
    bool bCapturando = false;

    UPROPERTY(ReplicatedUsing=OnRep_Estado)
    bool bAdolorido = false;

    // D13: bailando durante la Música. El patrón se computa en cada máquina a
    // partir del tiempo de servidor sincronizado — todos ven el mismo barrido.
    UPROPERTY(ReplicatedUsing=OnRep_Estado)
    bool bBailando = false;

    UPROPERTY(Replicated)
    float BaileYawBase = 0.f;

    FTimerHandle DolorTimer;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void OnCapturaPressed();
    void OnCapturaReleased();

    UFUNCTION(Server, Reliable)
    void Server_SetCapturando(bool bNueva);

    UFUNCTION() void OnRep_Estado();

    // Velocidad y orientación según estado (corre en server y clientes: ambos
    // leen el mismo BalanceData para que la predicción no divague).
    void AplicarEstadoAMovimiento();

    // Solo servidor, cada Tick mientras captura: ¿hay silla válida detrás?
    void ChequearSentado();

    // Cada Tick mientras baila (todas las máquinas): rotación de la coreografía.
    void TickBaile();

    const class USillasBalanceData* GetBalance() const;
};
