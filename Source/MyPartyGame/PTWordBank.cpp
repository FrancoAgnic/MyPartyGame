// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWordBank.h"
#include "PTTextTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// OJO: los nombres de este namespace anónimo NO pueden repetirse con los de otros .cpp del módulo,
// porque el unity build concatena varios .cpp en una misma unidad de traducción y colisionarían
// (le pasó a GLoaded/EnsureLoaded/CsvFullPath contra PTTextTable.cpp). Por eso van con prefijo WB.
namespace
{
    bool                 WB_GLoaded = false;
    TArray<FPTWordEntry> GWords;

    // Fallback mínimo si el CSV no está (así el juego no queda sin palabras). Solo el idioma de
    // referencia: si falta el banco, el problema es el banco, no las traducciones.
    void SeedFallback()
    {
        auto One = [](const TCHAR* W)
        {
            TArray<FString> Words; Words.Add(W);
            return FPTWordEntry(Words);
        };
        GWords = {
            One(TEXT("Gato")),  One(TEXT("Perro")), One(TEXT("Pez")),   One(TEXT("Pizza")),
            One(TEXT("Manzana")), One(TEXT("Auto")), One(TEXT("Barco")), One(TEXT("Árbol")),
            One(TEXT("Sol")),   One(TEXT("Casa")),  One(TEXT("Mano")),  One(TEXT("Robot"))
        };
    }

    FString WB_CsvFullPath()
    {
        return FPaths::ProjectContentDir() / PTWordBank::CsvRelativePath();
    }

    void WB_EnsureLoaded()
    {
        if (WB_GLoaded) return;
        WB_GLoaded = true;

        TArray<FString> Lines;
        const FString Path = WB_CsvFullPath();
        if (!FFileHelper::LoadFileToStringArray(Lines, *Path) ||
            !PTWordBank::ParseWordCsv(Lines, GWords))
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[WordBank] No pude leer %s — usando fallback mínimo."), *Path);
            SeedFallback();
        }
        else
        {
            UE_LOG(LogTemp, Log, TEXT("[WordBank] %d palabras desde %s."), GWords.Num(), *Path);
        }
    }
}

bool PTWordBank::ParseWordCsv(const TArray<FString>& Lines, TArray<FPTWordEntry>& OutWords)
{
    OutWords.Reset();
    if (Lines.Num() < 2) return false;

    TArray<FString> Header;
    PTText::SplitCsvLine(Lines[0], Header);

    // Las columnas se identifican POR NOMBRE, no por posición: así el CSV puede tener las columnas
    // en cualquier orden y el que sube un CSV propio no tiene que respetar un layout exacto.

    // Columna de CSV -> índice de idioma global (el de PTText). INDEX_NONE = no es un idioma.
    TArray<int32> ColToLang;
    ColToLang.Init(INDEX_NONE, Header.Num());

    const int32 NumLangs = FMath::Max(1, PTText::GetAvailableLanguages().Num());

    for (int32 c = 0; c < Header.Num(); ++c)
    {
        const FString H = Header[c].TrimStartAndEnd();
        // Categoría/dificultad ya no se usan: se ignoran si aparecen (compat con CSV viejos).
        if (H.Equals(TEXT("Category"),   ESearchCase::IgnoreCase)) continue;
        if (H.Equals(TEXT("Difficulty"), ESearchCase::IgnoreCase)) continue;
        if (H.Equals(TEXT("Name"),       ESearchCase::IgnoreCase)) continue; // índice de fila

        // Compatibilidad con los CSV viejos: "Word"/"WordEs" = idioma de referencia, "WordEn" = inglés.
        if (H.Equals(TEXT("Word"), ESearchCase::IgnoreCase) || H.Equals(TEXT("WordEs"), ESearchCase::IgnoreCase))
        { ColToLang[c] = 0; continue; }
        if (H.Equals(TEXT("WordEn"), ESearchCase::IgnoreCase))
        { ColToLang[c] = FMath::Max(0, PTText::GetLanguageIndex(TEXT("en"))); continue; }

        // Formato nuevo: el encabezado ES el código de idioma (ES, EN, PT...).
        const int32 Lang = PTText::GetLanguageIndex(H);
        if (Lang != INDEX_NONE) ColToLang[c] = Lang;
        else UE_LOG(LogTemp, Warning,
            TEXT("[WordBank] Columna '%s' ignorada: no es un idioma de UITexts.csv."), *H);
    }

    TArray<FString> Cells;
    for (int32 i = 1; i < Lines.Num(); ++i)
    {
        if (Lines[i].TrimStartAndEnd().IsEmpty()) continue;
        PTText::SplitCsvLine(Lines[i], Cells);

        FPTWordEntry E;
        E.Words.SetNum(NumLangs);
        for (int32 c = 0; c < Cells.Num() && c < ColToLang.Num(); ++c)
            if (ColToLang[c] != INDEX_NONE && E.Words.IsValidIndex(ColToLang[c]))
                E.Words[ColToLang[c]] = Cells[c].TrimStartAndEnd();

        if (!E.IsValidEntry()) continue; // fila sin ninguna palabra

        OutWords.Add(MoveTemp(E));
    }

    return OutWords.Num() > 0;
}

const TArray<FPTWordEntry>& PTWordBank::GetDefaultWords()
{
    WB_EnsureLoaded();
    return GWords;
}

void PTWordBank::Reload()
{
    WB_GLoaded = false;
    GWords.Reset();
    WB_EnsureLoaded();
}
