// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameInstance.h"
#include "PTTextTable.h"
#include "PTWordBank.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundMix.h"
#include "Sound/SoundClass.h"
#include "PTGameUserSettings.h"

namespace
{
    // "facil/fácil/1/f" → Facil; "dificil/difícil/3/d" → Dificil; resto → Media.
    EPTWordDifficulty ParseDifficulty(const FString& In)
    {
        const FString S = In.TrimStartAndEnd().ToLower();
        if (S == TEXT("1") || S.StartsWith(TEXT("f"))) return EPTWordDifficulty::Facil;
        if (S == TEXT("3") || S.StartsWith(TEXT("d"))) return EPTWordDifficulty::Dificil;
        return EPTWordDifficulty::Media;
    }
}

int32 UPTGameInstance::LoadCustomWordsFromCSVDialog()
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) return 0;

    const void* ParentHandle = FSlateApplication::IsInitialized()
        ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr) : nullptr;

    TArray<FString> Files;
    const bool bPicked = DP->OpenFileDialog(
        ParentHandle, TEXT("Elegir CSV de palabras"), FPaths::ProjectDir(), TEXT(""),
        TEXT("CSV (*.csv)|*.csv|Texto (*.txt)|*.txt|Todos (*.*)|*.*"),
        EFileDialogFlags::None, Files);

    if (!bPicked || Files.Num() == 0) return 0;
    return LoadCustomWordsFromCSVFile(Files[0]);
}

int32 UPTGameInstance::LoadCustomWordsFromCSVFile(const FString& Path)
{
    FString Content;
    if (!FFileHelper::LoadFileToString(Content, *Path)) // detecta UTF-8/BOM/UTF-16 solo
        return 0;

    TArray<FString> Lines;
    Content.ParseIntoArrayLines(Lines);

    TArray<FPTWordEntry> Parsed;

    // Formato del BANCO DEFAULT (Name,Category,Difficulty,ES,EN,... o con WordEs/WordEn): se intenta
    // SIEMPRE primero el mismo lector que el banco default, que mapea las columnas POR NOMBRE del
    // encabezado (así toma bien todos los idiomas). Si no reconoce columnas de palabra, devuelve 0 y
    // se cae al parser simple de abajo.
    if (Lines.Num() > 1 && PTWordBank::ParseWordCsv(Lines, Parsed) && Parsed.Num() > 0)
    {
        PendingMatchSettings.CustomWords     = MoveTemp(Parsed);
        PendingMatchSettings.bUseCustomWords = true;
        UE_LOG(LogTemp, Log, TEXT("[GameInstance] CSV (formato banco): %d palabras desde %s"),
               PendingMatchSettings.CustomWords.Num(), *Path);
        return PendingMatchSettings.CustomWords.Num();
    }
    Parsed.Reset();

    // Formato SIMPLE de siempre: "Palabra,Categoria,Dificultad" (un solo idioma). Se sigue
    // aceptando para no romper los CSV que la gente ya tenga armados.
    for (int32 i = 0; i < Lines.Num(); ++i)
    {
        const FString Line = Lines[i].TrimStartAndEnd();
        if (Line.IsEmpty() || Line.StartsWith(TEXT("#"))) continue;

        TArray<FString> Cols;
        Line.ParseIntoArray(Cols, TEXT(","), false);
        for (FString& C : Cols) C = C.TrimStartAndEnd();

        const FString Word = Cols.IsValidIndex(0) ? Cols[0] : FString();
        if (Word.IsEmpty()) continue;

        // Saltar una fila de encabezado ("Palabra,Categoria,..." o "Word,...").
        const FString WLow = Word.ToLower();
        if (i == 0 && (WLow == TEXT("palabra") || WLow == TEXT("word"))) continue;

        const FName Category = (Cols.IsValidIndex(1) && !Cols[1].IsEmpty())
            ? FName(*Cols[1]) : FName(TEXT("Personalizadas"));
        const EPTWordDifficulty Diff = ParseDifficulty(Cols.IsValidIndex(2) ? Cols[2] : FString());
        // Una sola traducción: va en el idioma de referencia y el resto cae a esa por el fallback.
        TArray<FString> OneLang; OneLang.Add(Word);
        Parsed.Add(FPTWordEntry(OneLang, Category, Diff));
    }

    if (Parsed.Num() == 0) return 0;

    PendingMatchSettings.CustomWords    = MoveTemp(Parsed);
    PendingMatchSettings.bUseCustomWords = true;
    UE_LOG(LogTemp, Log, TEXT("[GameInstance] CSV de palabras cargado: %d palabras desde %s"),
           PendingMatchSettings.CustomWords.Num(), *Path);
    return PendingMatchSettings.CustomWords.Num();
}

