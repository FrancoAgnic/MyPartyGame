// FUENTE DE VERDAD del joystick (igual que PTInputBindings para el teclado).
//
//  · Tabla de acciones reasignables (Id → botón por defecto) + overrides guardados en
//    UPTGameUserSettings. APTSculptPlayerController bindea con GetKey(Id); el panel
//    UPTGamepadSettingsWidget la muestra y la cambia.
//  · Lectura de sticks con zona muerta y movimiento/cámara compartidos (partida y lobby), usando
//    la sensibilidad / invertir Y que elige el jugador.

#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class APlayerController;

struct FPTGamepadAction
{
    FName Id;
    FName LabelKey;   // clave de UITexts.csv
    FKey  DefaultKey;
    FKey  Key;        // actual (default u override)
};

namespace PTGamepad
{
    MYPARTYGAME_API const TArray<FPTGamepadAction>& GetActions();
    MYPARTYGAME_API FKey GetKey(FName Id);
    /** Asigna un botón a una acción y lo guarda. Si otra acción usaba ese botón, se intercambian. */
    MYPARTYGAME_API void SetKey(FName Id, const FKey& NewKey);
    MYPARTYGAME_API void ResetToDefaults();
    MYPARTYGAME_API void RefreshFromSettings();

    /** ¿Se puede asignar este botón a una acción? (botones/gatillos/cruceta; no los sticks como eje). */
    MYPARTYGAME_API bool IsAssignableKey(const FKey& Key);

    /** Nombre corto para la UI: "A", "RB", "LT", "↑"... */
    MYPARTYGAME_API FString KeyLabel(const FKey& Key);

    /** Línea de ayuda "RT Esculpir · LB Achicar · ..." con los botones actuales (para la TV). */
    MYPARTYGAME_API FString BuildHintLine();

    // ── Ajustes del jugador ──
    MYPARTYGAME_API float LookSensitivity();
    MYPARTYGAME_API float MoveSensitivity();
    MYPARTYGAME_API bool  InvertY();
    MYPARTYGAME_API float DeadZone();

    /** Stick con zona muerta RADIAL reescalada (sin salto al salir de la zona muerta). */
    MYPARTYGAME_API FVector2D ReadStick(const APlayerController* PC, const FKey& X, const FKey& Y);

    /** Mueve el pawn con el stick izquierdo y la cámara con el derecho (respeta IgnoreMove/LookInput).
     *  YawSpeed/PitchSpeed en grados/seg a fondo, antes de multiplicar por la sensibilidad. */
    MYPARTYGAME_API void TickMoveLook(APlayerController* PC, float DeltaTime, bool bAllowMove, bool bAllowLook,
                                      float YawSpeed, float PitchSpeed, float Exponent, bool bExtraInvertY = false);
}
