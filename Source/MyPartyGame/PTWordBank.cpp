// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTWordBank.h"

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

    // Fallback mínimo si el DataTable no está importado todavía (así el juego no queda sin palabras).
    void SeedFallback()
    {
        using D = EPTWordDifficulty;
        GWords = {
            {TEXT("Gato"),TEXT("ANIMALES"),D::Facil},   {TEXT("Perro"),TEXT("ANIMALES"),D::Facil},
            {TEXT("Pez"),TEXT("ANIMALES"),D::Facil},     {TEXT("Pizza"),TEXT("COMIDA"),D::Facil},
            {TEXT("Manzana"),TEXT("COMIDA"),D::Facil},   {TEXT("Auto"),TEXT("VEHICULOS"),D::Facil},
            {TEXT("Barco"),TEXT("VEHICULOS"),D::Media},  {TEXT("Arbol"),TEXT("PLANTAS"),D::Facil},
            {TEXT("Sol"),TEXT("FENOMENOS_NATURALES"),D::Facil}, {TEXT("Casa"),TEXT("EDIFICIOS"),D::Facil},
            {TEXT("Mano"),TEXT("CUERPO_HUMANO"),D::Facil}, {TEXT("Robot"),TEXT("TECNOLOGIA"),D::Facil}
        };
    }

    void EnsureLoaded()
    {
        if (GLoaded) return;
        GLoaded = true;

        UDataTable* DT = LoadObject<UDataTable>(nullptr, PTWordBank::DefaultTablePath());
        if (!DT)
        {
            UE_LOG(LogTemp, Warning,
                TEXT("[WordBank] No encontré el DataTable en %s — usando fallback mínimo. Importá el CSV como DT_WordBank."),
                PTWordBank::DefaultTablePath());
            SeedFallback();
        }
        else
        {
            TSet<FName> SeenCats;
            for (const TPair<FName, uint8*>& Pair : DT->GetRowMap())
            {
                const FPTWordRow* Row = reinterpret_cast<const FPTWordRow*>(Pair.Value);
                if (!Row || Row->Word.IsEmpty()) continue;
                GWords.Add(FPTWordEntry(Row->Word, Row->Category, DiffFromInt(Row->Difficulty)));
                if (!Row->Category.IsNone()) SeenCats.Add(Row->Category);
            }
            GCategories = SeenCats.Array();
            UE_LOG(LogTemp, Log, TEXT("[WordBank] Cargadas %d palabras y %d categorías desde el DataTable."),
                   GWords.Num(), GCategories.Num());
        }

        // Categorías únicas ordenadas (para los checkboxes del lobby). Si vino del fallback, derivar acá.
        if (GCategories.Num() == 0)
        {
            TSet<FName> Seen;
            for (const FPTWordEntry& W : GWords) if (!W.Category.IsNone()) Seen.Add(W.Category);
            GCategories = Seen.Array();
        }
        GCategories.Sort([](const FName& A, const FName& B){ return A.LexicalLess(B); });
    }
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
