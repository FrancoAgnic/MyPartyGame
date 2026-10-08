// Navegación de TODA la UI con joystick, sin tocar cada WBP.
//
// Un preprocesador de input de Slate ve el joystick ANTES que el juego. Cuando hay un menú en pantalla
// (cursor visible + botones interactuables), toma el control:
//   · cruceta / stick izq.  → mueve el foco al control más cercano en esa dirección (navegación espacial)
//   · A                     → activa (botón, checkbox, combo; un cuadro de texto recibe el teclado)
//   · izquierda/derecha     → sobre un slider o combo, lo ajusta en vez de moverse
//   · B                     → "volver": aprieta el botón Back/Cerrar/Cancelar visible, o simula Esc
//   · stick der.            → scrollea la lista donde está el foco
// El foco se dibuja con un borde (UPTGamepadFocusWidget) y simula el hover del botón (sonido/estilo).
//
// Solo cuentan los controles que están DE VERDAD arriba: se hace un hit-test de Slate en el centro
// de cada uno, así un popup con fondo tapa los botones de atrás y no se navega hacia ellos.
// Al mover el mouse se apaga (el mouse vuelve a mandar).

#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Tickable.h"
#include "InputCoreTypes.h"
#include "PTGamepadUINavigator.generated.h"

class UWidget;
class UPTGamepadFocusWidget;
class FPTGamepadNavProcessor;

UCLASS()
class MYPARTYGAME_API UPTGamepadUINavigator : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── FTickableGameObject ──
    virtual void Tick(float DeltaTime) override;
    virtual TStatId GetStatId() const override { RETURN_QUICK_DECLARE_CYCLE_STAT(UPTGamepadUINavigator, STATGROUP_Tickables); }
    virtual bool IsTickable() const override { return !IsTemplate(); }
    virtual bool IsTickableWhenPaused() const override { return true; }

    /** ¿El joystick está manejando un menú ahora? */
    bool IsMenuActive() const { return bMenuActive; }
    bool IsUsingGamepad() const { return bUsingGamepad; }

    /** Reasignar botones: el PRÓXIMO botón se entrega al callback (Esc / 6 s cancelan → EKeys::Invalid).
     *  bKeyboardMouse=true → captura teclas de TECLADO y botones del RATÓN (en vez de botones del joystick). */
    void BeginKeyCapture(TFunction<void(const FKey&)> OnKey, bool bKeyboardMouse = false);
    bool IsCapturingKey() const { return (bool)CaptureCallback; }
    bool IsCapturingKeyboardMouse() const { return (bool)CaptureCallback && bCaptureKeyboardMouse; }

    /** Abre el panel de configuración del joystick (lo usan el botón de Ajustes, Y en Ajustes y "PTGamepad"). */
    void OpenGamepadSettings();

    // Desde el preprocesador.
    bool HandleKey(const FKey& Key, bool bDown, bool bRepeat);
    void HandleAnalog(const FKey& Key, float Value);
    void HandleMouseActivity();

private:
    TSharedPtr<FPTGamepadNavProcessor> Processor;
    UPROPERTY() UPTGamepadFocusWidget* Highlight = nullptr;
    UPROPERTY() class UPTGamepadSettingsWidget* SettingsPanel = nullptr;

    TArray<TWeakObjectPtr<UWidget>> Candidates;
    TSet<const void*> PrevCandidateSet; // SWidgets del refresco anterior (para preferir lo NUEVO = popups)
    TWeakObjectPtr<UWidget> Focused;
    TWeakObjectPtr<UWidget> Hovered;    // al que le mandamos OnHovered (para el OnUnhovered)

    bool  bUsingGamepad = false;
    bool  bCursorHidden = false;        // escondimos el cursor del PC por estar con joystick
    void  UpdateCursorVisibility();
    bool  bMenuActive = false;
    float RefreshAccum = 1.f;
    TSet<FKey> ConsumedDown;            // para tragarse también el "soltar" de lo que se tragó

    FVector2D LeftStick  = FVector2D::ZeroVector;
    FVector2D RightStick = FVector2D::ZeroVector;
    FIntPoint StickDir   = FIntPoint::ZeroValue;
    float     StickRepeat = 0.f;

    TFunction<void(const FKey&)> CaptureCallback;
    double CaptureStart = 0.0;
    bool   bCaptureKeyboardMouse = false; // la captura actual espera teclado/ratón (no joystick)

    UWorld* GetGameWorld() const;
    bool ComputeMenuContext() const;
    void RefreshCandidates();
    bool IsUsable(UWidget* W) const;
    UWidget* PickDefault() const;
    void SetFocus(UWidget* W);
    void Navigate(const FVector2D& Dir);
    void Activate();
    bool Adjust(int32 Sign);
    void Back();
    void ScrollIntoView(UWidget* W);
    void ScrollBy(float Amount);
    void UpdateHighlight(float DeltaTime);
    bool IsSettingsPanelActive() const;
};
