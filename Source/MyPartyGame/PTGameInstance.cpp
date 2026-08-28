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
#include "Sound/SoundSubmix.h"
#include "Components/AudioComponent.h"
#include "PTGameUserSettings.h"
#include "Multiplayer/MultiplayerSessionsSubsystem.h"
#include "UI/PTInvitePopupWidget.h"
#include "Mods/PTWordPackSubsystem.h"

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

    // Formato SIMPLE de siempre: una palabra por línea (la 1ª columna). Se ignoran columnas extra
    // (categoría/dificultad de CSV viejos). Se sigue aceptando para no romper los CSV ya armados.
    for (int32 i = 0; i < Lines.Num(); ++i)
    {
        const FString Line = Lines[i].TrimStartAndEnd();
        if (Line.IsEmpty() || Line.StartsWith(TEXT("#"))) continue;

        TArray<FString> Cols;
        Line.ParseIntoArray(Cols, TEXT(","), false);
        for (FString& C : Cols) C = C.TrimStartAndEnd();

        const FString Word = Cols.IsValidIndex(0) ? Cols[0] : FString();
        if (Word.IsEmpty()) continue;

        // Saltar una fila de encabezado ("Palabra,..." o "Word,...").
        const FString WLow = Word.ToLower();
        if (i == 0 && (WLow == TEXT("palabra") || WLow == TEXT("word"))) continue;

        // Una sola traducción: va en el idioma de referencia y el resto cae a esa por el fallback.
        TArray<FString> OneLang; OneLang.Add(Word);
        Parsed.Add(FPTWordEntry(OneLang));
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

void UPTGameInstance::SelectWordPack(const FString& PackId)
{
    UPTWordPackSubsystem* WP = GetSubsystem<UPTWordPackSubsystem>();
    if (!WP) return;
    const FPTWordPack* Pack = WP->FindPack(PackId);
    if (!Pack)
    {
        UE_LOG(LogTemp, Warning, TEXT("[GameInstance] SelectWordPack: no se encontró el banco '%s'."), *PackId);
        return;
    }
    const int32 N = LoadCustomWordsFromCSVFile(Pack->CsvPath);
    SelectedWordPackTitle = (N > 0) ? Pack->Title : FString();
    UE_LOG(LogTemp, Log, TEXT("[GameInstance] Banco '%s' seleccionado (%d palabras)."), *Pack->Title, N);
    OnSelectedWordPackChanged.Broadcast();
}

void UPTGameInstance::SelectDefaultWordBank()
{
    ClearCustomWords();
    SelectedWordPackTitle.Reset();
    OnSelectedWordPackChanged.Broadcast();
}

void UPTGameInstance::PublishWordPackFromDialog()
{
    IDesktopPlatform* DP = FDesktopPlatformModule::Get();
    if (!DP) return;

    const void* ParentHandle = FSlateApplication::IsInitialized()
        ? FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr) : nullptr;

    TArray<FString> Files;
    const bool bPicked = DP->OpenFileDialog(
        ParentHandle, TEXT("Elegir CSV para publicar al Workshop"), FPaths::ProjectDir(), TEXT(""),
        TEXT("CSV (*.csv)|*.csv|Texto (*.txt)|*.txt|Todos (*.*)|*.*"),
        EFileDialogFlags::None, Files);

    if (!bPicked || Files.Num() == 0) return;

    const FString Path  = Files[0];
    const FString Title = FPaths::GetBaseFilename(Path); // nombre del archivo como título inicial
    if (UPTWordPackSubsystem* WP = GetSubsystem<UPTWordPackSubsystem>())
        WP->PublishWordPack(Path, Title, FString(), FString());
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

    // Invitaciones (F3): el subsistema ya está inicializado tras Super::Init().
    if (UMultiplayerSessionsSubsystem* S = GetSubsystem<UMultiplayerSessionsSubsystem>())
    {
        // Llega una invitación de un amigo → popup Aceptar/Rechazar (sale en cualquier momento).
        S->OnInviteReceived.AddUObject(this, &UPTGameInstance::HandleInviteReceived);
        // Steam "Unirse a partida" (rich presence connect / lanzado desde invitación) → viajar al host.
        S->OnJoinRequestedById.AddUObject(this, &UPTGameInstance::HandleJoinRequestedById);

        // Punto ÚNICO de viaje tras unirse: funciona desde el menú Y desde una partida (para aceptar
        // el popup en medio del juego). El guard de BeginClientTravel evita doble-conexión aunque el
        // menú también intente viajar (su 2º intento queda ignorado).
        S->OnJoinSessionComplete.AddWeakLambda(this, [this](EOnJoinSessionCompleteResult::Type Result)
        {
            if (Result == EOnJoinSessionCompleteResult::Success) TravelToResolvedSession();
        });
    }
}

