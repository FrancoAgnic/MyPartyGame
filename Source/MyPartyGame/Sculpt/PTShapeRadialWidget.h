// Copyright Epic Games, Inc. All Rights Reserved.
// Menú radial para elegir la forma del sello al esculpir. Se abre MANTENIENDO la tecla de forma
// (antes "Tab" que ciclaba): mientras se mantiene, se arrastra el mouse hacia un slot y al soltar
// se selecciona esa forma. Toda la lógica (qué slot está bajo el cursor) vive acá en C++; el WBP
// solo aporta los visuales de los slots y dibuja el resaltado en OnSelectionChanged.
//
// En el WBP derivado (parent PTShapeRadialWidget):
//   - Ubicá los slots en círculo. El slot 0 va ARRIBA y el orden es HORARIO (0=arriba, 1=derecha,
//     2=abajo, 3=izquierda para 4 formas). Hacé que el widget ocupe el centro de la pantalla.
//   - Implementá el evento OnSelectionChanged(Index) para resaltar el slot Index (-1 = ninguno).
//   - Ajustá SlotShapes para que el orden matchee cómo pusiste los slots en pantalla.

#pragma once
#include "CoreMinimal.h"
#include "../UI/PTUserWidget.h"
#include "PTSculptVolume.h" // EPTStampShape
#include "PTShapeRadialWidget.generated.h"

class UPTRadialMenu;

UCLASS()
class MYPARTYGAME_API UPTShapeRadialWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Formas por slot, en orden horario desde arriba. Editable en el WBP para matchear el layout. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShapeRadial")
    TArray<EPTStampShape> SlotShapes = { EPTStampShape::Sphere, EPTStampShape::Cube,
                                         EPTStampShape::Cylinder, EPTStampShape::TriPrism };

    /** Radio muerto central (en px locales): si el cursor está más cerca que esto del centro no se
     *  selecciona nada (soltar ahí = mantener la forma actual). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShapeRadial")
    float DeadZonePixels = 45.f;

    /** Recalcula qué slot está bajo el cursor. Lo llama el PlayerController en cada tick mientras
     *  el radial está abierto. */
    void UpdateSelection();

    /** Índice del slot resaltado (-1 = ninguno / zona muerta). */
    UFUNCTION(BlueprintPure, Category="ShapeRadial")
    int32 GetSelectedIndex() const { return SelectedIndex; }

    /** Devuelve la forma seleccionada. false si está en zona muerta (→ mantener la forma actual). */
    bool GetSelectedShape(EPTStampShape& OutShape) const;

    /** El BP dibuja el resaltado del slot (Index = -1 → ninguno). Si usás un UPTRadialMenu (abajo)
     *  no hace falta implementarlo: el resaltado lo maneja ese widget solo. */
    UFUNCTION(BlueprintImplementableEvent, Category="ShapeRadial")
    void OnSelectionChanged(int32 NewIndex);

protected:
    /** OPCIONAL: si ponés un UPTRadialMenu (widget nativo del Palette) llamado "Radial" dentro del
     *  WBP, se usa su geometría y hit-test, y se le maneja el resaltado automáticamente. El orden de
     *  sus slots debe matchear SlotShapes (slot 0 = SlotShapes[0], etc.). */
    UPROPERTY(meta=(BindWidgetOptional)) UPTRadialMenu* Radial = nullptr;

private:
    int32 SelectedIndex = -1;
};
