// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTGameInstance.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Framework/Application/SlateApplication.h"

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
        Parsed.Add(FPTWordEntry(Word, Category, Diff));
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

void UPTGameInstance::Init()
{
    Super::Init();

    if (GEngine)
    {
        GEngine->OnNetworkFailure().AddUObject(this, &UPTGameInstance::HandleNetworkFailure);
        GEngine->OnTravelFailure().AddUObject(this, &UPTGameInstance::HandleTravelFailure);
    }
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

void UPTGameInstance::ReturnToMainMenuWithError(const FString& ErrorString)
{
    // Traducir el token del servidor a un mensaje amigable.
    if (ErrorString.Contains(TEXT("WrongPassword")))
        PendingConnectError = TEXT("Contraseña incorrecta");
    else if (!ErrorString.IsEmpty())
        PendingConnectError = TEXT("No se pudo conectar a la sesión");

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
