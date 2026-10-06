#include "PTLocalPartyServer.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "SocketTypes.h"
#include "IPAddress.h"
#include "Misc/SecureHash.h"
#include "Misc/Base64.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformTime.h"

DEFINE_LOG_CATEGORY_STATIC(LogPTLocalParty, Log, All);

namespace
{
    constexpr int32  MaxHttpHeader   = 16 * 1024;   // pedido HTTP más largo que esto = basura
    constexpr int32  MaxWsMessage    = 64 * 1024;   // un mensaje de celular nunca debería pasar esto
    constexpr double HttpIdleTimeout = 10.0;        // conexión HTTP colgada sin pedido completo
    constexpr double WsIdleTimeout   = 30.0;        // el celular manda "ping" cada ~5 s

    ISocketSubsystem* Sockets() { return ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM); }

    FString HeaderValue(const FString& Head, const FString& Name)
    {
        TArray<FString> Lines;
        Head.ParseIntoArrayLines(Lines);
        for (const FString& L : Lines)
        {
            int32 Colon;
            if (!L.FindChar(TEXT(':'), Colon)) continue;
            if (L.Left(Colon).TrimStartAndEnd().Equals(Name, ESearchCase::IgnoreCase))
                return L.Mid(Colon + 1).TrimStartAndEnd();
        }
        return FString();
    }

    FString ContentTypeFor(const FString& Path)
    {
        const FString Ext = FPaths::GetExtension(Path).ToLower();
        if (Ext == TEXT("html")) return TEXT("text/html; charset=utf-8");
        if (Ext == TEXT("js"))   return TEXT("application/javascript; charset=utf-8");
        if (Ext == TEXT("css"))  return TEXT("text/css; charset=utf-8");
        if (Ext == TEXT("json")) return TEXT("application/json; charset=utf-8");
        if (Ext == TEXT("png"))  return TEXT("image/png");
        if (Ext == TEXT("svg"))  return TEXT("image/svg+xml");
        if (Ext == TEXT("ico"))  return TEXT("image/x-icon");
        if (Ext == TEXT("mp3"))  return TEXT("audio/mpeg");
        if (Ext == TEXT("wav"))  return TEXT("audio/wav");
        return TEXT("application/octet-stream");
    }

    bool IsPrivateLan(const TArray<uint8>& Ip)
    {
        if (Ip.Num() != 4) return false;
        return Ip[0] == 192 && Ip[1] == 168
            || Ip[0] == 10
            || (Ip[0] == 172 && Ip[1] >= 16 && Ip[1] <= 31);
    }
}

FPTLocalPartyServer::~FPTLocalPartyServer()
{
    Stop();
}

bool FPTLocalPartyServer::Start(int32 InPort, const FString& InWebRoot)
{
    Stop();
    ISocketSubsystem* SS = Sockets();
    if (!SS) return false;

    FSocket* S = SS->CreateSocket(NAME_Stream, TEXT("PTLocalPartyListen"), FNetworkProtocolTypes::IPv4);
    if (!S) return false;

    TSharedRef<FInternetAddr> Addr = SS->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
    Addr->SetAnyAddress();
    Addr->SetPort(InPort);

    S->SetReuseAddr(true);
    S->SetNonBlocking(true);
    if (!S->Bind(*Addr) || !S->Listen(16))
    {
        UE_LOG(LogPTLocalParty, Error, TEXT("No se pudo abrir el puerto %d (¿ocupado?)."), InPort);
        SS->DestroySocket(S);
        return false;
    }

    ListenSocket = S;
    Port = InPort;
    WebRoot = InWebRoot;
    UE_LOG(LogPTLocalParty, Log, TEXT("Servidor local escuchando en %s:%d (web: %s)"), *GetLanIp(), Port, *WebRoot);
    return true;
}

void FPTLocalPartyServer::Stop()
{
    ISocketSubsystem* SS = Sockets();
    for (TUniquePtr<FConn>& C : Conns)
    {
        if (C->Socket)
        {
            C->Socket->Close();
            if (SS) SS->DestroySocket(C->Socket);
            C->Socket = nullptr;
        }
    }
    Conns.Reset();
    if (ListenSocket)
    {
        ListenSocket->Close();
        if (SS) SS->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }
}

