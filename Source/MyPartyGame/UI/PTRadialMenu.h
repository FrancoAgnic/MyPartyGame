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
