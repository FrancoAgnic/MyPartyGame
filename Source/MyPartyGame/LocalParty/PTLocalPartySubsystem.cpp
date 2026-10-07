#include "PTLocalPartySubsystem.h"
#include "PTLocalPartyServer.h"
#include "PTRelayTransport.h"
#include "Misc/ConfigCacheIni.h"
#include "PTSculptGameMode.h"
#include "PTSculptGameState.h"
#include "../Lobby/PTPlayerState.h"
#include "../PTGameInstance.h"
#include "../PTGameUserSettings.h"
#include "../PTTextTable.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogPTLocalPartySub, Log, All);

namespace
{
    // Paleta de colores de jugador (bien distinguibles entre sí, sobre fondo oscuro).
    const FLinearColor GPartyPalette[] = {
        FLinearColor::FromSRGBColor(FColor(0xFF, 0x6B, 0x6B)), // rojo coral
        FLinearColor::FromSRGBColor(FColor(0x4D, 0xA8, 0xFF)), // azul
        FLinearColor::FromSRGBColor(FColor(0x5B, 0xE0, 0x8A)), // verde
        FLinearColor::FromSRGBColor(FColor(0xFF, 0xC1, 0x4D)), // amarillo
        FLinearColor::FromSRGBColor(FColor(0xC0, 0x7B, 0xFF)), // violeta
        FLinearColor::FromSRGBColor(FColor(0xFF, 0x8F, 0x3D)), // naranja
        FLinearColor::FromSRGBColor(FColor(0x3D, 0xE0, 0xD8)), // turquesa
        FLinearColor::FromSRGBColor(FColor(0xFF, 0x7E, 0xC8)), // rosa
        FLinearColor::FromSRGBColor(FColor(0xA8, 0xE0, 0x4D)), // lima
        FLinearColor::FromSRGBColor(FColor(0xB0, 0x9A, 0x7A)), // arena
        FLinearColor::FromSRGBColor(FColor(0x8A, 0x9B, 0xFF)), // lavanda
        FLinearColor::FromSRGBColor(FColor(0xE0, 0xE0, 0xE0)), // gris claro
    };

    FString ColorHex(const FLinearColor& C)
    {
        return FString::Printf(TEXT("#%s"), *C.ToFColorSRGB().ToHex().Left(6));
    }

    FString PhaseName(EPTTurnPhase P)
    {
        switch (P)
        {
            case EPTTurnPhase::ChoosingWord: return TEXT("choosing");
            case EPTTurnPhase::Drawing:      return TEXT("drawing");
            case EPTTurnPhase::TurnEnd:      return TEXT("turnend");
            case EPTTurnPhase::GameOver:     return TEXT("gameover");
            default:                         return TEXT("lobby");
        }
    }

    FString ToJson(const TSharedRef<FJsonObject>& Obj)
    {
        FString Out;
        const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
            TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
        FJsonSerializer::Serialize(Obj, W);
        return Out;
    }

    // Nombre seguro: sin saltos de línea / control, recortado.
    FString CleanName(const FString& In)
    {
        FString Out;
        for (TCHAR C : In) if (C >= 32 && C != 127) Out.AppendChar(C);
        return Out.TrimStartAndEnd().Left(UPTLocalPartySubsystem::MaxNameLen);
    }
}

void UPTLocalPartySubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UPTLocalPartySubsystem::OnPostLoadMap);
}

void UPTLocalPartySubsystem::Deinitialize()
{
    FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
    StopServer();
    Super::Deinitialize();
}

// ── Servidor ────────────────────────────────────────────────────────────────

