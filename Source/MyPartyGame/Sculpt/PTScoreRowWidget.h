// Una fila del marcador en vivo: nombre + puntaje, con una imagen (icono de pincel) y
// un contorno amarillo en el nombre cuando ese jugador es el escultor del turno.
// El WBP_ScoreRow solo aporta los widgets (Img del pincel + TextBlock del nombre); toda
// la lógica de "mostrar el pincel / resaltar" vive acá en C++.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTScoreRowWidget.generated.h"

class UImage;
class UTextBlock;

UCLASS()
class MYPARTYGAME_API UPTScoreRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Llenar la fila. bSculptor = este jugador esculpe este turno (muestra la imagen
     *  y le pone el contorno amarillo al nombre). */
    void SetRow(const FString& Name, int32 Score, bool bSculptor);

protected:
    // Imagen del "está esculpiendo" (asignar la textura en el WBP). Solo visible para el escultor.
    UPROPERTY(meta=(BindWidgetOptional)) UImage*     ImgBrush;
    // "Nombre: puntaje".
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtName;

    // Contorno amarillo del nombre del escultor (editable en el WBP de la fila).
    UPROPERTY(EditAnywhere, Category="Score") FLinearColor SculptorOutlineColor = FLinearColor(1.f, 0.85f, 0.f, 1.f);
    UPROPERTY(EditAnywhere, Category="Score") int32        SculptorOutlineSize  = 2;
};
