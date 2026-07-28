// Copyright Epic Games, Inc. All Rights Reserved.
// Banco de palabras cargado desde un CSV suelto (NO hardcodeado, NO DataTable).
//
// N IDIOMAS, igual que los textos de la UI: los idiomas salen de los ENCABEZADOS del CSV.
//     Name,Category,Difficulty,ES,EN
//     1,ANIMALES,1,ABEJA,BEE
// Para sumar portugués se agrega la columna PT y sus palabras — sin tocar C++, sin reimportar nada:
//     Name,Category,Difficulty,ES,EN,PT
//     1,ANIMALES,1,ABEJA,BEE,ABELHA
//
// Las columnas se mapean POR CÓDIGO contra PTText::GetAvailableLanguages(), no por posición: así el
// índice de idioma significa lo mismo en la UI y en las palabras aunque el orden difiera.
//
// Es un CSV suelto (y no un DataTable) por lo mismo que UITexts: un DataTable tiene las columnas
// fijadas en C++, así que cada idioma nuevo obligaría a recompilar.

#pragma once
#include "CoreMinimal.h"
#include "PTMatchSettings.h"

namespace PTWordBank
{
    /** Ruta del CSV, relativa al Content del proyecto (se empaqueta con la build). */
    inline const TCHAR* CsvRelativePath() { return TEXT("Localization/WordBank.csv"); }

    /** Banco por defecto. Carga el CSV la primera vez y cachea. */
    const TArray<FPTWordEntry>& GetDefaultWords();

    /** Categorías únicas del banco (para los checkboxes del lobby), ordenadas alfabéticamente. */
    const TArray<FName>& GetDefaultCategories();

    /** Vuelve a leer el CSV del disco. */
    void Reload();

    /** Parsea un CSV de palabras (el default, el que sube el host, o el de un mod).
     *  Acepta el formato con columnas de idioma y también el viejo de una sola columna "Word".
     *  Devuelve true si sacó al menos una palabra. */
    bool ParseWordCsv(const TArray<FString>& Lines, TArray<FPTWordEntry>& OutWords);
}