bool UPTLocalPartySubsystem::StartServer()
{
    const UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance());
    const bool bWantOnline = GI && GI->bLocalPartyOnline;
    if (Server && bOnlineTransport == bWantOnline) return true; // ya está andando (o conectando al relay)
    StopServer();

    if (bWantOnline)
    {
        // URL del relay: Config/DefaultGame.ini → [LocalParty] RelayUrl=wss://play.tudominio.com
        FString RelayUrl;
        GConfig->GetString(TEXT("LocalParty"), TEXT("RelayUrl"), RelayUrl, GGameIni);
        RelayUrl.TrimStartAndEndInline();
        RelayUrl.TrimQuotesInline();
        // Sin esquema (o cortado a "wss:" por un "//" sin comillas en el .ini) → asumir wss://host.
        if (!RelayUrl.Contains(TEXT("://")))
        {
            RelayUrl.RemoveFromStart(TEXT("wss:"));
            RelayUrl.RemoveFromStart(TEXT("ws:"));
            if (!RelayUrl.IsEmpty()) RelayUrl = TEXT("wss://") + RelayUrl;
        }
        if (RelayUrl.IsEmpty() || RelayUrl == TEXT("wss://"))
        {
            UE_LOG(LogPTLocalPartySub, Error, TEXT("Modo online: falta RelayUrl en [LocalParty] de DefaultGame.ini."));
            return false;
        }
        TUniquePtr<FPTRelayTransport> Relay = MakeUnique<FPTRelayTransport>();
        Relay->OnClientConnected.BindUObject(this, &UPTLocalPartySubsystem::HandleConnected);
        Relay->OnClientDisconnected.BindUObject(this, &UPTLocalPartySubsystem::HandleDisconnected);
        Relay->OnClientMessage.BindUObject(this, &UPTLocalPartySubsystem::HandleMessage);
        Relay->Start(RelayUrl);
        HostKey = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(10).ToLower();
#if !UE_BUILD_SHIPPING
        UE_LOG(LogPTLocalPartySub, Log, TEXT("[DEV] Clave del streamer (link privado ?h=): %s"), *HostKey);
#endif
        Server = MoveTemp(Relay);
        bOnlineTransport = true;
        StartStreamChats();
        return true;
    }

    TUniquePtr<FPTLocalPartyServer> Local = MakeUnique<FPTLocalPartyServer>();
    Local->OnClientConnected.BindUObject(this, &UPTLocalPartySubsystem::HandleConnected);
    Local->OnClientDisconnected.BindUObject(this, &UPTLocalPartySubsystem::HandleDisconnected);
    Local->OnClientMessage.BindUObject(this, &UPTLocalPartySubsystem::HandleMessage);

    // Ruta RELATIVA (como los CSV de textos): en la build empaquetada el archivo vive en el pak (UFS).
    const FString WebRoot = FPaths::ProjectContentDir() / TEXT("LocalParty/Web");
    // Si el puerto está ocupado (otra instancia), probar los siguientes.
    for (int32 Try = 0; Try < 5; ++Try)
    {
        if (Local->Start(Port + Try, WebRoot))
        {
            LocalPort = Port + Try;
            Server = MoveTemp(Local);
            bOnlineTransport = false;
            CachedLanIp = FPTLocalPartyServer::GetLanIp();
            UE_LOG(LogPTLocalPartySub, Log, TEXT("Modo local: los celulares entran en %s"), *GetJoinUrl());
            return true;
        }
    }
    return false;
}

void UPTLocalPartySubsystem::StopServer()
{
    StopStreamChats();
    if (Server) Server->Stop();
    Server.Reset();
    bOnlineTransport = false;
    for (FPTPhonePlayer& P : Players) { P.bOnline = false; P.ConnId = INDEX_NONE; }
}

FString UPTLocalPartySubsystem::GetHostJoinUrl() const
{
    const FString Url = bOnlineTransport ? GetJoinUrl() : FString();
    return Url.IsEmpty() ? FString() : Url + TEXT("?h=") + HostKey;
}

bool UPTLocalPartySubsystem::IsHostConnected() const
{
    for (const FPTPhonePlayer& P : Players) if (P.bIsHost && P.bOnline) return true;
    return false;
}

int32 UPTLocalPartySubsystem::GetGuesserCount() const
{
    int32 N = 0;
    for (const FPTPhonePlayer& P : Players) if (!P.bIsHost) ++N;
    return N;
}

int32 UPTLocalPartySubsystem::GetPhoneGuesserCount() const
{
    int32 N = 0;
    for (const FPTPhonePlayer& P : Players) if (!P.bIsHost && P.ChatPlatform == 0) ++N;
    return N;
}

int32 UPTLocalPartySubsystem::GetChatPlayerCount() const
{
    int32 N = 0;
    for (const FPTPhonePlayer& P : Players) if (P.ChatPlatform != 0) ++N;
    return N;
}

FString UPTLocalPartySubsystem::GetRoomCode() const
{
    if (!bOnlineTransport || !Server) return FString();
    return static_cast<const FPTRelayTransport*>(Server.Get())->GetRoomCode();
}

FString UPTLocalPartySubsystem::GetPublicHost() const
{
    if (!bOnlineTransport || !Server) return FString();
    FString U = static_cast<const FPTRelayTransport*>(Server.Get())->GetPublicBaseUrl();
    U.RemoveFromStart(TEXT("https://"));
    U.RemoveFromStart(TEXT("http://"));
    return U;
}

FString UPTLocalPartySubsystem::GetOnlineError() const
{
    if (!bOnlineTransport || !Server) return FString();
    return static_cast<const FPTRelayTransport*>(Server.Get())->GetLastError();
}

bool UPTLocalPartySubsystem::IsServerRunning() const
{
    return Server && Server->IsRunning();
}

FString UPTLocalPartySubsystem::GetJoinUrl() const
{
    if (!IsServerRunning()) return FString();
    if (bOnlineTransport)
    {
        const FPTRelayTransport* R = static_cast<const FPTRelayTransport*>(Server.Get());
        return R->GetRoomCode().IsEmpty() ? FString() : R->GetPublicBaseUrl() / R->GetRoomCode();
    }
    if (CachedLanIp.IsEmpty()) return FString();
    return LocalPort == 80
        ? FString::Printf(TEXT("http://%s"), *CachedLanIp)
        : FString::Printf(TEXT("http://%s:%d"), *CachedLanIp, LocalPort);
}

void UPTLocalPartySubsystem::OnPostLoadMap(UWorld* World)
{
    // Salir del modo local por cualquier vía (menú de pausa → "Salir", error de red, etc.): si el mapa
    // nuevo NO es una partida de Sculpturillo, apagar el servidor y bajar el flag. Si no, el próximo
    // "Hostear" online arrancaría en modo local.
    if (!World || World->IsNetMode(NM_Client)) return;
    if (World->GetAuthGameMode<APTSculptGameMode>()) return;

    if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
    {
        if (GI->bLocalPartyMode)
            UE_LOG(LogPTLocalPartySub, Log, TEXT("Se salió del modo local (mapa %s)."), *World->GetMapName());
        GI->bLocalPartyMode = false;
        GI->bLocalPartyOnline = false;
    }
    StopServer();
    ClearPlayers();
}

