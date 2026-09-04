// Copyright Epic Games, Inc. All Rights Reserved.
// Widget NATIVO de menú radial: aparece en el Palette de UMG (categoría "Party Game") y se arrastra
// directo a cualquier WBP. Es data-driven: agregás slots en el panel de detalles y a cada slot le
// asignás un icono (textura o material). Los dibuja en círculo y resalta el que esté seleccionado.
//
// No usa widgets hijos: los slots son una lista editable (FPTRadialSlot). Toda la geometría (posición
// de cada slot por ángulo) y el resaltado los pinta el Slate SPTRadialMenu por código.
//
// Para la lógica de "qué slot está bajo el cursor" usá HitTestAbsolute()/SetHighlightedIndex().

#pragma once
#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Styling/SlateBrush.h"
#include "PTRadialMenu.generated.h"

class SPTRadialMenu;

/** Un slot del radial: su icono y una etiqueta opcional para identificarlo desde código. */
USTRUCT(BlueprintType)
struct FPTRadialSlot
{
    GENERATED_BODY()

    /** Icono del slot (textura o material a través del brush). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial") FSlateBrush Icon;

    /** Etiqueta opcional (ej. "Cube") para mapear el slot a algo desde C++/BP sin depender del orden. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial") FName Tag;
};

UCLASS()
class MYPARTYGAME_API UPTRadialMenu : public UWidget
{
    GENERATED_BODY()

public:
    /** Los slots del radial. Agregá/quitá desde el panel de detalles y ponele un icono a cada uno. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slots")
    TArray<FPTRadialSlot> Slots;

    // ── Layout ──────────────────────────────────────────────────────────────
    /** Distancia del centro al centro de cada icono (px). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Layout")
    float Radius = 160.f;
    /** Tamaño del icono de cada slot (px). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Layout")
    FVector2D IconSize = FVector2D(72.f, 72.f);
    /** Ángulo del PRIMER slot en grados (-90 = arriba). Los demás se reparten parejo. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Layout")
    float StartAngleDegrees = -90.f;
    /** true = los slots avanzan en sentido horario; false = antihorario. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Layout")
    bool bClockwise = true;

    // ── Porciones tipo "pizza" (fondo de cada slot) ───────────────────────────
    // Cada slot es una porción (sector anular) con el centro hueco (para ver el preview de la
    // herramienta). Se dibujan DETRÁS de los iconos, con degradado del centro hacia afuera.
    /** Dibujar las porciones de fondo. Si es false, solo se ven los iconos (comportamiento viejo). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    bool bDrawSlices = true;
    /** Radio INTERIOR de las porciones (el hueco central donde se ve el preview). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    float SliceInnerRadius = 70.f;
    /** Radio EXTERIOR de las porciones (borde de afuera de la pizza). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    float SliceOuterRadius = 240.f;
    /** Separación entre porciones, en grados (el "corte" de la pizza). 0 = pegadas. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    float SliceGapDegrees = 4.f;
    /** Color de la porción en el CENTRO (inicio del degradado). El alpha controla la transparencia. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    FLinearColor SliceColorInner = FLinearColor(0.05f, 0.05f, 0.08f, 0.55f);
    /** Color de la porción AFUERA (fin del degradado). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    FLinearColor SliceColorOuter = FLinearColor(0.15f, 0.15f, 0.22f, 0.85f);
    /** Color central de la porción cuando está resaltada (hover). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    FLinearColor SliceHoverColorInner = FLinearColor(0.20f, 0.35f, 0.55f, 0.70f);
    /** Color externo de la porción cuando está resaltada (hover). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    FLinearColor SliceHoverColorOuter = FLinearColor(0.35f, 0.55f, 0.85f, 0.95f);
    /** Color del contorno (outline) que aparece en la porción bajo el cursor. Alpha 0 lo apaga. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    FLinearColor OutlineColor = FLinearColor(1.f, 0.9f, 0.3f, 1.f);
    /** Grosor del contorno de hover (px). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Slices")
    float OutlineThickness = 2.5f;

    // ── Selección / resaltado ─────────────────────────────────────────────────
    /** Radio muerto central (px): dentro de él, HitTest devuelve -1 (ningún slot). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Selection")
    float DeadZoneRadius = 45.f;
    /** Brush opcional que se dibuja DETRÁS del slot resaltado (un aro/glow). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Highlight")
    FSlateBrush HighlightBrush;
    /** Escala del icono cuando está resaltado. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Highlight")
    float HighlightScale = 1.2f;
    /** Glow/outline: dibuja el MISMO icono agrandado y tintado detrás del slot resaltado (sigue la
     *  silueta del icono → se ve como un contorno luminoso). GlowColor con alpha 0 lo desactiva. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Highlight")
    FLinearColor GlowColor = FLinearColor(1.f, 0.9f, 0.2f, 0.9f);
    /** Cuánto se agranda la silueta del glow respecto al icono. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Highlight")
    float GlowScale = 1.55f;
    /** Tinte de los iconos normales y del resaltado. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Highlight")
    FLinearColor NormalTint = FLinearColor::White;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Radial|Highlight")
    FLinearColor HighlightTint = FLinearColor::White;

    // ── API ────────────────────────────────────────────────────────────────
    /** Resalta el slot Index (-1 = ninguno). */
    UFUNCTION(BlueprintCallable, Category="Radial") void SetHighlightedIndex(int32 Index);
    UFUNCTION(BlueprintPure,     Category="Radial") int32 GetHighlightedIndex() const { return HighlightedIndex; }
    UFUNCTION(BlueprintPure,     Category="Radial") int32 GetSlotCount() const { return Slots.Num(); }

    /** Slot bajo una posición ABSOLUTA de pantalla (Slate), usando la última geometría pintada.
     *  Devuelve -1 si está en la zona muerta o si no hay slots. */
    UFUNCTION(BlueprintCallable, Category="Radial") int32 HitTestAbsolute(FVector2D AbsScreenPos) const;

    /** Etiqueta del slot Index (None si Index inválido). */
    UFUNCTION(BlueprintPure, Category="Radial") FName GetSlotTag(int32 Index) const
    { return Slots.IsValidIndex(Index) ? Slots[Index].Tag : NAME_None; }

    virtual void SynchronizeProperties() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
#if WITH_EDITOR
    virtual const FText GetPaletteCategory() override;
#endif

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

private:
    int32 HighlightedIndex = -1;
    TSharedPtr<SPTRadialMenu> MyRadial;
};