FString FPTLocalPartyServer::GetLanIp()
{
    ISocketSubsystem* SS = Sockets();
    if (!SS) return FString();

    TArray<TSharedPtr<FInternetAddr>> Addrs;
    SS->GetLocalAdapterAddresses(Addrs);

    // Preferir 192.168.x (WiFi de casa), después cualquier privada, después lo que haya.
    FString Best, AnyPrivate, AnyV4;
    for (const TSharedPtr<FInternetAddr>& A : Addrs)
    {
        if (!A.IsValid()) continue;
        const TArray<uint8> Ip = A->GetRawIp();
        if (Ip.Num() != 4 || Ip[0] == 127 || Ip[0] == 169) continue; // loopback / APIPA
        const FString S = A->ToString(false);
        if (Ip[0] == 192 && Ip[1] == 168 && Best.IsEmpty()) Best = S;
        if (IsPrivateLan(Ip) && AnyPrivate.IsEmpty()) AnyPrivate = S;
        if (AnyV4.IsEmpty()) AnyV4 = S;
    }
    if (!Best.IsEmpty()) return Best;
    if (!AnyPrivate.IsEmpty()) return AnyPrivate;
    if (!AnyV4.IsEmpty()) return AnyV4;

    bool bCanBindAll = false;
    TSharedRef<FInternetAddr> Host = SS->GetLocalHostAddr(*GLog, bCanBindAll);
    return Host->ToString(false);
}

FPTLocalPartyServer::FConn* FPTLocalPartyServer::Find(int32 Id)
{
    for (TUniquePtr<FConn>& C : Conns) if (C->Id == Id && !C->bDead) return C.Get();
    return nullptr;
}

void FPTLocalPartyServer::Tick()
{
    if (!ListenSocket) return;
    AcceptNew();

    const double Now = FPlatformTime::Seconds();
    for (int32 i = 0; i < Conns.Num(); ++i)
    {
        FConn& C = *Conns[i];
        if (!C.bDead) ReadFrom(C);
        if (!C.bDead)
        {
            if (C.bWebSocket) ProcessWebSocket(C);
            else              ProcessHttp(C);
        }
        if (!C.bDead) FlushOut(C);
        if (!C.bDead && Now - C.LastActivity > (C.bWebSocket ? WsIdleTimeout : HttpIdleTimeout))
            CloseConn(C);
    }

    // Barrer las muertas DESPUÉS de iterar (los delegates pueden mandar a otras conexiones).
    for (int32 i = Conns.Num() - 1; i >= 0; --i)
    {
        if (!Conns[i]->bDead) continue;
        if (Conns[i]->Socket)
        {
            Conns[i]->Socket->Close();
            Sockets()->DestroySocket(Conns[i]->Socket);
        }
        Conns.RemoveAt(i);
    }
}

void FPTLocalPartyServer::AcceptNew()
{
    bool bPending = false;
    while (ListenSocket->HasPendingConnection(bPending) && bPending)
    {
        FSocket* S = ListenSocket->Accept(TEXT("PTLocalPartyClient"));
        if (!S) break;
        S->SetNonBlocking(true);
        S->SetNoDelay(true);
        TUniquePtr<FConn> C = MakeUnique<FConn>();
        C->Id = NextId++;
        C->Socket = S;
        C->LastActivity = FPlatformTime::Seconds();
        Conns.Add(MoveTemp(C));
    }
}

void FPTLocalPartyServer::ReadFrom(FConn& C)
{
    uint8 Buf[4096];
    for (;;)
    {
        int32 Read = 0;
        if (!C.Socket->Recv(Buf, sizeof(Buf), Read))
        {
            CloseConn(C); // 0 bytes en stream = el otro lado cerró
            return;
        }
        if (Read <= 0) return; // would-block: nada más por ahora
        C.In.Append(Buf, Read);
        C.LastActivity = FPlatformTime::Seconds();
        if (C.In.Num() > MaxWsMessage * 2) { CloseConn(C); return; }
    }
}

void FPTLocalPartyServer::FlushOut(FConn& C)
{
    while (C.Out.Num() > 0)
    {
        int32 Sent = 0;
        if (!C.Socket->Send(C.Out.GetData(), C.Out.Num(), Sent))
        {
            if (Sockets()->GetLastErrorCode() == SE_EWOULDBLOCK) return; // buffer lleno: reintentar el próximo tick
            CloseConn(C);
            return;
        }
        if (Sent <= 0) return;
        C.Out.RemoveAt(0, Sent, EAllowShrinking::No);
    }
    if (C.bCloseAfterSend) CloseConn(C);
}