// ── Partida ─────────────────────────────────────────────────────────────────

void UPTLocalPartySubsystem::BindGameMode(APTSculptGameMode* GM)
{
    GameMode = GM;
    if (!GM) return;
    StartServer();

    if (APTSculptGameState* G = GM->GetGameState<APTSculptGameState>())
    {
        if (BoundGameState.Get() != G)
        {
            BoundGameState = G;
            G->OnChatLine.AddUniqueDynamic(this, &UPTLocalPartySubsystem::OnChatLine);
        }
    }

    // Los que ya estaban (venían conectados desde antes del OpenLevel o de una partida anterior)
    // necesitan su PlayerState en este mundo nuevo.
    for (FPTPhonePlayer& P : Players) EnsurePlayerState(P);
    EnsureVip();
    OnPlayersChanged.Broadcast();
    PushStates(true);
}

void UPTLocalPartySubsystem::UnbindGameMode(APTSculptGameMode* GM)
{
    if (GameMode.Get() == GM) GameMode.Reset();
    for (FPTPhonePlayer& P : Players) P.PlayerState.Reset();
    BoundGameState.Reset();
}

FString UPTLocalPartySubsystem::GetVipName() const
{
    for (const FPTPhonePlayer& P : Players) if (P.bVip) return P.Name;
    return FString();
}

const FPTPhonePlayer* UPTLocalPartySubsystem::FindByPlayerState(const APlayerState* PS) const
{
    if (!PS) return nullptr;
    for (const FPTPhonePlayer& P : Players) if (P.PlayerState.Get() == PS) return &P;
    return nullptr;
}

FPTPhonePlayer* UPTLocalPartySubsystem::FindByConn(int32 ConnId)
{
    for (FPTPhonePlayer& P : Players) if (P.ConnId == ConnId) return &P;
    return nullptr;
}

FPTPhonePlayer* UPTLocalPartySubsystem::FindById(int32 Id)
{
    for (FPTPhonePlayer& P : Players) if (P.Id == Id) return &P;
    return nullptr;
}

void UPTLocalPartySubsystem::EnsurePlayerState(FPTPhonePlayer& P)
{
    if (P.PlayerState.IsValid() || P.bIsHost) return; // el streamer no es jugador (esculpe la PC)
    if (APTSculptGameMode* GM = GameMode.Get())
        P.PlayerState = GM->LocalParty_AddPlayer(P.Name, P.Language, P.Color);
}

void UPTLocalPartySubsystem::RemovePlayer(int32 Id)
{
    const int32 Idx = Players.IndexOfByPredicate([Id](const FPTPhonePlayer& P) { return P.Id == Id; });
    if (Idx == INDEX_NONE) return;

    FPTPhonePlayer P = Players[Idx];
    Players.RemoveAt(Idx);
    if (P.ConnId != INDEX_NONE && Server) Server->Disconnect(P.ConnId);
    if (APTSculptGameMode* GM = GameMode.Get())
        if (APTPlayerState* PS = P.PlayerState.Get())
            GM->LocalParty_RemovePlayer(PS);

    UE_LOG(LogPTLocalPartySub, Log, TEXT("Jugador '%s' salió de la partida local."), *P.Name);
    EnsureVip();
    OnPlayersChanged.Broadcast();
    PushStates(true);
}

void UPTLocalPartySubsystem::EnsureVip()
{
    // Audiencia: el único anfitrión es el streamer.
    if (bOnlineTransport)
    {
        for (FPTPhonePlayer& P : Players) P.bVip = P.bIsHost;
        return;
    }
    // Local: el VIP (quien empieza la partida) es el primero que entró y sigue conectado.
    bool bHasOnlineVip = false;
    for (const FPTPhonePlayer& P : Players) if (P.bVip && P.bOnline) bHasOnlineVip = true;
    if (bHasOnlineVip) return;
    for (FPTPhonePlayer& P : Players) P.bVip = false;
    for (FPTPhonePlayer& P : Players) if (P.bOnline) { P.bVip = true; return; }
    if (Players.Num() > 0) Players[0].bVip = true; // todos offline: que quede alguien
}

void UPTLocalPartySubsystem::ClearPlayers()
{
    if (Server) for (const FPTPhonePlayer& P : Players) if (P.ConnId != INDEX_NONE) Server->Disconnect(P.ConnId);
    Players.Reset();
    OnPlayersChanged.Broadcast();
}

// ── Mensajes del celular ────────────────────────────────────────────────────

void UPTLocalPartySubsystem::HandleConnected(int32 ConnId)
{
    // Nada todavía: el celular manda "join" (con su token si ya había entrado).
}

