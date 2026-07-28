// Copyright Epic Games, Inc. All Rights Reserved.
// Localización de la UI por tabla en runtime, con soporte para N idiomas.
//
// Por qué NO usamos el pipeline nativo de UE (.locres): con pocos idiomas y un solo mantenedor, el
// gather/compile del Localization Dashboard obliga a recompilar la localización por cada texto que
// se toca. Acá los textos viven en un CSV que se lee directo: editás una celda y listo.
//
// ESCALABLE A MÁS IDIOMAS SIN TOCAR C++: los idiomas salen de los ENCABEZADOS del CSV.
//     Key,ES,EN
//     MENU_PLAY,Jugar,Play
// Para sumar portugués alcanza con agregar la columna PT y sus textos:
//     Key,ES,EN,PT
//     MENU_PLAY,Jugar,Play,Jogar
// No hay que recompilar, ni reimportar un DataTable, ni declarar nada: el juego descubre "PT" solo
// y aparece como idioma disponible. La primera columna de idioma es la de referencia (el fallback
// cuando a una fila le falta una traducción).
//
// El idioma sale de UPTGameUserSettings::LanguageCode (NUESTRA preferencia), no de la cultura del
// motor, que falla en silencio si no hay datos de localización instalados.
//
// Uso desde C++:       PTText::Get(TEXT("MENU_PLAY"))   -> FText ya traducido
// Uso desde Blueprint: nodo "PT Text".
// Pero normalmente NO hace falta nada de eso: UPTLocalizationSubsystem traduce los widgets solo.

#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PTTextTable.generated.h"

class UUserWidget;

/** Un idioma disponible, descubierto desde los encabezados del CSV. */
USTRUCT(BlueprintType)
struct FPTLanguage
{
    GENERATED_BODY()

    /** Código corto tal cual está en el encabezado, en minúscula ("es", "en", "pt"). */
    UPROPERTY(BlueprintReadOnly, Category="Localización") FString Code;

    /** Nombre para mostrar en Configuración ("Español", "English"). Ver PT_LANGUAGE_NAME abajo. */
    UPROPERTY(BlueprintReadOnly, Category="Localización") FText DisplayName;
};

namespace PTText
{
    /** Ruta del CSV, relativa al Content del proyecto (se copia igual a la build empaquetada). */
    inline const TCHAR* CsvRelativePath() { return TEXT("Localization/UITexts.csv"); }

    // Texto en el idioma actual. Si falta la clave devuelve "?CLAVE?" (para detectarla de una).
    MYPARTYGAME_API FText   Get(FName Key);
    MYPARTYGAME_API FString GetStr(FName Key);

    // Igual que Get pero rellenando "{0}", "{1}", ... con los argumentos dados.
    MYPARTYGAME_API FText Format(FName Key, const FFormatOrderedArguments& Args);

    /** Idiomas que trae el CSV, en el orden de sus columnas. Configuración arma los botones con esto. */
    MYPARTYGAME_API const TArray<FPTLanguage>& GetAvailableLanguages();

    /** true si ese código existe en el CSV (para no dejar guardado un idioma que no está). */
    MYPARTYGAME_API bool IsLanguageAvailable(const FString& Code);

    /** Índice del idioma activo. ES EL MISMO índice que usa FPTWordEntry::Words, así "idioma 2"
     *  significa lo mismo en la UI y en las palabras a adivinar. 0 si no hay nada cargado. */
    MYPARTYGAME_API int32 GetCurrentLanguageIndex();

    /** Índice de un código ("pt"), o INDEX_NONE si ese idioma no está en el CSV. */
    MYPARTYGAME_API int32 GetLanguageIndex(const FString& Code);

    /** Parte una línea de CSV respetando comillas (para que un texto pueda llevar comas).
     *  Vive acá porque lo usan tanto los textos como el banco de palabras. */
    MYPARTYGAME_API void SplitCsvLine(const FString& Line, TArray<FString>& Out);

    // ── Traducción AUTOMÁTICA de widgets ────────────────────────────────────────────────────
    // Recorre el árbol de un widget y traduce cada texto que coincida con alguna fila de la tabla
    // (compara contra TODOS los idiomas, así funciona en cualquier sentido y se puede llamar N veces).
    // No hace falta bindear nada en el Editor: se escribe el texto en español y esto lo cambia.
    //
    // Por qué es seguro con el texto dinámico (marcadores, nombres, relojes, la palabra a adivinar):
    // solo se reemplaza lo que COINCIDE EXACTO con una fila conocida. "Franco: 40" o "3 seg" no
    // coinciden con nada, así que se dejan intactos.
    MYPARTYGAME_API void LocalizeWidgetTree(UUserWidget* Widget);

    // Vuelve a leer el CSV del disco (para iterar textos sin reiniciar el juego).
    MYPARTYGAME_API void Reload();

    // Avisa que cambió el idioma. Lo dispara UPTGameUserSettings::SetLanguageCode (único punto
    // por donde se cambia). Se suscribe UPTLocalizationSubsystem para re-traducir lo que esté en
    // pantalla, y los widgets que arman su texto UNA sola vez (p. ej. la hotbar del HUD).
    DECLARE_MULTICAST_DELEGATE(FPTOnLanguageChanged);
    MYPARTYGAME_API FPTOnLanguageChanged& OnLanguageChanged();
}

/** Acceso desde Blueprint. */
UCLASS()
class MYPARTYGAME_API UPTTextLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Traduce de una todos los textos de este widget (y de los widgets que contiene).
        Normalmente NO hace falta llamarlo: el subsistema lo hace solo. */
    UFUNCTION(BlueprintCallable, Category="Sculpturillo|Localización")
    static void PTLocalizeWidget(UUserWidget* Widget);

    /** Texto localizado de la clave dada, según el idioma elegido en Configuración. */
    UFUNCTION(BlueprintPure, Category="Sculpturillo|Localización", meta=(DisplayName="PT Text"))
    static FText PTText(FName Key);

    /** Idiomas disponibles (los que trae el CSV). Para armar los botones de Configuración. */
    UFUNCTION(BlueprintPure, Category="Sculpturillo|Localización")
    static TArray<FPTLanguage> PTGetLanguages();

    /** Código del idioma activo ("es", "en", ...). */
    UFUNCTION(BlueprintPure, Category="Sculpturillo|Localización")
    static FString PTGetCurrentLanguage();

    /** Cambia el idioma y lo guarda. Traduce la pantalla en el acto. */
    UFUNCTION(BlueprintCallable, Category="Sculpturillo|Localización")
    static void PTSetLanguage(const FString& Code);
};