void FPTLocalPartyServer::QueueRaw(FConn& C, const uint8* Data, int32 Num)
{
    C.Out.Append(Data, Num);
}

void FPTLocalPartyServer::ProcessHttp(FConn& C)
{
    // Esperar el pedido completo (cabecera terminada en \r\n\r\n). Los GET no traen cuerpo.
    int32 End = INDEX_NONE;
    for (int32 i = 3; i < C.In.Num(); ++i)
        if (C.In[i - 3] == '\r' && C.In[i - 2] == '\n' && C.In[i - 1] == '\r' && C.In[i] == '\n') { End = i + 1; break; }
    if (End == INDEX_NONE)
    {
        if (C.In.Num() > MaxHttpHeader) CloseConn(C);
        return;
    }

    TArray<uint8> HeadBytes(C.In.GetData(), End);
    HeadBytes.Add(0);
    const FString Request = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(HeadBytes.GetData()));
    C.In.RemoveAt(0, End);

    FString FirstLine, Rest;
    Request.Split(TEXT("\r\n"), &FirstLine, &Rest);
    TArray<FString> Parts;
    FirstLine.ParseIntoArrayWS(Parts);
    if (Parts.Num() < 2 || Parts[0] != TEXT("GET")) { CloseConn(C); return; }

    FString Path = Parts[1];
    int32 Q;
    if (Path.FindChar(TEXT('?'), Q)) Path = Path.Left(Q);

    const FString Upgrade = HeaderValue(Rest, TEXT("Upgrade"));
    if (Path == TEXT("/ws") && Upgrade.Equals(TEXT("websocket"), ESearchCase::IgnoreCase))
    {
        const FString Key = HeaderValue(Rest, TEXT("Sec-WebSocket-Key"));
        if (Key.IsEmpty()) { CloseConn(C); return; }

        // RFC 6455: Accept = base64(SHA1(Key + GUID)).
        const FTCHARToUTF8 Src(*(Key + TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11")));
        uint8 Hash[20];
        FSHA1::HashBuffer(Src.Get(), Src.Length(), Hash);
        const FString Accept = FBase64::Encode(Hash, 20);

        const FString Resp = FString::Printf(
            TEXT("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n"),
            *Accept);
        const FTCHARToUTF8 R(*Resp);
        QueueRaw(C, reinterpret_cast<const uint8*>(R.Get()), R.Length());
        C.bWebSocket = true;
        OnClientConnected.ExecuteIfBound(C.Id);
        return;
    }

    ServeFile(C, Path);
}

void FPTLocalPartyServer::ServeFile(FConn& C, const FString& InPath)
{
    FString Rel = InPath;
    while (Rel.StartsWith(TEXT("/"))) Rel.RightChopInline(1);
    if (Rel.IsEmpty()) Rel = TEXT("index.html");

    TArray<uint8> Body;
    FString Status = TEXT("200 OK");
    const bool bSafe = !Rel.Contains(TEXT("..")) && !Rel.Contains(TEXT(":")) && !Rel.Contains(TEXT("\\"));
    const FString Full = FPaths::Combine(WebRoot, Rel);
    if (!bSafe || !FFileHelper::LoadFileToArray(Body, *Full, FILEREAD_Silent))
    {
        Status = TEXT("404 Not Found");
        const FTCHARToUTF8 NotFound(TEXT("No encontrado"));
        Body = TArray<uint8>(reinterpret_cast<const uint8*>(NotFound.Get()), NotFound.Length());
        Rel = TEXT("x.txt");
    }

    const FString Header = FString::Printf(
        TEXT("HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\nCache-Control: no-cache\r\nConnection: close\r\n\r\n"),
        *Status, Rel.EndsWith(TEXT(".txt")) ? TEXT("text/plain; charset=utf-8") : *ContentTypeFor(Rel), Body.Num());
    const FTCHARToUTF8 H(*Header);
    QueueRaw(C, reinterpret_cast<const uint8*>(H.Get()), H.Length());
    QueueRaw(C, Body.GetData(), Body.Num());
    C.bCloseAfterSend = true;
}

void FPTLocalPartyServer::ProcessWebSocket(FConn& C)
{
    // Frames del cliente: SIEMPRE enmascarados (RFC 6455 §5.3).
    for (;;)
    {
        if (C.In.Num() < 2) return;
        const uint8 B0 = C.In[0], B1 = C.In[1];
        const bool  bFin    = (B0 & 0x80) != 0;
        const uint8 Opcode  = B0 & 0x0F;
        const bool  bMasked = (B1 & 0x80) != 0;
        uint64 Len = B1 & 0x7F;
        int32 Pos = 2;
        if (Len == 126)
        {
            if (C.In.Num() < 4) return;
            Len = (uint64(C.In[2]) << 8) | C.In[3];
            Pos = 4;
        }
        else if (Len == 127)
        {
            if (C.In.Num() < 10) return;
            Len = 0;
            for (int32 i = 0; i < 8; ++i) Len = (Len << 8) | C.In[2 + i];
            Pos = 10;
        }
        if (!bMasked || Len > (uint64)MaxWsMessage) { CloseConn(C); return; }
        if (C.In.Num() < Pos + 4 + (int32)Len) return; // frame incompleto

        const uint8* Mask = &C.In[Pos];
        Pos += 4;
        TArray<uint8> Payload;
        Payload.SetNumUninitialized((int32)Len);
        for (int32 i = 0; i < (int32)Len; ++i) Payload[i] = C.In[Pos + i] ^ Mask[i & 3];
        C.In.RemoveAt(0, Pos + (int32)Len);

        switch (Opcode)
        {
            case 0x0: // continuación
            case 0x1: // texto
            {
                C.Fragment.Append(Payload);
                if (C.Fragment.Num() > MaxWsMessage) { CloseConn(C); return; }
                if (bFin)
                {
                    C.Fragment.Add(0);
                    const FString Text = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(C.Fragment.GetData()));
                    C.Fragment.Reset();
                    OnClientMessage.ExecuteIfBound(C.Id, Text);
                    if (C.bDead) return;
                }
                break;
            }
            case 0x8: // close
                QueueFrame(C, 0x8, nullptr, 0);
                FlushOut(C);
                CloseConn(C);
                return;
            case 0x9: // ping → pong con el mismo payload
                QueueFrame(C, 0xA, Payload.GetData(), Payload.Num());
                break;
            default: // pong / binario: ignorar
                break;
        }
    }
}