void UPTLocalPartySubsystem::HandleDisconnected(int32 ConnId)
{
    if (FPTPhonePlayer* P = FindByConn(ConnId))
    {
        P->ConnId = INDEX_NONE;
        P->bOnline = false;
        P->OfflineSince = FPlatformTime::Seconds();
        P->LastSentState.Reset();
        UE_LOG(LogPTLocalPartySub, Log, TEXT("Celular de '%s' desconectado (espera para reconectar)."), *P->Name);
        EnsureVip();
        OnPlayersChanged.Broadcast();
        PushStates(true);
    }
}

void UPTLocalPartySubsystem::HandleMessage(int32 ConnId, const FString& Text)
{
    TSharedPtr<FJsonObject> Msg;
    const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(R, Msg) || !Msg.IsValid()) return;

    const FString Type = Msg->GetStringField(TEXT("t"));
    if (Type == TEXT("ping")) return;
    if (Type == TEXT("join")) { HandleJoin(ConnId, Msg); return; }

    FPTPhonePlayer* P = FindByConn(ConnId);
    if (!P) return; // todavía no entró
    APTSculptGameMode* GM = GameMode.Get();
    APTPlayerState* PS = P->PlayerState.Get();

    if (Type == TEXT("guess"))
    {
        if (GM && PS && !P->bIsHost)
            GM->LocalParty_Guess(PS, Msg->GetStringField(TEXT("text")).Left(MaxGuessLen));
    }
    else if (Type == TEXT("choose"))
    {
        const int32 I = (int32)Msg->GetNumberField(TEXT("i"));
        if (GM && P->bIsHost)  GM->LocalParty_ChooseAsHost(I);
        else if (GM && PS)     GM->LocalParty_Choose(PS, I);
    }
    else if (Type == TEXT("lang"))
    {
        // Cambió el idioma desde el celular: las palabras le llegan en ese idioma desde ahora.
        FString Lang = Msg->GetStringField(TEXT("lang")).ToLower().Left(2);
        if (PTText::GetLanguageIndex(Lang) != INDEX_NONE)
        {
            P->Language = Lang;
            if (PS) PS->Language = Lang;
            if (P->bIsHost && GM)
                if (APTPlayerState* TV = GM->GetTVPlayerState()) TV->Language = Lang;
        }
    }
    else if (Type == TEXT("start"))
    {
        if (GM && P->bVip) GM->LocalParty_RequestStart();
    }
    else if (Type == TEXT("again"))
    {
        if (GM && P->bVip) GM->LocalParty_PlayAgain();
    }
    else if (Type == TEXT("menu"))
    {
        if (GM && P->bVip) GM->LocalParty_ExitToMenu();
    }
    PushStates(false);
}

void UPTLocalPartySubsystem::HandleJoin(int32 ConnId, const TSharedPtr<FJsonObject>& Msg)
{
    const FString Token = Msg->GetStringField(TEXT("token"));
    const FString Name  = CleanName(Msg->GetStringField(TEXT("name")));
    FString Lang = Msg->GetStringField(TEXT("lang")).ToLower().Left(2);
    if (PTText::GetLanguageIndex(Lang) == INDEX_NONE) Lang = TEXT("es");

    auto Reject = [this, ConnId](const TCHAR* Code)
    {
        TSharedRef<FJsonObject> E = MakeShared<FJsonObject>();
        E->SetStringField(TEXT("t"), TEXT("error"));
        E->SetStringField(TEXT("code"), Code);
        SendToConn(ConnId, E);
    };

    // Streamer: entra con el link privado (?h=CLAVE). Hay uno solo; si vuelve a entrar, retoma.
    const FString HostTry = Msg->GetStringField(TEXT("host"));
    if (bOnlineTransport && !HostTry.IsEmpty() && HostTry == HostKey)
    {
        FPTPhonePlayer* H = Players.FindByPredicate([](const FPTPhonePlayer& X) { return X.bIsHost; });
        if (!H)
        {
            FPTPhonePlayer N;
            N.Id = NextPlayerId++;
            N.Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
            N.bIsHost = true;
            N.Color = FLinearColor(1.f, 0.72f, 0.3f, 1.f);
            Players.Add(N);
            H = &Players.Last();
        }
        else if (H->ConnId != INDEX_NONE && H->ConnId != ConnId && Server)
        {
            Server->Disconnect(H->ConnId);
        }
        H->Name = Name.IsEmpty() ? TEXT("Streamer") : Name;
        H->Language = Lang;
        H->ConnId = ConnId;
        H->bOnline = true;
        H->OfflineSince = 0.0;
        H->LastSentState.Reset();
        if (APTSculptGameMode* GM = GameMode.Get())
            if (APTPlayerState* TV = GM->GetTVPlayerState()) TV->Language = Lang;
        EnsureVip();
        UE_LOG(LogPTLocalPartySub, Log, TEXT("Se conectó el celular del streamer (conn %d)."), ConnId);

        TSharedRef<FJsonObject> W = MakeShared<FJsonObject>();
        W->SetStringField(TEXT("t"), TEXT("welcome"));
        W->SetNumberField(TEXT("id"), H->Id);
        W->SetStringField(TEXT("token"), H->Token);
        W->SetBoolField(TEXT("host"), true);
        SendTo(*H, W);
        OnPlayersChanged.Broadcast();
        PushStates(true);
        return;
    }

    if (Name.IsEmpty()) { Reject(TEXT("name")); return; }

    // ¿Ya era jugador? (mismo celular → mismo token). Si la conexión vieja sigue viva, se reemplaza.
    FPTPhonePlayer* P = nullptr;
    if (!Token.IsEmpty())
        for (FPTPhonePlayer& X : Players) if (X.Token == Token) { P = &X; break; }

    if (!P)
    {
        // Nombre repetido: si el dueño está offline se lo toma (otro navegador del mismo jugador);
        // si está conectado (o es alguien del chat del stream), se rechaza.
        for (FPTPhonePlayer& X : Players)
        {
            if (!X.Name.Equals(Name, ESearchCase::IgnoreCase)) continue;
            if (X.ChatPlatform != 0) { Reject(TEXT("name")); return; }
            if (X.bOnline && X.ConnId != ConnId) { Reject(TEXT("name")); return; }
            P = &X;
            break;
        }
    }

    if (!P)
    {
        if (GetPhoneGuesserCount() >= (bOnlineTransport ? MaxPlayersOnline : MaxPlayers)) { Reject(TEXT("full")); return; }
        FPTPhonePlayer N;
        N.Id = NextPlayerId++;
        N.Name = Name;
        N.Token = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        // Color: el primero de la paleta que nadie use.
        for (const FLinearColor& C : GPartyPalette)
        {
            const bool bUsed = Players.ContainsByPredicate([&C](const FPTPhonePlayer& X) { return X.Color.Equals(C); });
            if (!bUsed) { N.Color = C; break; }
        }
        Players.Add(N);
        P = &Players.Last();
        UE_LOG(LogPTLocalPartySub, Log, TEXT("Entró '%s' desde el celular (conn %d)."), *Name, ConnId);
    }
    else
    {
        if (P->ConnId != INDEX_NONE && P->ConnId != ConnId && Server) Server->Disconnect(P->ConnId);
        UE_LOG(LogPTLocalPartySub, Log, TEXT("Reconectó '%s' (conn %d)."), *P->Name, ConnId);
    }

    P->ConnId = ConnId;
    P->bOnline = true;
    P->Language = Lang;
    if (P->Name != Name && !P->PlayerState.IsValid()) P->Name = Name;
    P->LastSentState.Reset();

    EnsurePlayerState(*P);
    if (APTPlayerState* PS = P->PlayerState.Get()) PS->Language = Lang;
    EnsureVip();

    TSharedRef<FJsonObject> W = MakeShared<FJsonObject>();
    W->SetStringField(TEXT("t"), TEXT("welcome"));
    W->SetNumberField(TEXT("id"), P->Id);
    W->SetStringField(TEXT("token"), P->Token);
    SendTo(*P, W);

    OnPlayersChanged.Broadcast();
    PushStates(true);
}

