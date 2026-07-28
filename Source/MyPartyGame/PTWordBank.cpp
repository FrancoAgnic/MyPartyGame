// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWordBank.h"
#include "PTTextTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    bool                 GLoaded = false;
    TArray<FPTWordEntry> GWords;
    TArray<FName>        GCategories;

    EPTWordDifficulty DiffFromInt(int32 D)
    {
        if (D <= 1) return EPTWordDifficulty::Facil;
        if (D >= 3) return EPTWordDifficulty::Dificil;
        return EPTWordDifficulty::Media;
    }

    // Fallback mínimo si el CSV no está (así el juego no queda sin palabras). Solo el idioma de
    // referencia: si falta el banco, el problema es el banco, no las traducciones.
    void SeedFallback()
    {
        using D = EPTWordDifficulty;
        auto One = [](const TCHAR* W, const TCHAR* C, D Diff)
        {
            TArray<FString> Words; Words.Add(W);
            return FPTWordEntry(Words, FName(C), Diff);
        };
        GWords = {
            One(TEXT("Gato"),TEXT("ANIMALES"),D::Facil),   One(TEXT("Perro"),TEXT("ANIMALES"),D::Facil),
            One(TEXT("Pez"),TEXT("ANIMALES"),D::Facil),    One(TEXT("Pizza"),TEXT("COMIDA"),D::Facil),
            One(TEXT("Manzana"),TEXT("COMIDA"),D::Facil),  One(TEXT("Auto"),TEXT("VEHICULOS"),D::Facil),
            One(TEXT("Barco"),TEXT("VEHICULOS"),D::Media), One(TEXT("Arbol"),TEXT("PLANTAS"),D::Facil),
            One(TEXT("Sol"),TEXT("FENOMENOS_NATURALES"),D::Facil), One(TEXT("Casa"),TEXT("EDIFICIOS"),D::Facil),
            One(TEXT("Mano"),TEXT("CUERPO_HUMANO"),D::Facil), One(TEXT("Robot"),TEXT("TECNOLOGIA"),D::Facil)
        };
    }

    FString CsvFullPath()
    {
        return FPaths::ProjectContentDir() / PTWordBank::CsvRelativePath();
    }

    void EnsureLoaded()
    {
        if (GLoaded) return;
        GLoaded = true;

        TArray<FString> Lines;
        const FString Path = CsvFullPath();
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

        TSet<FName> Seen;
        for (const FPTWordEntry& W : GWords) if (!W.Category.IsNone()) Seen.Add(W.Category);
        GCategories = Seen.Array();
        GCategories.Sort([](const FName& A, const FName& B){ return A.LexicalLess(B); });
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
    int32 CatCol = INDEX_NONE, DiffCol = INDEX_NONE;

    // Columna de CSV -> índice de idioma global (el de PTText). INDEX_NONE = no es un idioma.
    TArray<int32> ColToLang;
    ColToLang.Init(INDEX_NONE, Header.Num());

    const int32 NumLangs = FMath::Max(1, PTText::GetAvailableLanguages().Num());

    for (int32 c = 0; c < Header.Num(); ++c)
    {
        const FString H = Header[c].TrimStartAndEnd();
        if (H.Equals(TEXT("Category"),   ESearchCase::IgnoreCase)) { CatCol  = c; continue; }
        if (H.Equals(TEXT("Difficulty"), ESearchCase::IgnoreCase)) { DiffCol = c; continue; }
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

        if (Cells.IsValidIndex(CatCol))  E.Category   = FName(*Cells[CatCol].TrimStartAndEnd());
        if (Cells.IsValidIndex(DiffCol)) E.Difficulty = DiffFromInt(FCString::Atoi(*Cells[DiffCol]));

        OutWords.Add(MoveTemp(E));
    }

    return OutWords.Num() > 0;
}

const TArray<FPTWordEntry>& PTWordBank::GetDefaultWords()
{
    EnsureLoaded();
    return GWords;
}

const TArray<FName>& PTWordBank::GetDefaultCategories()
{
    EnsureLoaded();
    return GCategories;
}

void PTWordBank::Reload()
{
    GLoaded = false;
    GWords.Reset();
    GCategories.Reset();
    EnsureLoaded();
}