void UPTGameInstance::HandleInviteReceived(const FString& FromName)
{
    ShowInvitePopup(FromName);
}

void UPTGameInstance::HandleJoinRequestedById(const FString& HostSteamId)
{
    if (HostSteamId.IsEmpty()) return;
    UMultiplayerSessionsSubsystem* S = GetSubsystem<UMultiplayerSessionsSubsystem>();
    // Unirse directo a la partida del host por SteamSockets (mismo camino que un join normal).
    const FString Name = S ? S->GetLocalPlayerDisplayName() : TEXT("Player");
    const FString TravelURL = FString::Printf(TEXT("steam.%s?Name=%s"),
        *HostSteamId, *UMultiplayerSessionsSubsystem::SanitizeNameForTravelURL(Name));
    UE_LOG(LogTemp, Log, TEXT("[GameInstance] Unirse por Steam → %s"), *TravelURL);
    NotifyJoinedServer(TravelURL); // guard anti-flood + arma reintento
}

void UPTGameInstance::ShowInvitePopup(const FString& FromName)
{
    if (!InvitePopupClass) return; // sin WBP asignado: el usuario igual puede aceptar por el overlay de Steam.

    // Si ya había un popup, cerrarlo (siempre mostramos la última invitación).
    if (ActivePopup) { ActivePopup->RemoveFromParent(); ActivePopup = nullptr; }

    ActivePopup = CreateWidget<UPTInvitePopupWidget>(this, InvitePopupClass);
    if (!ActivePopup) return;
    ActivePopup->AddToViewport(1000); // Z alto: por encima de menús/HUD/otros popups
    ActivePopup->Setup(FromName, InvitePopupSeconds);
}

void UPTGameInstance::TravelToResolvedSession()
{
    UMultiplayerSessionsSubsystem* S = GetSubsystem<UMultiplayerSessionsSubsystem>();
    if (!S) return;

    FString ConnectString;
    if (!S->GetResolvedConnectString(ConnectString)) return;

    // Sin contraseña (se sacó el código). Nombre para que el server lo lea en PostLogin.
    const FString TravelURL = ConnectString
        + TEXT("?Name=") + UMultiplayerSessionsSubsystem::SanitizeNameForTravelURL(S->GetLocalPlayerDisplayName());

    NotifyJoinedServer(TravelURL); // guard anti-flood + arma reintento
}