// ── Estado → celulares ──────────────────────────────────────────────────────

void UPTLocalPartySubsystem::Tick(float DeltaTime)
{
    if (!Server) return;
    Server->Tick();
    if (TwitchChat.IsValid()) TwitchChat->Tick();
    if (KickChat.IsValid())   KickChat->Tick();

    // Celulares que no volvieron: sacarlos de la partida.
    const double Now = FPlatformTime::Seconds();
    const APTSculptGameState* G = BoundGameState.Get();
    const bool bInLobby = !G || G->TurnPhase == EPTTurnPhase::WaitingForPlayers;
    const float Grace = bInLobby ? OfflineGraceInLobby : OfflineGraceInGame;
    TArray<int32> ToRemove;
    for (const FPTPhonePlayer& P : Players)
        if (!P.bOnline && P.OfflineSince > 0.0 && Now - P.OfflineSince > Grace) ToRemove.Add(P.Id);
    for (int32 Id : ToRemove) RemovePlayer(Id);

    StateAccum += DeltaTime;
    if (StateAccum >= 0.2f)
    {
        StateAccum = 0.f;
        PushStates(false);
    }
}

void UPTLocalPartySubsystem::PushStates(bool bForce)
{
    if (!Server) return;
    for (FPTPhonePlayer& P : Players)
    {
        if (!P.bOnline || P.ConnId == INDEX_NONE) continue;
        const FString S = BuildStateFor(P);
        if (!bForce && S == P.LastSentState) continue;
        P.LastSentState = S;
        Server->Send(P.ConnId, S);
    }
}

