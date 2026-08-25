// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTTextTable.h"
#include "PTGameUserSettings.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/EditableTextBox.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
    bool GLoaded = false;

    /** Idiomas descubiertos en los encabezados, en orden de columna. */
    TArray<FPTLanguage> GLanguages;

    /** clave → (índice de idioma → texto). El índice es el mismo de GLanguages. */
    TMap<FName, TArray<FString>> GTexts;

    /** Índice inverso "texto → clave", con TODOS los idiomas, para la traducción automática. */
    TMap<FString, FName> GReverse;

    // Clave de comparación para la traducción automática: sin distinguir mayúsculas, sin espacios de
    // más, e IGNORANDO signos decorativos al inicio/fin (¡ ¿ ! ? - – — . * _). Así un texto del WBP
    // matchea la fila del CSV aunque difiera solo en esos signos (p.ej. "Todos adivinaron!" vs la fila
    // "¡Todos adivinaron!", o "-No hay amigos-" vs "No hay amigos"). El texto MOSTRADO sale igual del
    // CSV, así que queda bien escrito. Se aplica igual al construir el índice y al buscar → simétrico.
    FString NormalizeForLookup(const FString& S)
    {
        FString T = S.TrimStartAndEnd();
        auto IsDeco = [](TCHAR C)
        {
            return C == TEXT('!') || C == TEXT('?') ||
                   C == TCHAR(0x00A1) || C == TCHAR(0x00BF) ||                 // ¡ ¿
                   C == TEXT('-') || C == TCHAR(0x2013) || C == TCHAR(0x2014) || // - – —
                   C == TEXT('.') || C == TEXT('*') || C == TEXT('_') ||
                   FChar::IsWhitespace(C);
        };
        int32 A = 0, B = T.Len();
        while (A < B && IsDeco(T[A]))     ++A;
        while (B > A && IsDeco(T[B - 1])) --B;
        return T.Mid(A, B - A).ToLower();
    }

    /** Nombre lindo de un idioma. Si sale uno nuevo y no está acá, se muestra el código en mayúscula
     *  (así agregar un idioma NO obliga a tocar código; esto es solo cosmético). */
    FText PrettyLanguageName(const FString& Code)
    {
        static const TMap<FString, FString> Known = {
            {TEXT("es"), TEXT("Español")},   {TEXT("en"), TEXT("English")},
            {TEXT("pt"), TEXT("Português")}, {TEXT("fr"), TEXT("Français")},
            {TEXT("de"), TEXT("Deutsch")},   {TEXT("it"), TEXT("Italiano")},
            {TEXT("ja"), TEXT("日本語")},     {TEXT("ko"), TEXT("한국어")},
            {TEXT("zh"), TEXT("中文")},       {TEXT("ru"), TEXT("Русский")},
            {TEXT("pl"), TEXT("Polski")},    {TEXT("tr"), TEXT("Türkçe")},
        };
        if (const FString* Found = Known.Find(Code)) return FText::FromString(*Found);
        return FText::FromString(Code.ToUpper());
    }

    FString CsvFullPath()
    {
        return FPaths::ProjectContentDir() / PTText::CsvRelativePath();
    }
}

/** Parte una línea de CSV respetando comillas, para que un texto pueda llevar comas:
 *  MENU_HI,"Hola, mundo","Hi, world"   */
void PTText::SplitCsvLine(const FString& Line, TArray<FString>& Out)
{
    Out.Reset();
    FString Cur;
    bool bInQuotes = false;

    for (int32 i = 0; i < Line.Len(); ++i)
    {
        const TCHAR C = Line[i];
        if (C == TEXT('"'))
        {
            // Comilla doble dentro de comillas ("") = una comilla literal.
            if (bInQuotes && i + 1 < Line.Len() && Line[i + 1] == TEXT('"')) { Cur.AppendChar(TEXT('"')); ++i; }
            else bInQuotes = !bInQuotes;
        }
        else if (C == TEXT(',') && !bInQuotes) { Out.Add(Cur); Cur.Reset(); }
        else Cur.AppendChar(C);
    }
    Out.Add(Cur);
}

