#include "PTRelayTransport.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformTime.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPTRelay, Log, All);

FPTRelayTransport::~FPTRelayTransport()
{
    Stop();
}

void FPTRelayTransport::Start(const FString& InRelayUrl)
{
    RelayUrl = InRelayUrl;
    while (RelayUrl.EndsWith(TEXT("/"))) RelayUrl.LeftChopInline(1);
    bStopped = false;
    Attempts = 0;
    Connect();
}

FString FPTRelayTransport::GetPublicBaseUrl() const
{
    FString U = RelayUrl;
    if (U.StartsWith(TEXT("wss://")))     U = TEXT("https://") + U.RightChop(6);
    else if (U.StartsWith(TEXT("ws://"))) U = TEXT("http://") + U.RightChop(5);
    return U;
}

void FPTRelayTransport::Connect()
{
    if (bStopped || RelayUrl.IsEmpty()) return;
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
        FModuleManager::Get().LoadModule(TEXT("WebSockets"));

    Status = EStatus::Connecting;
    if (Socket.IsValid())
    {
        Socket->OnConnected().Clear();
        Socket->OnConnectionError().Clear();
        Socket->OnClosed().Clear();
        Socket->OnMessage().Clear();
        Socket->Close();
    }

    Socket = FWebSocketsModule::Get().CreateWebSocket(RelayUrl + TEXT("/host"));
    Socket->OnConnected().AddLambda([this]()
    {
        // Saludo: si ya teníamos sala, pedir retomarla (los celulares siguen conectados al relay).
        TSharedRef<FJsonObject> H = MakeShared<FJsonObject>();
        H->SetStringField(TEXT("t"), TEXT("hello"));
        if (!RoomCode.IsEmpty())
        {
            H->SetStringField(TEXT("code"), RoomCode);
            H->SetStringField(TEXT("secret"), RoomSecret);
        }
        SendJson(H);
    });
    Socket->OnConnectionError().AddLambda([this](const FString& Err) { ScheduleReconnect(Err); });
    Socket->OnClosed().AddLambda([this](int32 Code, const FString& Reason, bool)
    {
        ScheduleReconnect(FString::Printf(TEXT("cerrado (%d %s)"), Code, *Reason));
    });
    Socket->OnMessage().AddLambda([this](const FString& Msg) { HandleMessage(Msg); });
    Socket->Connect();
    UE_LOG(LogPTRelay, Log, TEXT("Conectando al relay %s ..."), *RelayUrl);
}

void FPTRelayTransport::ScheduleReconnect(const FString& Why)
{
    if (bStopped) return;
    // Los celulares que veíamos quedan "desconectados" para el juego hasta que el relay los re-anuncie.
    for (int32 Id : OpenClients) OnClientDisconnected.ExecuteIfBound(Id);
    OpenClients.Reset();

    LastError = Why;
    Status = EStatus::Error;
    const float Delay = FMath::Min(15.f, 1.f * FMath::Pow(2.f, (float)FMath::Min(Attempts, 4)));
    ++Attempts;
    ReconnectAt = FPlatformTime::Seconds() + Delay;
    UE_LOG(LogPTRelay, Warning, TEXT("Relay: %s. Reintento en %.0fs."), *Why, Delay);
}

void FPTRelayTransport::Tick()
{
    FlushPending();
    if (!bStopped && Status == EStatus::Error && ReconnectAt > 0.0 && FPlatformTime::Seconds() >= ReconnectAt)
    {
        ReconnectAt = 0.0;
        Connect();
    }
}

void FPTRelayTransport::Stop()
{
    FlushPending();
    bStopped = true;
    if (Socket.IsValid())
    {
        // Avisar al relay que cierre la sala (si no, espera la gracia por si volvemos).
        if (Socket->IsConnected())
        {
            TSharedRef<FJsonObject> C = MakeShared<FJsonObject>();
            C->SetStringField(TEXT("t"), TEXT("close"));
            SendJson(C);
        }
        Socket->OnConnected().Clear();
        Socket->OnConnectionError().Clear();
        Socket->OnClosed().Clear();
        Socket->OnMessage().Clear();
        Socket->Close();
        Socket.Reset();
    }
    OpenClients.Reset();
    RoomCode.Reset();
    RoomSecret.Reset();
    Status = EStatus::Connecting;
}

