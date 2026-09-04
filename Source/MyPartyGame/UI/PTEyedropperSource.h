// Copyright Epic Games, Inc. All Rights Reserved.
// Interfaz que implementan los PlayerControllers que tienen un volumen de escultura pintable, para
// que el color picker (gotero) pueda pedirles el color EXACTO pintado bajo el cursor (raycast a la
// malla + lectura del atlas de pintura, sin luz ni tonemapping). Así el gotero recupera el color
// original tal cual se guardó, no el que se ve iluminado en pantalla.

#pragma once
#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PTEyedropperSource.generated.h"

UINTERFACE(MinimalAPI)
class UPTEyedropperSource : public UInterface
{
    GENERATED_BODY()
};

class IPTEyedropperSource
{
    GENERATED_BODY()
public:
    /** Devuelve en OutColor el color PINTADO exacto bajo el cursor (raymarch a la superficie del
     *  volumen + SampleWorldPaintColor). Devuelve false si el cursor no apunta a arcilla pintada
     *  (el llamador puede caer al muestreo de pantalla, aproximado). */
    virtual bool EyedropColorUnderCursor(FLinearColor& OutColor) const = 0;
};