namespace
{
    void EnsureLoaded()
    {
        if (GLoaded) return;
        GLoaded = true;

        TArray<FString> Lines;
        const FString Path = CsvFullPath();
        if (!FFileHelper::LoadFileToStringArray(Lines, *Path) || Lines.Num() < 2)
        {
            // Sin CSV la UI queda en el idioma en que está escrita en los WBP (español): se degrada
            // en vez de romperse, y el warning dice exactamente qué falta.
            UE_LOG(LogTemp, Warning, TEXT("[UITexts] No pude leer %s — la UI queda sin traducir."), *Path);
            return;
        }

        // ── Encabezado: la columna 0 es la clave, el resto son los idiomas ──
        TArray<FString> Header;
        PTText::SplitCsvLine(Lines[0], Header);
        for (int32 c = 1; c < Header.Num(); ++c)
        {
            const FString Code = Header[c].TrimStartAndEnd().ToLower();
            if (Code.IsEmpty()) continue;
            FPTLanguage Lang;
            Lang.Code        = Code;
            Lang.DisplayName = PrettyLanguageName(Code);
            GLanguages.Add(Lang);
        }
        if (GLanguages.Num() == 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("[UITexts] %s no declara ninguna columna de idioma."), *Path);
            return;
        }

        // ── Filas ──
        TArray<FString> Cells;
        for (int32 i = 1; i < Lines.Num(); ++i)
        {
            if (Lines[i].TrimStartAndEnd().IsEmpty()) continue;
            PTText::SplitCsvLine(Lines[i], Cells);
            if (Cells.Num() < 2) continue;

            const FName Key(*Cells[0].TrimStartAndEnd());
            if (Key.IsNone()) continue;

            TArray<FString>& Row = GTexts.Add(Key);
            Row.SetNum(GLanguages.Num());
            for (int32 c = 0; c < GLanguages.Num(); ++c)
                Row[c] = Cells.IsValidIndex(c + 1) ? Cells[c + 1].TrimStartAndEnd() : FString();
        }

        // ── Índice inverso: se indexan TODOS los idiomas, empezando por el primero (el de
        // referencia, el que se escribe en los WBP) para que gane si dos idiomas comparten texto.
        for (int32 c = 0; c < GLanguages.Num(); ++c)
            for (const TPair<FName, TArray<FString>>& P : GTexts)
                if (P.Value.IsValidIndex(c) && !P.Value[c].IsEmpty())
                    GReverse.FindOrAdd(NormalizeForLookup(P.Value[c]), P.Key);

        FString Codes;
        for (const FPTLanguage& L : GLanguages) Codes += L.Code + TEXT(" ");
        UE_LOG(LogTemp, Log, TEXT("[UITexts] %d textos, %d idiomas (%s) desde %s."),
               GTexts.Num(), GLanguages.Num(), *Codes.TrimEnd(), *Path);
    }

    /** Índice de columna del idioma activo. -1 si no hay idiomas cargados. */
    int32 CurrentLangIndex()
    {
        if (GLanguages.Num() == 0) return INDEX_NONE;

        FString Code = TEXT("es");
        if (const UPTGameUserSettings* S = UPTGameUserSettings::Get()) Code = S->GetLanguageCode().ToLower();

        for (int32 i = 0; i < GLanguages.Num(); ++i)
            if (GLanguages[i].Code == Code) return i;

        return 0; // idioma guardado que ya no está en el CSV → columna de referencia
    }
}

FString PTText::GetStr(FName Key)
{
    EnsureLoaded();

    const TArray<FString>* Row = GTexts.Find(Key);
    if (!Row) return FString::Printf(TEXT("?%s?"), *Key.ToString());

    const int32 Idx = CurrentLangIndex();
    if (Idx != INDEX_NONE && Row->IsValidIndex(Idx) && !(*Row)[Idx].IsEmpty())
        return (*Row)[Idx];

    // Falta la traducción en ese idioma → primera columna con algo (nunca se muestra vacío).
    for (const FString& S : *Row) if (!S.IsEmpty()) return S;
    return FString::Printf(TEXT("?%s?"), *Key.ToString());
}

