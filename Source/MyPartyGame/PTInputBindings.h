// Copyright Epic Games, Inc. All Rights Reserved.
// FUENTE DE VERDAD ÚNICA de las teclas del esculpido.
//
// La usan LOS DOS lados:
//  - APTSculptPlayerController: para bindear (BindKey) → nunca hay teclas hardcodeadas sueltas.
//  - UPTGameplayHUDWidget: para mostrar una fila por acción en la UI.
// Así, cuando se agregue el rebind en el panel de Settings, alcanza con PTInput::SetKey y tanto
// el binding real como la UI quedan reflejados sin tocar dos lugares.
//
// Los overrides del usuario se guardan en UPTGameUserSettings (Config) por Id.

#pragma once
#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "PTInputBindings.generated.h"

USTRUCT(BlueprintType)
struct FPTKeyBinding
{
    GENERATED_BODY()

    /** Id estable (no traducir): con esto se guarda el rebind. */
    UPROPERTY(BlueprintReadOnly, Category="Input") FName Id;

    /** Clave de localización del nombre visible (ver DT_UITexts). */
    UPROPERTY(BlueprintReadOnly, Category="Input") FName LabelKey;

    /** Texto para la UI ya traducido al idioma actual. Lo rellena GetBindings() en cada llamada,
     *  así cambiar de idioma se refleja sin reiniciar. */
    UPROPERTY(BlueprintReadOnly, Category="Input") FText Label;

    /** Tecla actual (default o el override guardado). */
    UPROPERTY(BlueprintReadOnly, Category="Input") FKey Key;

    /** false = viene de Enhanced Input (WASD/cámara/volar): se muestra pero todavía no se rebindea. */
    UPROPERTY(BlueprintReadOnly, Category="Input") bool bRebindable = true;
};

namespace PTInput
{
    /** Lista ordenada de acciones con su tecla ACTUAL (defaults + overrides guardados). */
    MYPARTYGAME_API const TArray<FPTKeyBinding>& GetBindings();

    /** Tecla actual de una acción (EKeys::Invalid si el Id no existe). */
    MYPARTYGAME_API FKey GetKey(FName Id);

    /** Cambia la tecla de una acción y la persiste. Para el futuro panel de rebind: después de
     *  llamarla hay que re-bindear el input (APTSculptPlayerController::SetupInputComponent). */
    MYPARTYGAME_API void SetKey(FName Id, const FKey& NewKey);

    /** Vuelve a leer los overrides guardados (invalida la caché). */
    MYPARTYGAME_API void RefreshFromSettings();
}
