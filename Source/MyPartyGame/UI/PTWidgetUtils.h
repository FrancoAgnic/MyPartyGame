// Utilidades de UMG compartidas.

#pragma once
#include "CoreMinimal.h"

class UUserWidget;
class UButton;

namespace PTWidgetUtils
{
    /** Crea un botón igual a Src (estilo, texto, slot) y lo inserta justo después en su caja
     *  vertical/horizontal. Devuelve null si Src no está en una Vertical/Horizontal Box (p. ej. en un
     *  Canvas: ahí no sabemos dónde ubicarlo y hay que agregarlo en el WBP). */
    MYPARTYGAME_API UButton* CloneButtonAfter(UUserWidget* Owner, UButton* Src, FName Name, const FText& Label);
}
