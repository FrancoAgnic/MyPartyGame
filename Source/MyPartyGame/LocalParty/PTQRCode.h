// Generador de códigos QR mínimo (modo byte, corrección de errores M, versiones 1–10).
// Alcanza para la URL del modo local ("http://192.168.0.10:8787" ≈ versión 2-3).
// Implementación según ISO/IEC 18004, estructura basada en el algoritmo de referencia de Nayuki.

#pragma once
#include "CoreMinimal.h"

class UTexture2D;

namespace PTQR
{
    /** Codifica el texto (UTF-8) en una matriz cuadrada de módulos (true = oscuro).
     *  ForcedMask = -1 elige la mejor máscara; 0..7 la fuerza (para tests).
     *  Devuelve false si el texto no entra en la versión 10. */
    MYPARTYGAME_API bool Encode(const FString& Text, TArray<bool>& OutModules, int32& OutSize, int32 ForcedMask = -1);

    /** Textura lista para un UImage: blanco con módulos negros + borde silencioso de 4 módulos.
     *  PixelsPerModule escala la imagen (filtro Nearest, se ve nítida al agrandarla). */
    MYPARTYGAME_API UTexture2D* MakeTexture(const FString& Text, int32 PixelsPerModule = 8);
}