FString UPTLocalPartySubsystem::BuildStateFor(const FPTPhonePlayer& Me) const
{
    const APTSculptGameState* G = BoundGameState.Get();
    const APTSculptGameMode* GM = GameMode.Get();
    // El celular del streamer "es" la PC (su PlayerState) para opciones / palabra / máscara.
    const APTPlayerState* MyPS = Me.bIsHost ? (GM ? GM->GetTVPlayerState() : nullptr) : Me.PlayerState.Get();
    const EPTTurnPhase Phase = G ? G->TurnPhase : EPTTurnPhase::WaitingForPlayers;

    TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("t"), TEXT("state"));
    O->SetStringField(TEXT("phase"), PhaseName(Phase));
    O->SetBoolField(TEXT("audience"), bOnlineTransport);
    O->SetNumberField(TEXT("round"), G ? G->CurrentRound : 0);
    O->SetNumberField(TEXT("rounds"), G ? G->TotalRounds : 0);
    O->SetNumberField(TEXT("minPlayers"), GM ? GM->LocalParty_GetMinPlayers() : 2);
    O->SetBoolField(TEXT("hostOnline"), IsHostConnected());

    float Secs = 0.f;
    if (G && Phase == EPTTurnPhase::ChoosingWord) Secs = G->GetPhaseSecondsRemaining();
    if (G && Phase == EPTTurnPhase::Drawing)      Secs = G->GetTurnSecondsRemaining();
    O->SetNumberField(TEXT("secs"), FMath::CeilToInt(Secs));

    // Escultor del turno (en audiencia es la PC → se muestra como el streamer).
    const APTPlayerState* Sculptor = G ? G->CurrentSculptor : nullptr;
    const FPTPhonePlayer* SculptorRec = FindByPlayerState(Sculptor);
    if (!SculptorRec && Sculptor && Sculptor->bIsLocalPartyTV)
        SculptorRec = Players.FindByPredicate([](const FPTPhonePlayer& X) { return X.bIsHost; });
    if (Sculptor)
    {
        TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
        S->SetNumberField(TEXT("id"), SculptorRec ? SculptorRec->Id : -1);
        S->SetStringField(TEXT("name"), SculptorRec ? SculptorRec->Name : Sculptor->GetPlayerName());
        O->SetObjectField(TEXT("sculptor"), S);
    }

    // Máscara en el idioma de ESTE celular (al final del turno trae la palabra completa).
    if (G)
    {
        int32 L = PTText::GetLanguageIndex(Me.Language);
        if (L == INDEX_NONE) L = 0;
        const FString Mask = G->MaskedWords.IsValidIndex(L) ? G->MaskedWords[L]
                           : (G->MaskedWords.Num() > 0 ? G->MaskedWords[0] : FString());
        O->SetStringField(TEXT("mask"), Mask);
    }

    const bool bIsSculptor = MyPS && Sculptor == MyPS;
    // Solo el escultor recibe las opciones y la palabra (anti-spoiler: nunca viajan a los demás).
    if (bIsSculptor && GM)
    {
        if (Phase == EPTTurnPhase::ChoosingWord)
        {
            TArray<TSharedPtr<FJsonValue>> Arr;
            for (const FString& W : GM->LocalParty_GetChoicesFor(MyPS)) Arr.Add(MakeShared<FJsonValueString>(W));
            O->SetArrayField(TEXT("choices"), Arr);
        }
        else if (Phase == EPTTurnPhase::Drawing)
        {
            O->SetStringField(TEXT("word"), GM->LocalParty_GetSecretWordFor(MyPS));
        }
    }

    TSharedRef<FJsonObject> You = MakeShared<FJsonObject>();
    You->SetBoolField(TEXT("vip"), Me.bVip);
    You->SetBoolField(TEXT("host"), Me.bIsHost);
    You->SetBoolField(TEXT("sculptor"), bIsSculptor);
    You->SetBoolField(TEXT("guessed"), MyPS && MyPS->bHasGuessedThisTurn);
    You->SetNumberField(TEXT("score"), (MyPS && !Me.bIsHost) ? MyPS->GameScore : 0);
    O->SetObjectField(TEXT("you"), You);

    // Jugadores (sin el streamer). Audiencia: solo el top + vos (si no, con 100 sería enorme).
    TArray<const FPTPhonePlayer*> List;
    for (const FPTPhonePlayer& P : Players) if (!P.bIsHost) List.Add(&P);
    O->SetNumberField(TEXT("count"), List.Num());
    if (bOnlineTransport)
    {
        List.Sort([](const FPTPhonePlayer& A, const FPTPhonePlayer& B)
        {
            const int32 SA = A.PlayerState.IsValid() ? A.PlayerState->GameScore : 0;
            const int32 SB = B.PlayerState.IsValid() ? B.PlayerState->GameScore : 0;
            return SA > SB;
        });
        const bool bMeInTop = List.IndexOfByKey(&Me) < AudienceTopN;
        TArray<const FPTPhonePlayer*> Top;
        for (int32 i = 0; i < List.Num() && Top.Num() < AudienceTopN; ++i) Top.Add(List[i]);
        if (!Me.bIsHost && !bMeInTop) Top.Add(&Me);
        List = MoveTemp(Top);
    }

    TArray<TSharedPtr<FJsonValue>> PArr;
    for (const FPTPhonePlayer* P : List)
    {
        const APTPlayerState* PS = P->PlayerState.Get();
        TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
        J->SetNumberField(TEXT("id"), P->Id);
        J->SetStringField(TEXT("name"), P->Name);
        J->SetNumberField(TEXT("score"), PS ? PS->GameScore : 0);
        J->SetBoolField(TEXT("guessed"), PS && PS->bHasGuessedThisTurn && Phase == EPTTurnPhase::Drawing);
        J->SetBoolField(TEXT("online"), P->bOnline);
        J->SetStringField(TEXT("color"), ColorHex(P->Color));
        PArr.Add(MakeShared<FJsonValueObject>(J));
    }
    O->SetArrayField(TEXT("players"), PArr);

    return ToJson(O);
}

