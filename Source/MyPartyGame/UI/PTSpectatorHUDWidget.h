// Copyright Epic Games, Inc. All Rights Reserved.
// HUD mínimo del modo ESPECTADOR (dev). Se arma 100% por código (sin WBP): una bandera grande anclada
// al CENTRO-DERECHA de la pantalla, que muestra el idioma del jugador cuyo POV estás mirando.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTSpectatorHUDWidget.generated.h"

class UImage;
class UCanvasPanel;
class UTexture2D;

UCLASS()
class MYPARTYGAME_API UPTSpectatorHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Muestra la bandera dada (grande, centro-derecha). nullptr = la oculta. */
    void SetFlag(UTexture2D* Flag);

    /** Tamaño de la bandera en px (ancho x alto). */
    UPROPERTY(EditAnywhere, Category="Spectator") FVector2D FlagSize = FVector2D(150.f, 150.f); // cuadrado (la textura es 512x512)
    /** Separación desde el borde derecho, en px. */
    UPROPERTY(EditAnywhere, Category="Spectator") float RightMargin = 48.f;

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;

    UPROPERTY(Transient) UCanvasPanel* RootCanvas = nullptr;
    UPROPERTY(Transient) UImage*       FlagImg    = nullptr;
};
