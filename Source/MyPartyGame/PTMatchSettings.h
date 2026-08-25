// Copyright Epic Games, Inc. All Rights Reserved.
// Tipos compartidos: banco de palabras (solo la palabra en sus idiomas) y la configuración de
// partida que el host arma en el lobby (tiempo de turno, rondas, % de revelado, y palabras propias
// subidas por CSV). Los usan el lobby, el GameInstance y el SculptGameMode.
// (Categoría y dificultad se eliminaron: el banco es solo la lista de palabras — "menos es más".)

#pragma once
#include "CoreMinimal.h"
#include "PTMatchSettings.generated.h"

// Una palabra del banco, en sus distintos idiomas.
//
// N IDIOMAS: Words está indexado por el MISMO índice de idioma que la UI
// (PTText::GetAvailableLanguages()), así "el idioma 2" significa lo mismo en todo el juego.
// Sumar un idioma es agregar una columna al CSV — no se toca esta struct ni ningún otro código.
USTRUCT(BlueprintType)
struct FPTWordEntry
{
    GENERATED_BODY()

    /** Traducciones, una por idioma (mismo orden que PTText::GetAvailableLanguages()).
     *  Una entrada vacía = falta esa traducción → se usa el fallback de ForLang(). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Word") TArray<FString> Words;

    FPTWordEntry() {}
    explicit FPTWordEntry(const TArray<FString>& InWords) : Words(InWords) {}

    /** Traducción para ese índice de idioma. Si falta, cae a la primera que exista (nunca vacío). */
    FString ForLang(int32 LangIndex) const
    {
        if (Words.IsValidIndex(LangIndex) && !Words[LangIndex].IsEmpty()) return Words[LangIndex];
        for (const FString& S : Words) if (!S.IsEmpty()) return S;
        return FString();
    }

    /** Idioma de referencia (primera columna del CSV). Es el que se usa para logs y para el
     *  fallback general. */
    FString Primary() const { return ForLang(0); }

    bool IsValidEntry() const { return !Primary().IsEmpty(); }
};

// Config de la partida que el host elige en el lobby. Viaja lobby→Lvl-01 por el GameInstance.
USTRUCT(BlueprintType)
struct FPTMatchSettings
{
    GENERATED_BODY()

    // Tuneables numéricos.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match") float TurnDuration   = 90.f; // seg de dibujo
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match") int32 NumRounds      = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match") float RevealFraction = 0.3f; // 0..0.95 (default 30%, igual que el game mode)

    // Palabras subidas por el host (CSV). Si bUseCustomWords, reemplazan el banco default.
    UPROPERTY(BlueprintReadWrite, Category="Match") TArray<FPTWordEntry> CustomWords;
    UPROPERTY(BlueprintReadWrite, Category="Match") bool bUseCustomWords = false;
};