void UPTGameInstance::ClearCustomWords()
{
    PendingMatchSettings.CustomWords.Reset();
    PendingMatchSettings.bUseCustomWords = false;
}

void UPTGameInstance::ApplyUIButtonSounds(UUserWidget* Root) const
{
    if (!Root || (!UIHoverSound && !UIClickSound) || !Root->WidgetTree) return;

    TArray<UWidget*> All;
    Root->WidgetTree->GetAllWidgets(All);
    for (UWidget* W : All)
    {
        if (UButton* Btn = Cast<UButton>(W))
        {
            // Copiar el estilo actual y solo setear los sonidos (Slate los reproduce solo en hover/press).
            FButtonStyle Style = Btn->GetStyle();
            if (UIHoverSound) Style.HoveredSlateSound.SetResourceObject(UIHoverSound);
            if (UIClickSound) Style.PressedSlateSound.SetResourceObject(UIClickSound);
            Btn->SetStyle(Style);
        }
        else if (UUserWidget* Sub = Cast<UUserWidget>(W))
        {
            // Sub-widget (WBP anidado): sus botones viven en SU propio árbol → recursión.
            ApplyUIButtonSounds(Sub);
        }
    }
}

void UPTGameInstance::Init()
{
    Super::Init();

    if (GEngine)
    {
        GEngine->OnNetworkFailure().AddUObject(this, &UPTGameInstance::HandleNetworkFailure);
        GEngine->OnTravelFailure().AddUObject(this, &UPTGameInstance::HandleTravelFailure);
    }

    // Re-aplicar el mix de audio (Música/Efectos) al cargar cada nivel: el SetSoundMixClassOverride se
    // pierde entre mundos, así que lo reponemos en cada mapa.
    FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UPTGameInstance::OnPostLoadMap);
}

void UPTGameInstance::OnPostLoadMap(UWorld* /*LoadedWorld*/)
{
    ApplyAudioMix();
}

float UPTGameInstance::GetMusicVolume() const
{
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    return S ? S->GetMusicVolume() : 1.f;
}

float UPTGameInstance::GetSFXVolume() const
{
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    return S ? S->GetSFXVolume() : 1.f;
}

void UPTGameInstance::SetMusicVolume(float V)
{
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get()) { S->SetMusicVolume(V); S->SaveSettings(); }
    ApplyAudioMix();
}

void UPTGameInstance::SetSFXVolume(float V)
{
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get()) { S->SetSFXVolume(V); S->SaveSettings(); }
    ApplyAudioMix();
}

void UPTGameInstance::ApplyAudioMix()
{
    UWorld* W = GetWorld();
    if (!W || !AudioMix) return; // sin Sound Mix asignado no hay nada que aplicar
    const float MusicV = GetMusicVolume();
    const float SFXV   = GetSFXVolume();
    // Fade 0s = inmediato. bApplyToChildren=true → afecta también las sub-clases de cada Sound Class.
    if (MusicClass) UGameplayStatics::SetSoundMixClassOverride(W, AudioMix, MusicClass, MusicV, 1.f, 0.f, true);
    if (SFXClass)   UGameplayStatics::SetSoundMixClassOverride(W, AudioMix, SFXClass,   SFXV,   1.f, 0.f, true);
    UGameplayStatics::PushSoundMixModifier(W, AudioMix);
}

