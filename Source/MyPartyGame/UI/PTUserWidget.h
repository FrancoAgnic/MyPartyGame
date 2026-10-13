// Copyright Epic Games, Inc. All Rights Reserved.
// Clase base de TODOS los widgets de UI del juego. Trabajo extra:
//  1) al construirse, aplica los sonidos de UI (hover/click) del GameInstance a TODOS los botones.
//  2) provee PlayPopIn(): animación "blop" (escala 0→1 con un pequeño rebote) para popups.
//  3) logo de Sculpturillo opcional, categoría "Branding" en Details.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Engine/Texture2D.h"
#include "PTUserWidget.generated.h"

UCLASS()
class MYPARTYGAME_API UPTUserWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Reproduce la animación "blop": escala el widget de 0 a 1 con un pequeño rebote (overshoot).
     *  Los popups la llaman al mostrarse (en ShowPanel / Setup / NativeConstruct). Va por TIMER (no
     *  depende de NativeTick, que en modo Auto puede no correr si el WBP no implementa Tick). */
    UFUNCTION(BlueprintCallable, Category="PopIn")
    void PlayPopIn();
    /** Igual que PlayPopIn pero animando un WIDGET HIJO (p.ej. un panel interno como Game Settings),
     *  no el widget entero. El timer lo maneja este widget. */
    UFUNCTION(BlueprintCallable, Category="PopIn")
    void PlayPopInOn(class UWidget* Target);

    // Detecta cuándo se muestra/oculta el widget (para bGameLogoHidesOthers), venga de donde venga.
    virtual void SetVisibility(ESlateVisibility InVisibility) override;

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

    // Duración del blop (segundos). Editable por si se quiere más rápido/lento.
    UPROPERTY(EditAnywhere, Category="PopIn") float PopInDuration = 0.5f;
    // Si es true, el blop se reproduce solo al construirse (para popups que se crean ya visibles).
    UPROPERTY(EditAnywhere, Category="PopIn") bool  bAutoPopIn = false;

    // ── Logo del juego (Sculpturillo) ────────────────────────────────────────────────────────────
    // Si está prendido, al construirse el widget C++ agrega el logo encima de todo (no bloquea clicks).
    // Viene prendido en el menú principal, el HUD de la partida, la pausa y settings; en cualquier otro
    // WBP se puede prender desde Details. Si el WBP ya trae un Image llamado "GameLogo", se usa ese
    // (con su layout del WBP) y C++ solo le pone la textura.
    UPROPERTY(EditAnywhere, Category="Branding") bool bShowGameLogo = false;
    UPROPERTY(EditAnywhere, Category="Branding", meta=(EditCondition="bShowGameLogo"))
    TSoftObjectPtr<UTexture2D> GameLogoTexture = TSoftObjectPtr<UTexture2D>(
        FSoftObjectPath(TEXT("/Game/Template/UI/Branding/T_SculpturilloLogo.T_SculpturilloLogo")));
    // Ancho en pantalla (unidades de UI a 1080p); el alto sale de la proporción de la textura.
    UPROPERTY(EditAnywhere, Category="Branding", meta=(EditCondition="bShowGameLogo", ClampMin="16"))
    float GameLogoWidth = 240.f;
    // Punto de la pantalla donde se engancha, que también es el punto del logo que se apoya ahí:
    // (1,0) = arriba a la derecha, (0,0) = arriba a la izquierda, (0.5,0.5) = centro, (1,1) = abajo a la derecha.
    UPROPERTY(EditAnywhere, Category="Branding", meta=(EditCondition="bShowGameLogo"))
    FVector2D GameLogoAnchor = FVector2D(1.f, 0.f);
    // Corrimiento desde ese punto (X+ = derecha, Y+ = abajo).
    UPROPERTY(EditAnywhere, Category="Branding", meta=(EditCondition="bShowGameLogo"))
    FVector2D GameLogoOffset = FVector2D(-28.f, 22.f);
    UPROPERTY(EditAnywhere, Category="Branding", meta=(EditCondition="bShowGameLogo", ClampMin="0", ClampMax="1"))
    float GameLogoOpacity = 0.9f;
    // En vez de un punto fijo de la pantalla, apoyar el logo sobre el borde de ARRIBA del panel principal
    // del widget (el hijo visible más grande de la raíz) y seguirlo si cambia de alto. Offset se suma.
    UPROPERTY(EditAnywhere, Category="Branding", meta=(EditCondition="bShowGameLogo"))
    bool bGameLogoOnPanel = false;
    // Qué parte del alto del logo se mete dentro del panel (0 = justo encima, 0.5 = centrado en el borde).
    UPROPERTY(EditAnywhere, Category="Branding", meta=(EditCondition="bShowGameLogo && bGameLogoOnPanel", ClampMin="0", ClampMax="1"))
    float GameLogoPanelOverlap = 0.3f;
    // Ventana que "se queda" con el logo mientras está abierta (pausa, settings): se ve SOLO el logo de la
    // última de estas que se abrió y se esconden todos los demás (el de la esquina incluido) → nunca hay dos.
    UPROPERTY(EditAnywhere, Category="Branding", meta=(EditCondition="bShowGameLogo"))
    bool bGameLogoHidesOthers = false;

    UPROPERTY(meta=(BindWidgetOptional)) class UImage* GameLogo = nullptr;

    /** Además de bShowGameLogo: las subclases lo pisan para esconder el logo en algún contexto
     *  (p.ej. el HUD en el editor de mapas). */
    virtual bool ShouldShowGameLogo() const { return bShowGameLogo; }
    /** Crea el logo (una sola vez) y le aplica textura/tamaño/visibilidad. Se llama en NativeConstruct. */
    void ApplyGameLogo();

private:
    void PopInStep();
    void UpdateGameLogoVisibility();
    void UpdateGameLogoOnPanel();        // bGameLogoOnPanel: reubica el logo sobre el panel (timer)
    void SetGameLogoOwner(bool bOwner);  // bGameLogoHidesOthers: entra/sale de la pila de dueños del logo
    static void RefreshGameLogos(UWorld* World); // aplica "solo se ve el logo del dueño de arriba" a todos
    bool bGameLogoSuppressed    = false; // hay otra ventana abierta que se quedó con el logo
    bool bGameLogoCreated       = false; // lo creó C++ (no vino del WBP) → C++ maneja tamaño y opacidad
    bool bGameLogoPlaced        = false; // bGameLogoOnPanel: ya se ubicó sobre el panel (antes no se muestra)
    FTimerHandle GameLogoPanelTimer;

    FTimerHandle PopInTimer;
    float PopInElapsed = 0.f;
    TWeakObjectPtr<class UWidget> PopInTarget; // qué se anima (this por defecto, o un hijo)
};