void UPTGameInstance::OnPostLoadMap(UWorld* LoadedWorld)
{
    ApplyAudioMix();
    UpdateMenuMusic(LoadedWorld);

    // Terminó de cargar un mapa → cualquier intento de conexión que estuviera en curso YA se resolvió
    // (o entramos a la sesión, o rebotamos al menú). Liberamos el guard anti-flood y cortamos el
    // watchdog. Si la conexión tuvo éxito, además damos por buena la reconexión (rearmamos intentos).
    bClientConnectPending = false;
    if (ConnectWatchdogHandle.IsValid())
    {
        if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(ConnectWatchdogHandle);
        ConnectWatchdogHandle.Invalidate();
    }
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

void UPTGameInstance::UpdateMenuMusic(UWorld* World)
{
    if (!World) return;

    // Nombre del mapa cargado (sin el prefijo de PIE tipo "UEDPIE_0_"). El lobby corre sobre el mismo
    // mapa MainMenu, así que con chequear MainMenu cubrimos menú + lobby; Lvl-01 queda afuera.
    const FString MapName = World->GetMapName();
    const bool bWantMusic = MapName.Contains(MenuMusicMapName);

    const bool bPlaying = MenuMusicComp && MenuMusicComp->IsPlaying();

    if (bWantMusic)
    {
        if (!bPlaying && MenuMusic)
        {
            // Persiste entre transiciones (menú→lobby es un ServerTravel sobre el mismo mapa) para que
            // NO se reinicie. bAutoDestroy: al hacer Stop() en Lvl-01 se limpia solo. El loop lo da el asset.
            // CreateSound2D (no auto-play) + FadeIn: arranca en 0 y sube al volumen del settings de forma
            // progresiva, siempre (incluso la primera vez que abrís el juego).
            MenuMusicComp = UGameplayStatics::CreateSound2D(
                World, MenuMusic, 1.f, 1.f, 0.f, nullptr,
                /*bPersistAcrossLevelTransition=*/true, /*bAutoDestroy=*/true);
            if (MenuMusicComp)
            {
                MusicStartWorldTime = World->GetTimeSeconds(); // t0 para la fase por BPM
                MenuMusicComp->FadeIn(FMath::Max(0.f, MenuMusicFadeInSeconds), 1.f, 0.f, EAudioFaderCurve::Linear);
                // Envelope follower del submix de música → el personaje del lobby reacciona a la energía.
                // La música se rutea POR este submix desde el asset (SoundWave → "Submix" = este), así
                // que no hace falta SetSubmixSend (eso duplicaría el audio al master). Solo lo analizamos.
                if (MusicAnalysisSubmix && !bEnvelopeBound)
                {
                    FOnSubmixEnvelopeBP Del; // single-cast (SoundSubmixSend.h)
                    Del.BindDynamic(this, &UPTGameInstance::OnMusicEnvelope);
                    MusicAnalysisSubmix->StartEnvelopeFollowing(World);
                    MusicAnalysisSubmix->AddEnvelopeFollowerDelegate(World, Del);
                    bEnvelopeBound = true;
                }
            }
        }
    }
    else if (MenuMusicComp)
    {
        MenuMusicComp->Stop(); // Lvl-01 (u otro mapa): cortar la música del menú/lobby
        MenuMusicComp = nullptr;
        MusicEnergy = 0.f;     // sin música → el personaje deja de reaccionar
    }
}

float UPTGameInstance::GetMenuMusicBeats() const
{
    if (!MenuMusicComp || !MenuMusicComp->IsPlaying() || MenuMusicBPM <= 0.f) return -1.f;
    const UWorld* W = GetWorld();
    if (!W) return -1.f;
    double Pos = W->GetTimeSeconds() - MusicStartWorldTime; // seg de reproducción
    if (MenuMusicLoopSeconds > 0.f) Pos = FMath::Fmod(Pos, (double)MenuMusicLoopSeconds); // re-alinear cada loop
    return (float)((Pos - MenuMusicBeatOffset) * (MenuMusicBPM / 60.0));
}

void UPTGameInstance::OnMusicEnvelope(const TArray<float>& Envelope)
{
    // Envelope trae la amplitud por canal (0..1). Promediamos, aplicamos ganancia y suavizamos.
    float Sum = 0.f;
    for (float E : Envelope) Sum += E;
    const float Avg = Envelope.Num() > 0 ? Sum / Envelope.Num() : 0.f;
    const float Target = FMath::Clamp(Avg * MusicEnergyGain, 0.f, 1.f);
    // Suavizado exponencial simple (el delegate llega a ritmo fijo del mixer): ataque rápido, caída suave.
    const float Alpha = (Target > MusicEnergy) ? 0.6f : 0.25f;
    MusicEnergy = FMath::Lerp(MusicEnergy, Target, Alpha);
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

    // El intento de conexión actual falló → liberar el guard y cortar el watchdog. Si TryReconnect
    // decide reintentar, arrancará un ClientTravel nuevo (con el guard ya libre) tras el delay.
    bClientConnectPending = false;
    if (ConnectWatchdogHandle.IsValid())
    {
        World->GetTimerManager().ClearTimer(ConnectWatchdogHandle);
        ConnectWatchdogHandle.Invalidate();
    }

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

bool UPTGameInstance::BeginClientTravel(const FString& TravelURL)
{
    if (TravelURL.IsEmpty()) return false;

    // ANTI-FLOOD: si ya hay una conexión en curso, NO abrir otra. Abrir una 2ª conexión al mismo host
    // (mismo SteamID) es justo lo que apilaba conexiones duplicadas y CRASHEABA al host con la
    // assertion "MappedClientConnections.Remove(ConstAddrRef) == 1". Un solo intento a la vez.
    if (bClientConnectPending)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[GameInstance] BeginClientTravel IGNORADO: ya hay una conexión en curso."));
        return false;
    }

    UWorld* World = GetWorld();
    APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
    if (!World || !PC) return false;

    PendingReconnectURL = TravelURL;
    LastGameURL         = TravelURL; // persiste para el botón "Reconectar" de Find Sessions

    bClientConnectPending = true;
    // Watchdog: si en ConnectWatchdogSeconds no aterrizamos en un mapa (OnPostLoadMap lo cancela al
    // conectar OK), abortamos al menú en vez de dejar que el motor siga reintentando el Browse solo
    // (cada reintento abre otra conexión → apila → crashea al host).
    World->GetTimerManager().SetTimer(ConnectWatchdogHandle, this,
        &UPTGameInstance::OnConnectWatchdog, ConnectWatchdogSeconds, false);

    UE_LOG(LogTemp, Log, TEXT("[GameInstance] BeginClientTravel → %s"), *TravelURL);
    PC->ClientTravel(TravelURL, ETravelType::TRAVEL_Absolute);
    return true;
}

void UPTGameInstance::OnConnectWatchdog()
{
    if (!bClientConnectPending) return; // ya se resolvió (conectó o rebotó)

    UE_LOG(LogTemp, Warning,
        TEXT("[GameInstance] Watchdog: la conexión no completó a tiempo → aborto al menú. "
             "Probá reconectar de nuevo en unos segundos (el host ya habrá soltado tu conexión vieja)."));

    // No encadenar reintentos automáticos: el host quizá todavía no soltó la conexión vieja. Al menú;
    // el jugador reintenta a mano (para entonces el host ya limpió → handshake limpio).
    ReconnectAttemptsRemaining = 0;
    PendingConnectError = PTText::GetStr(TEXT("ERR_CONNECT_SESSION"));
    ReturnToMainMenuWithError(FString()); // string vacío = no pisa el PendingConnectError de arriba
}

void UPTGameInstance::DoReconnectAttempt()
{
    // NO resetea ReconnectAttemptsRemaining (ya se descontó en TryReconnect) → un solo intento auto.
    UE_LOG(LogTemp, Log, TEXT("[GameInstance] Reintentando conexión: %s"), *PendingReconnectURL);
    BeginClientTravel(PendingReconnectURL);
}

void UPTGameInstance::NotifyJoinedServer(const FString& TravelURL)
{
    // Join FRESCO (manual / por código): arma el auto-reintento y viaja por el punto único (guard).
    ReconnectAttemptsRemaining = MaxReconnectAttempts;
    BeginClientTravel(TravelURL);
}

void UPTGameInstance::ReconnectToLastGame()
{
    if (LastGameURL.IsEmpty()) return;
    ReconnectAttemptsRemaining = MaxReconnectAttempts; // reintento manual: rearma el auto-reintento
    BeginClientTravel(LastGameURL);
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
    // golpeando una dirección muerta. Tampoco ofrecer reconectar (la partida ya no existe).
    PendingReconnectURL.Reset();
    ReconnectAttemptsRemaining = 0;
    LastGameURL.Reset();
}

void UPTGameInstance::ReturnToMainMenuWithError(const FString& ErrorString)
{
    // Traducir el token del servidor a un mensaje amigable.
    if (ErrorString.Contains(TEXT("WrongPassword")))
    {
        PendingConnectError = PTText::GetStr(TEXT("ERR_WRONG_PASSWORD"));
        LastGameURL.Reset(); // password incorrecta: reconectar con la misma URL fallaría igual
    }
    else if (!ErrorString.IsEmpty())
        PendingConnectError = PTText::GetStr(TEXT("ERR_CONNECT_SESSION"));

    // Se rindió definitivamente: no dejar un intento de reconexión colgado para la próxima sesión.
    PendingReconnectURL.Reset();
    ReconnectAttemptsRemaining = 0;

    // Cortar el guard/watchdog de conexión (el OpenLevel de abajo también dispara OnPostLoadMap que
    // los limpia, pero lo hacemos ya para que el watchdog no llegue a re-disparar).
    bClientConnectPending = false;
    if (ConnectWatchdogHandle.IsValid())
    {
        if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(ConnectWatchdogHandle);
        ConnectWatchdogHandle.Invalidate();
    }

    // Volver al mapa del menú principal.
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

FString UPTGameInstance::ConsumePendingConnectError()
{
    const FString Out = PendingConnectError;
    PendingConnectError.Reset();
    return Out;
}