FText PTText::Get(FName Key)
{
    return FText::FromString(GetStr(Key));
}

FText PTText::Format(FName Key, const FFormatOrderedArguments& Args)
{
    return FText::Format(FTextFormat(Get(Key)), Args);
}

const TArray<FPTLanguage>& PTText::GetAvailableLanguages()
{
    EnsureLoaded();
    return GLanguages;
}

bool PTText::IsLanguageAvailable(const FString& Code)
{
    return GetLanguageIndex(Code) != INDEX_NONE;
}

int32 PTText::GetLanguageIndex(const FString& Code)
{
    EnsureLoaded();
    const FString Wanted = Code.ToLower();
    for (int32 i = 0; i < GLanguages.Num(); ++i)
        if (GLanguages[i].Code == Wanted) return i;
    return INDEX_NONE;
}

int32 PTText::GetCurrentLanguageIndex()
{
    EnsureLoaded();
    const int32 Idx = CurrentLangIndex();
    return Idx == INDEX_NONE ? 0 : Idx;
}

PTText::FPTOnLanguageChanged& PTText::OnLanguageChanged()
{
    static FPTOnLanguageChanged Delegate;
    return Delegate;
}

void PTText::Reload()
{
    GLoaded = false;
    GLanguages.Reset();
    GTexts.Reset();
    GReverse.Reset();
    EnsureLoaded();
}

void PTText::LocalizeWidgetTree(UUserWidget* Widget)
{
    if (!Widget || !Widget->WidgetTree) return;
    EnsureLoaded();
    if (GReverse.Num() == 0) return; // sin tabla no hay nada que traducir

    // Devuelve el texto traducido, o false si no es una frase conocida (→ es texto dinámico).
    auto Translate = [](const FText& In, FText& Out) -> bool
    {
        const FString Cur = In.ToString();
        if (Cur.IsEmpty()) return false;
        const FName* Key = GReverse.Find(NormalizeForLookup(Cur));
        if (!Key) return false;

        Out = PTText::Get(*Key);
        return !Out.EqualTo(In); // ya estaba en el idioma correcto → no tocar
    };

    Widget->WidgetTree->ForEachWidget([&Translate](UWidget* W)
    {
        if (UTextBlock* T = Cast<UTextBlock>(W))
        {
            FText New;
            if (Translate(T->GetText(), New)) T->SetText(New);
        }
        else if (URichTextBlock* R = Cast<URichTextBlock>(W))
        {
            FText New;
            if (Translate(R->GetText(), New)) R->SetText(New);
        }
        else if (UEditableTextBox* E = Cast<UEditableTextBox>(W))
        {
            // En las cajas de texto se traduce el placeholder, nunca lo que escribió el jugador.
            FText New;
            if (Translate(E->GetHintText(), New)) E->SetHintText(New);
        }
        else if (UUserWidget* Nested = Cast<UUserWidget>(W))
        {
            // ForEachWidget no entra en el árbol de los widgets anidados (WBP dentro de WBP):
            // hay que bajar a mano, si no las filas/tarjetas quedarían sin traducir.
            PTText::LocalizeWidgetTree(Nested);
        }
    });
}

// ── Blueprint ────────────────────────────────────────────────────────────────────────────────

void UPTTextLibrary::PTLocalizeWidget(UUserWidget* Widget)
{
    ::PTText::LocalizeWidgetTree(Widget);
}

FText UPTTextLibrary::PTText(FName Key)
{
    return ::PTText::Get(Key);
}

TArray<FPTLanguage> UPTTextLibrary::PTGetLanguages()
{
    return ::PTText::GetAvailableLanguages();
}

FString UPTTextLibrary::PTGetCurrentLanguage()
{
    if (const UPTGameUserSettings* S = UPTGameUserSettings::Get()) return S->GetLanguageCode();
    return TEXT("es");
}

void UPTTextLibrary::PTSetLanguage(const FString& Code)
{
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
    {
        S->SetLanguageCode(Code); // guarda + avisa (la pantalla se re-traduce sola)
        S->SaveSettings();
    }
}