void FPTLocalPartyServer::QueueFrame(FConn& C, uint8 Opcode, const uint8* Payload, int32 Num)
{
    uint8 Header[10];
    int32 H = 0;
    Header[H++] = 0x80 | Opcode; // FIN + opcode; el servidor NO enmascara
    if (Num < 126)
    {
        Header[H++] = (uint8)Num;
    }
    else if (Num <= 0xFFFF)
    {
        Header[H++] = 126;
        Header[H++] = (uint8)(Num >> 8);
        Header[H++] = (uint8)(Num & 0xFF);
    }
    else
    {
        Header[H++] = 127;
        const uint64 L = (uint64)Num;
        for (int32 i = 7; i >= 0; --i) Header[H++] = (uint8)((L >> (8 * i)) & 0xFF);
    }
    QueueRaw(C, Header, H);
    if (Num > 0) QueueRaw(C, Payload, Num);
}

void FPTLocalPartyServer::Send(int32 ClientId, const FString& Text)
{
    FConn* C = Find(ClientId);
    if (!C || !C->bWebSocket) return;
    const FTCHARToUTF8 U(*Text);
    QueueFrame(*C, 0x1, reinterpret_cast<const uint8*>(U.Get()), U.Length());
    FlushOut(*C);
}

void FPTLocalPartyServer::Broadcast(const FString& Text)
{
    const FTCHARToUTF8 U(*Text);
    for (TUniquePtr<FConn>& C : Conns)
    {
        if (C->bDead || !C->bWebSocket) continue;
        QueueFrame(*C, 0x1, reinterpret_cast<const uint8*>(U.Get()), U.Length());
        FlushOut(*C);
    }
}

void FPTLocalPartyServer::Disconnect(int32 ClientId)
{
    if (FConn* C = Find(ClientId))
    {
        QueueFrame(*C, 0x8, nullptr, 0);
        FlushOut(*C);
        CloseConn(*C);
    }
}

void FPTLocalPartyServer::CloseConn(FConn& C)
{
    if (C.bDead) return;
    C.bDead = true;
    if (C.bWebSocket) OnClientDisconnected.ExecuteIfBound(C.Id);
}
