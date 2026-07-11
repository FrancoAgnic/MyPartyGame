// Copyright Epic Games, Inc. All Rights Reserved.
// Tipos compartidos: banco de palabras (palabra + categoría + dificultad) y la configuración de
// partida que el host arma en el lobby (tiempo de turno, rondas, % de revelado, categorías,
// dificultad, y palabras propias subidas por CSV). Los usan el lobby, el GameInstance y el
// SculptGameMode.

#pragma once
#include "CoreMinimal.h"
#include "PTMatchSettings.generated.h"

UENUM(BlueprintType)
enum class EPTWordDifficulty : uint8
{
    Facil   UMETA(DisplayName="Fácil"),
    Media   UMETA(DisplayName="Media"),
    Dificil UMETA(DisplayName="Difícil")
};

// Una palabra del banco, con su categoría y dificultad (para filtrar).
USTRUCT(BlueprintType)
struct FPTWordEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Word") FString Word;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Word") FName   Category;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Word") EPTWordDifficulty Difficulty = EPTWordDifficulty::Media;

    FPTWordEntry() {}
    FPTWordEntry(const FString& InWord, const FName& InCat, EPTWordDifficulty InDiff)
        : Word(InWord), Category(InCat), Difficulty(InDiff) {}
};

// Config de la partida que el host elige en el lobby. Viaja lobby→Lvl-01 por el GameInstance.
USTRUCT(BlueprintType)
struct FPTMatchSettings
{
    GENERATED_BODY()

    // Tuneables numéricos.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match") float TurnDuration   = 90.f; // seg de dibujo
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match") int32 NumRounds      = 3;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match") float RevealFraction = 0.7f; // 0..0.95

    // Filtros de palabras. Array vacío = sin filtrar (entran todas).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match") TArray<FName> ActiveCategories;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Match") TArray<EPTWordDifficulty> ActiveDifficulties;

    // Palabras subidas por el host (CSV). Si bUseCustomWords, reemplazan el banco default.
    UPROPERTY(BlueprintReadWrite, Category="Match") TArray<FPTWordEntry> CustomWords;
    UPROPERTY(BlueprintReadWrite, Category="Match") bool bUseCustomWords = false;
};
