// Copyright Epic Games, Inc. All Rights Reserved.
// Un cuadrito de la barra de herramientas del HUD (estilo hotbar): icono + tecla, y estado
// "equipado". Lo usan las tres barras: herramientas (1/2/3/4), formas (TAB) y atajos contextuales.
//
// En el WBP derivado (opcionales, poné los que uses):
//   IconImage      (Image)     → el icono de la herramienta/forma
//   KeyText        (TextBlock) → la tecla ("1", "TAB", "E"...)
//   LabelText      (TextBlock) → nombre corto ("Agregar", "Cubo"...)
//   SelectedMarker (cualquier widget) → borde/resaltado del equipado (se muestra/oculta solo)
// Para animaciones o estilos más finos del resaltado, usar el evento OnSelectedChanged en BP.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTToolSlotWidget.generated.h"

class UTextBlock;
class UImage;
class UTexture2D;

UCLASS()
class MYPARTYGAME_API UPTToolSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Llena el cuadrito. KeyName ya viene con el nombre lindo de la tecla (ej: "1", "Tab"). */
    UFUNCTION(BlueprintCallable, Category="UI")
    void SetSlot(UTexture2D* Icon, const FText& KeyName, const FText& Label);

    /** Marca/desmarca como equipado (mueve SelectedMarker y avisa a BP). */
    UFUNCTION(BlueprintCallable, Category="UI")
    void SetSelected(bool bInSelected);

    UFUNCTION(BlueprintPure, Category="UI")
    bool IsSelected() const { return bSelected; }

    /** Círculo de progreso (para el "mantener BACKSPACE 3s"). Alpha 0..1 y el contador del centro.
     *  Con Alpha <= 0 se oculta el círculo y el contador. */
    UFUNCTION(BlueprintCallable, Category="UI")
    void SetProgress(float Alpha01, const FText& CenterText);

protected:
    UPROPERTY(meta=(BindWidgetOptional)) UImage*     IconImage;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* KeyText;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* LabelText;
    UPROPERTY(meta=(BindWidgetOptional)) UWidget*    SelectedMarker;

    // ── Progreso circular (opcional) ─────────────────────────────────────────
    // ProgressRing: Image con un material radial que tenga un parámetro escalar de relleno
    // (por defecto "Percent"): 0 = vacío, 1 = círculo completo.
    UPROPERTY(meta=(BindWidgetOptional)) UImage*     ProgressRing;
    // Contador de segundos en el centro ("3", "2", "1").
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* ProgressText;

    /** Nombre del parámetro escalar de relleno del material del ProgressRing. */
    UPROPERTY(EditAnywhere, Category="UI")
    FName ProgressParamName = TEXT("Percent");

    /** Para animar/estilizar el progreso desde Blueprint (opcional). Alpha 0..1. */
    UFUNCTION(BlueprintImplementableEvent, Category="UI")
    void OnProgressChanged(float Alpha01);

    /** Para estilizar/animar el resaltado desde Blueprint (opcional). */
    UFUNCTION(BlueprintImplementableEvent, Category="UI")
    void OnSelectedChanged(bool bInSelected);

private:
    bool bSelected = false;

    // MID del material del ProgressRing (se crea la primera vez que se usa el progreso).
    UPROPERTY() class UMaterialInstanceDynamic* ProgressMID = nullptr;
    float LastProgress = -1.f;
};