void FPTRelayTransport::HandleMessage(const FString& Text)
{
    TSharedPtr<FJsonObject> M;
    const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Text);
    if (!FJsonSerializer::Deserialize(R, M) || !M.IsValid()) return;
    const FString T = M->GetStringField(TEXT("t"));

    if (T == TEXT("room"))
    {
        const FString NewCode = M->GetStringField(TEXT("code"));
        if (!RoomCode.IsEmpty() && NewCode != RoomCode)
            UE_LOG(LogPTRelay, Warning, TEXT("El relay no pudo retomar la sala %s: sala nueva %s."), *RoomCode, *NewCode);
        RoomCode = NewCode;
        RoomSecret = M->GetStringField(TEXT("secret"));
        int32 RelayVersion = 1;
        M->TryGetNumberField(TEXT("v"), RelayVersion);
        bRelayBatches = RelayVersion >= 2;
        Status = EStatus::Ready;
        Attempts = 0;
        LastError.Reset();
        UE_LOG(LogPTRelay, Log, TEXT("Sala online lista: %s/%s"), *GetPublicBaseUrl(), *RoomCode);
    }
    else if (T == TEXT("_open"))
    {
        const int32 C = (int32)M->GetNumberField(TEXT("c"));
        OpenClients.Add(C);
        OnClientConnected.ExecuteIfBound(C);
    }
    else if (T == TEXT("_msg"))
    {
        const int32 C = (int32)M->GetNumberField(TEXT("c"));
        if (!OpenClients.Contains(C)) { OpenClients.Add(C); OnClientConnected.ExecuteIfBound(C); }
        OnClientMessage.ExecuteIfBound(C, M->GetStringField(TEXT("d")));
    }
    else if (T == TEXT("_close"))
    {
        const int32 C = (int32)M->GetNumberField(TEXT("c"));
        if (OpenClients.Remove(C) > 0) OnClientDisconnected.ExecuteIfBound(C);
    }
}

void FPTRelayTransport::SendJson(const TSharedRef<FJsonObject>& Obj)
{
    if (!Socket.IsValid() || !Socket->IsConnected()) return;
    FString Out;
    const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W =
        TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Out);
    FJsonSerializer::Serialize(Obj, W);
    Socket->Send(Out);
}

void FPTRelayTransport::FlushPending()
{
    if (PendingSends.Num() == 0) return;
    if (Socket.IsValid() && Socket->IsConnected())
    {
        TArray<TSharedPtr<FJsonValue>> Items;
        Items.Reserve(PendingSends.Num());
        for (const TPair<int32, FString>& P : PendingSends)
        {
            TArray<TSharedPtr<FJsonValue>> Pair;
            Pair.Add(MakeShared<FJsonValueNumber>(P.Key));
            Pair.Add(MakeShared<FJsonValueString>(P.Value));
            Items.Add(MakeShared<FJsonValueArray>(Pair));
        }
        TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("t"), TEXT("_multi"));
        O->SetArrayField(TEXT("m"), Items);
        SendJson(O);
    }
    PendingSends.Reset();
}

void FPTRelayTransport::Send(int32 ClientId, const FString& Text)
{
    if (!OpenClients.Contains(ClientId)) return;
    if (bRelayBatches)
    {
        // Se manda en el próximo Tick, junto con todo lo demás del frame (en orden).
        PendingSends.Emplace(ClientId, Text);
        return;
    }
    TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("t"), TEXT("_send"));
    O->SetNumberField(TEXT("c"), ClientId);
    O->SetStringField(TEXT("d"), Text);
    SendJson(O);
}

void FPTRelayTransport::Broadcast(const FString& Text)
{
    TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("t"), TEXT("_bcast"));
    O->SetStringField(TEXT("d"), Text);
    SendJson(O);
}

void FPTRelayTransport::Disconnect(int32 ClientId)
{
    if (OpenClients.Remove(ClientId) == 0) return;
    TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("t"), TEXT("_kick"));
    O->SetNumberField(TEXT("c"), ClientId);
    SendJson(O);
    OnClientDisconnected.ExecuteIfBound(ClientId);
}
