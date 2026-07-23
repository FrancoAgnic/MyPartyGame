// Copyright Epic Games, Inc. All Rights Reserved.
// Banco de palabras por defecto cargado desde un DataTable (NO hardcodeado). El usuario importa
// el CSV como DT_WordBank; C++ lo lee por ruta y cachea. Lo usan el SculptGameMode (palabras) y
// el lobby (categorías, derivadas del propio banco).

#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "PTMatchSettings.h"
#include "PTWordBank.generated.h"

// Fila del DataTable importado del CSV. Columnas: Name (índice, row name), Word, Category, Difficulty(1/2/3).
USTRUCT(BlueprintType)
struct FPTWordRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Word") FString Word;   // primaria (español)
    // Traducción al inglés (columna opcional del CSV/DataTable). Vacía = el inglés cae a Word.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Word") FString WordEn;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Word") FName   Category;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Word") int32   Difficulty = 2; // 1=Fácil 2=Media 3=Difícil
};

namespace PTWordBank
{
    // Ruta del DataTable (el asset que se importa del CSV). Único "hardcode": la ruta, no los datos.
    inline const TCHAR* DefaultTablePath() { return TEXT("/Game/Template/Data/DT_WordBank.DT_WordBank"); }

    // Banco por defecto (Word/Category/Difficulty). Carga el DataTable la primera vez y cachea.
    // Si el DataTable no existe, devuelve un fallback mínimo (para que el juego no quede sin palabras).
    const TArray<FPTWordEntry>& GetDefaultWords();

    // Categorías únicas del banco (para los checkboxes del lobby), ordenadas alfabéticamente.
    const TArray<FName>& GetDefaultCategories();
}