void UPTLocalPartySubsystem::SendTo(const FPTPhonePlayer& P, const TSharedRef<FJsonObject>& Obj)
{
    if (P.ConnId != INDEX_NONE) SendToConn(P.ConnId, Obj);
}

void UPTLocalPartySubsystem::SendToConn(int32 ConnId, const TSharedRef<FJsonObject>& Obj)
{
    if (Server) Server->Send(ConnId, ToJson(Obj));
}

void UPTLocalPartySubsystem::BroadcastJson(const TSharedRef<FJsonObject>& Obj)
{
    const FString S = ToJson(Obj);
    if (Server) for (const FPTPhonePlayer& P : Players) if (P.ConnId != INDEX_NONE) Server->Send(P.ConnId, S);
}

void UPTLocalPartySubsystem::OnChatLine(const FString& Name, const FString& Message, EPTChatType Type)
{
    if (Type == EPTChatType::Close) return; // privado del que escribió (lo manda NotifyCloseGuess)
    if (bOnlineTransport && Type == EPTChatType::Normal) return; // audiencia: el chat se ve en la TV / stream
    TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("t"), TEXT("chat"));
    O->SetStringField(TEXT("name"), Name);
    // En los aciertos Message es el código de idioma (no la palabra): no hace falta en el celular.
    O->SetStringField(TEXT("msg"), Type == EPTChatType::Correct ? FString() : Message);
    O->SetStringField(TEXT("kind"), Type == EPTChatType::Correct ? TEXT("correct")
                                  : Type == EPTChatType::System  ? TEXT("system") : TEXT("normal"));
    BroadcastJson(O);
}

void UPTLocalPartySubsystem::NotifyGuessed(const APTPlayerState* PS, const FString& Word, int32 Points)
{
    if (const FPTPhonePlayer* P = FindByPlayerState(PS))
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("t"), TEXT("guessed"));
        O->SetStringField(TEXT("word"), Word);
        O->SetNumberField(TEXT("pts"), Points);
        SendTo(*P, O);
    }
    PushStates(false);
}

void UPTLocalPartySubsystem::NotifyCloseGuess(const APTPlayerState* PS)
{
    if (const FPTPhonePlayer* P = FindByPlayerState(PS))
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("t"), TEXT("close"));
        SendTo(*P, O);
    }
}

void UPTLocalPartySubsystem::NotifyTurnStarted(const APTPlayerState* Sculptor)
{
    const FPTPhonePlayer* P = FindByPlayerState(Sculptor);
    if (!P && Sculptor && Sculptor->bIsLocalPartyTV)
        P = Players.FindByPredicate([](const FPTPhonePlayer& X) { return X.bIsHost; });
    if (P)
    {
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("t"), TEXT("buzz"));
        SendTo(*P, O);
    }
    PushStates(true);
}

// ── Chat del stream (Twitch / Kick) ─────────────────────────────────────────

namespace
{
    // Comandos para entrar / salir desde el chat. El idioma del comando es el idioma del jugador.
    struct FPTChatCmd { const TCHAR* Cmd; const TCHAR* Lang; };
    const FPTChatCmd GChatJoinCmds[] = {
        { TEXT("!unirse"), TEXT("es") }, { TEXT("!unirme"), TEXT("es") }, { TEXT("!jugar"), TEXT("es") },
        { TEXT("!join"), TEXT("en") },   { TEXT("!play"), TEXT("en") },
        { TEXT("!entrar"), TEXT("pt") }, { TEXT("!jogar"), TEXT("pt") },
        { TEXT("!beitreten"), TEXT("de") }, { TEXT("!mitspielen"), TEXT("de") },
        { TEXT("!rejoindre"), TEXT("fr") }, { TEXT("!jouer"), TEXT("fr") },
        { TEXT("!unisciti"), TEXT("it") }, { TEXT("!gioca"), TEXT("it") },
    };
    const TCHAR* GChatLeaveCmds[] = { TEXT("!salir"), TEXT("!leave"), TEXT("!sair"), TEXT("!verlassen"), TEXT("!quitter"), TEXT("!esci") };
}

void UPTLocalPartySubsystem::StartStreamChats()
{
    if (!bOnlineTransport) return;
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    auto Ensure = [this](TSharedPtr<FPTStreamChat>& Chat, EPTChatPlatform Platform, const FString& Channel)
    {
        if (!Chat.IsValid())
        {
            Chat = MakeShared<FPTStreamChat>(Platform);
            Chat->OnMessage.BindUObject(this, &UPTLocalPartySubsystem::HandleStreamChat);
        }
        Chat->Start(Channel);
    };
    Ensure(TwitchChat, EPTChatPlatform::Twitch, S ? S->GetTwitchChannel() : FString());
    Ensure(KickChat,   EPTChatPlatform::Kick,   S ? S->GetKickChannel()   : FString());
}

void UPTLocalPartySubsystem::StopStreamChats()
{
    if (TwitchChat.IsValid()) TwitchChat->Stop();
    if (KickChat.IsValid())   KickChat->Stop();
}

void UPTLocalPartySubsystem::SetStreamChannel(EPTChatPlatform Platform, const FString& Channel)
{
    const FString Norm = FPTStreamChat::NormalizeChannel(Platform, Channel);
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
    {
        if (Platform == EPTChatPlatform::Twitch) S->SetTwitchChannel(Norm); else S->SetKickChannel(Norm);
        S->SaveSettings();
    }
    StartStreamChats();
}