void UPTGameInstance::HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
                                           ENetworkFailure::Type FailureType,
                                           const FString& ErrorString)
{
    // Con varios clientes en PIE comparten el mismo GEngine: ignorar fallos de otros mundos.
    if (World != GetWorld()) return;

    // FailureReceived = el servidor cerró la conexión con un ErrorMessage explícito
    // (ej. el "WrongPassword" que pone PTLobbyGameMode::PreLogin). Reintentar no arregla
    // una contraseña incorrecta, así que va directo al menú.
    if (FailureType == ENetworkFailure::FailureReceived)
    {
        ReturnToMainMenuWithError(ErrorString);
        return;
    }

    // Estos son caídas reales de la conexión (no eventos de red durante ServerTravel) —
    // acá sí vale la pena reintentar antes de rendirse y volver al menú.
    const bool bDroppedConnection =
        FailureType == ENetworkFailure::PendingConnectionFailure ||
        FailureType == ENetworkFailure::ConnectionTimeout         ||
        FailureType == ENetworkFailure::ConnectionLost;

    if (bDroppedConnection && TryReconnect(World))
    {
        return; // reintento en curso, todavía no volver al menú
    }

    if (bDroppedConnection)
    {
        ReturnToMainMenuWithError(ErrorString);
    }
}

bool UPTGameInstance::TryReconnect(UWorld* World)
{
    if (PendingReconnectURL.IsEmpty() || ReconnectAttemptsRemaining <= 0)
    {
        return false;
    }

    --ReconnectAttemptsRemaining;
    UE_LOG(LogTemp, Warning, TEXT("[GameInstance] Conexión perdida. Reintentando en %.0fs (quedan %d intento(s))..."),
        ReconnectRetryDelaySeconds, ReconnectAttemptsRemaining);

    FTimerHandle Unused;
    World->GetTimerManager().SetTimer(
        Unused, FTimerDelegate::CreateUObject(this, &UPTGameInstance::DoReconnectAttempt),
        ReconnectRetryDelaySeconds, false);

    return true;
}

void UPTGameInstance::DoReconnectAttempt()
{
    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!PC)
    {
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("[GameInstance] Reintentando conexión: %s"), *PendingReconnectURL);
    PC->ClientTravel(PendingReconnectURL, ETravelType::TRAVEL_Absolute);
}

void UPTGameInstance::NotifyJoinedServer(const FString& TravelURL)
{
    PendingReconnectURL      = TravelURL;
    ReconnectAttemptsRemaining = MaxReconnectAttempts;
}

void UPTGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType,
                                          const FString& ErrorString)
{
    // Con varios clientes en PIE comparten el mismo GEngine: ignorar fallos de otros mundos.
    if (World != GetWorld()) return;

    // Solo actuar si hay un error real (string no vacío).
    if (!ErrorString.IsEmpty())
    {
        ReturnToMainMenuWithError(ErrorString);
    }
}

void UPTGameInstance::SetPendingConnectError(const FString& InError)
{
    PendingConnectError = InError;
    // El host cerró a propósito: reintentar la conexión no tiene sentido y dejaría al jugador
    // golpeando una dirección muerta.
    PendingReconnectURL.Reset();
    ReconnectAttemptsRemaining = 0;
}

void UPTGameInstance::ReturnToMainMenuWithError(const FString& ErrorString)
{
    // Traducir el token del servidor a un mensaje amigable.
    if (ErrorString.Contains(TEXT("WrongPassword")))
        PendingConnectError = PTText::GetStr(TEXT("ERR_WRONG_PASSWORD"));
    else if (!ErrorString.IsEmpty())
        PendingConnectError = PTText::GetStr(TEXT("ERR_CONNECT_SESSION"));

    // Se rindió definitivamente: no dejar un intento de reconexión colgado para la próxima sesión.
    PendingReconnectURL.Reset();
    ReconnectAttemptsRemaining = 0;

    // Volver al mapa del menú principal.
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

FString UPTGameInstance::ConsumePendingConnectError()
{
    const FString Out = PendingConnectError;
    PendingConnectError.Reset();
    return Out;
}
