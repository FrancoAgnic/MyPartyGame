// Copyright Epic Games, Inc. All Rights Reserved.
// Menú radial para elegir la forma del sello al esculpir. Se abre MANTENIENDO la tecla de forma;
// mientras se mantiene, se mueve el mouse hacia un slot (arriba/abajo/izquierda/derecha) y al soltar
// se selecciona esa forma. Con la RUEDA del mouse se cambia de PÁGINA (grupos de 4 formas).
//
// Selección CARDINAL (no diagonal): el slot se elige por el EJE DOMINANTE del cursor respecto al
// centro (|dx|>|dy| → izq/der; si no → arriba/abajo). Es mucho más rápido/directo que los sectores.
//
// Config data-driven: llená ShapeSlots en el WBP con {Shape, Icono, Nombre}. El radial arma las
// páginas de a 4 solo y le pone los iconos al UPTRadialMenu en runtime.
//
// En el WBP derivado (parent PTShapeRadialWidget):
//   - Poné un UPTRadialMenu (Palette → "Party Game") llamado "Radial", centrado. NO hace falta cargarle
//     los slots a mano: los pone el código desde ShapeSlots (4 por página).
//   - Layout recomendado del Radial: StartAngle -90 (slot 0 arriba), Clockwise (0=arriba,1=der,2=abajo,3=izq).

#pragma once
#include "CoreMinimal.h"
#include "../UI/PTUserWidget.h"
#include "Styling/SlateBrush.h"
#include "PTSculptVolume.h" // EPTStampShape
#include "PTShapeRadialWidget.generated.h"

class UPTRadialMenu;
class UImage;
class UTexture2D;

/** Una forma configurable del radial: su primitiva, su icono y un nombre para identificarla. */
USTRUCT(BlueprintType)
struct FPTShapeSlotDef
{
    GENERATED_BODY()

    /** Primitiva a esculpir (esfera, cubo, cono, pirámide, toroide, cápsula, prisma hex, octaedro...). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ShapeRadial") EPTStampShape Shape = EPTStampShape::Sphere;
    /** Icono del slot (textura/material vía brush). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ShapeRadial") FSlateBrush   Icon;
    /** Nombre opcional (solo para identificar en el editor). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="ShapeRadial") FText         Name;
};

UCLASS()
class MYPARTYGAME_API UPTShapeRadialWidget : public UPTUserWidget
{
    GENERATED_BODY()

public:
    /** Todas las formas disponibles. Se agrupan en PÁGINAS de 4 (cardinales). Editá esta lista en el WBP:
     *  agregá una entrada por forma con su icono. Se navega entre páginas con la rueda del mouse. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShapeRadial")
    TArray<FPTShapeSlotDef> ShapeSlots;

    /** Radio muerto central (px locales): si el cursor está más cerca que esto del centro no se
     *  selecciona nada (soltar ahí = mantener la forma actual). */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="ShapeRadial")
    float DeadZonePixels = 40.f;

    /** Prepara el radial abriendo en StartPage (para reabrir en la última página usada). */
    void BeginRadial(int32 StartPage = 0);

    /** Página actual (para que el PlayerController la recuerde entre aperturas). */
    int32 GetCurrentPage() const { return CurrentPage; }

    /** Recalcula qué slot (cardinal) está bajo el cursor. Lo llama el PlayerController cada tick. */
    void UpdateSelection();

    /** Cambia de página (rueda del mouse). Envuelve al llegar a los extremos. */
    void NextPage();
    void PrevPage();

    /** Índice del slot resaltado dentro de la página (-1 = ninguno / zona muerta). */
    UFUNCTION(BlueprintPure, Category="ShapeRadial")
    int32 GetSelectedIndex() const { return SelectedIndex; }

    /** Devuelve la forma seleccionada. false si está en zona muerta (→ mantener la forma actual). */
    bool GetSelectedShape(EPTStampShape& OutShape) const;

    /** El BP puede resaltar el slot si no usa el UPTRadialMenu (Index = -1 → ninguno). */
    UFUNCTION(BlueprintImplementableEvent, Category="ShapeRadial")
    void OnSelectionChanged(int32 NewIndex);

protected:
    virtual void NativeDestruct() override; // restaura el cursor del SO al cerrar

    /** UPTRadialMenu nativo dentro del WBP (recomendado): se le cargan los iconos de la página actual
     *  y se le maneja el resaltado. */
    UPROPERTY(meta=(BindWidgetOptional)) UPTRadialMenu* Radial = nullptr;

    /** Cursor custom (un DOT) que sigue al mouse mientras el radial está abierto. Ponelo dentro de un
     *  Canvas Panel en el WBP. Si está, se oculta el cursor del SO y se muestra solo este dot. */
    UPROPERTY(meta=(BindWidgetOptional)) UImage* CursorDot = nullptr;
    /** Textura del dot (asigná el dot BLANCO del color picker). */
    UPROPERTY(EditAnywhere, Category="ShapeRadial|Cursor") UTexture2D* CursorDotTexture = nullptr;
    /** Tamaño del dot en px. */
    UPROPERTY(EditAnywhere, Category="ShapeRadial|Cursor") FVector2D CursorDotSize = FVector2D(22.f, 22.f);

    /** Formas por página (cardinales). No tocar: la selección cardinal asume 4. */
    static constexpr int32 SlotsPerPage = 4;

private:
    int32 SelectedIndex = -1; // slot dentro de la página (0=arriba,1=der,2=abajo,3=izq)
    int32 CurrentPage   = 0;

    int32 NumPages() const;
    void  BuildPage();        // vuelca los 4 iconos de la página actual al UPTRadialMenu
};