FString UPTLocalPartySubsystem::GetStreamChannel(EPTChatPlatform Platform) const
{
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    if (!S) return FString();
    return Platform == EPTChatPlatform::Twitch ? S->GetTwitchChannel() : S->GetKickChannel();
}

FPTStreamChat::EStatus UPTLocalPartySubsystem::GetStreamChatStatus(EPTChatPlatform Platform) const
{
    const TSharedPtr<FPTStreamChat>& C = Platform == EPTChatPlatform::Twitch ? TwitchChat : KickChat;
    return C.IsValid() ? C->GetStatus() : FPTStreamChat::EStatus::Off;
}

FString UPTLocalPartySubsystem::GetStreamChatError(EPTChatPlatform Platform) const
{
    const TSharedPtr<FPTStreamChat>& C = Platform == EPTChatPlatform::Twitch ? TwitchChat : KickChat;
    return C.IsValid() ? C->GetLastError() : FString();
}

bool UPTLocalPartySubsystem::HasStreamChatGivenUp(EPTChatPlatform Platform) const
{
    const TSharedPtr<FPTStreamChat>& C = Platform == EPTChatPlatform::Twitch ? TwitchChat : KickChat;
    return C.IsValid() && C->HasGivenUp();
}

bool UPTLocalPartySubsystem::IsStreamChatConnected() const
{
    return GetStreamChatStatus(EPTChatPlatform::Twitch) == FPTStreamChat::EStatus::Connected
        || GetStreamChatStatus(EPTChatPlatform::Kick) == FPTStreamChat::EStatus::Connected;
}

void UPTLocalPartySubsystem::HandleStreamChat(const FPTStreamChatMessage& M)
{
    if (!bOnlineTransport) return;
    const FString Text = M.Text.TrimStartAndEnd();
    if (Text.IsEmpty()) return;
    const FString Key = FString::Printf(TEXT("%d:%s"), (int32)M.Platform + 1, *M.UserId);
    FPTPhonePlayer* P = Players.FindByPredicate([&Key](const FPTPhonePlayer& X) { return X.ChatUserId == Key; });

    if (Text.StartsWith(TEXT("!")))
    {
        FString Cmd = Text, Rest;
        Text.Split(TEXT(" "), &Cmd, &Rest);
        Cmd = Cmd.ToLower();
        for (const FPTChatCmd& J : GChatJoinCmds)
            if (Cmd == J.Cmd) { if (!P) JoinFromChat(M, Key, J.Lang); return; }
        for (const TCHAR* L : GChatLeaveCmds)
            if (Cmd == L) { if (P) RemovePlayer(P->Id); return; }
        return; // comandos de otros bots del canal: no son intentos
    }
    if (!P) return; // no se unió: es chat normal del stream

    const double Now = FPlatformTime::Seconds();
    if (Now - P->LastChatAt < ChatGuessCooldown) return;
    P->LastChatAt = Now;
    APTSculptGameMode* GM = GameMode.Get();
    APTPlayerState* PS = P->PlayerState.Get();
    if (GM && PS) GM->LocalParty_ChatGuess(PS, Text.Left(MaxGuessLen));
}

void UPTLocalPartySubsystem::JoinFromChat(const FPTStreamChatMessage& M, const FString& Key, const FString& Lang)
{
    if (GetChatPlayerCount() >= MaxChatPlayers) return;

    // Nombre del chat (recortado). Si choca con otro jugador, se le agrega un número.
    FString Base = CleanName(M.UserName);
    if (Base.IsEmpty()) Base = TEXT("Viewer");
    FString Name = Base;
    for (int32 Num = 2; Players.ContainsByPredicate([&Name](const FPTPhonePlayer& X) { return X.Name.Equals(Name, ESearchCase::IgnoreCase); }); ++Num)
    {
        const FString Suffix = FString::FromInt(Num);
        Name = Base.Left(MaxNameLen - Suffix.Len()) + Suffix;
    }

    FPTPhonePlayer N;
    N.Id = NextPlayerId++;
    N.Name = Name;
    N.Language = Lang;
    N.ChatPlatform = (uint8)M.Platform + 1;
    N.ChatUserId = Key;
    N.bOnline = true; // sin celular: nunca "se desconecta" (sale con !salir o al cerrar la sala)
    // Su color del chat (si es legible sobre fondo oscuro); si no, uno de la paleta.
    N.Color = GPartyPalette[N.Id % UE_ARRAY_COUNT(GPartyPalette)];
    if (M.Color.Len() == 7 && M.Color.StartsWith(TEXT("#")))
    {
        const FLinearColor C = FLinearColor::FromSRGBColor(FColor::FromHex(M.Color));
        if (C.GetLuminance() > 0.12f) N.Color = C;
    }
    Players.Add(N);
    EnsurePlayerState(Players.Last());
    UE_LOG(LogPTLocalPartySub, Log, TEXT("Entró '%s' desde el chat de %s."), *Name,
        M.Platform == EPTChatPlatform::Kick ? TEXT("Kick") : TEXT("Twitch"));
    OnPlayersChanged.Broadcast();
    PushStates(true);
}
