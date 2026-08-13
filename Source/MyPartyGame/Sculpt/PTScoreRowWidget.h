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
class UWidgetAnimation;
class USoundBase;

UCLASS()
class MYPARTYGAME_API UPTScoreRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Llenar la fila. bSculptor = este jugador esculpe este turno (muestra la imagen
     *  y le pone el contorno amarillo al nombre). */
    void SetRow(const FString& Name, int32 Score, bool bSculptor);

    /** Muestra/oculta el "+N" al lado del nombre cuando este jugador acaba de adivinar.
     *  bShow=false lo oculta. Requiere el TextBlock opcional TxtGuessPlus en el WBP de la fila. */
    void SetGuessPlus(int32 Points, bool bShow);

    /** Anima el puntaje subiendo de From a To en Duration segundos, con un "rebote" (escala) del
     *  número cada vez que sube un valor. Satisfactorio de ver. */
    void AnimateScore(int32 From, int32 To, float Duration);

protected:
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    // Imagen del "está esculpiendo" (asignar la textura en el WBP). Solo visible para el escultor.
    UPROPERTY(meta=(BindWidgetOptional)) UImage*     ImgBrush;
    // Nombre. Si NO existe TxtScore, acá va "Nombre: puntaje" (compatibilidad con el WBP viejo).
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtName;
    // Puntaje SOLO (opcional). Si existe, TxtName muestra solo el nombre y el número va acá. SIEMPRE visible.
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtScore;
    // Copia "fantasma/sombra" del número, superpuesta sobre TxtScore. La animación ScorePopAnim anima
    // ESTE (escala + fade), así el número real (TxtScore) nunca desaparece. C++ lo mantiene sincronizado.
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtScorePop;
    // "+N" que aparece unos segundos cuando el jugador adivina (opcional; ubicalo al lado del nombre).
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtGuessPlus;

    // Animación del rebote del puntaje (creala en la pestaña Animations del WBP con ESTE nombre:
    // "ScorePopAnim"). Si la asignás, se dispara en cada subida del conteo; si no, hay un rebote por
    // código como fallback. Debe llamarse igual que esta variable (regla de BindWidgetAnim de UMG).
    UPROPERTY(Transient, meta=(BindWidgetAnimOptional)) UWidgetAnimation* ScorePopAnim = nullptr;

    // Tick que suena UNA vez por cada punto que sube en el conteo animado (asignalo en el WBP de la fila).
    UPROPERTY(EditAnywhere, Category="Sound") USoundBase* SndPointTick = nullptr;

    // Contorno amarillo del nombre del escultor (editable en el WBP de la fila).
    UPROPERTY(EditAnywhere, Category="Score") FLinearColor SculptorOutlineColor = FLinearColor(1.f, 0.85f, 0.f, 1.f);
    UPROPERTY(EditAnywhere, Category="Score") int32        SculptorOutlineSize  = 2;

private:
    // Nombre + puntaje "final" para poder repintar el texto durante la animación de conteo.
    FString RowName;
    int32   FinalScore = 0;
    void    ApplyScoreText(int32 Value); // setea TxtName = "Nombre: Value"

    // Estado de la animación de conteo (rebote).
    bool  bAnimating   = false;
    int32 AnimFrom = 0, AnimTo = 0, LastShown = 0;
    float AnimElapsed = 0.f, AnimDuration = 0.f;
    float NumScale = 1.f;   // escala actual del número (rebote por código → vuelve a 1)
    float LastPopAt = -1.f; // throttle para no reiniciar la animación UMG demasiado seguido
};
