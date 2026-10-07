#include "PTStreamChat.h"
#include "IWebSocket.h"
#include "WebSocketsModule.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/PlatformTime.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogPTStreamChat, Log, All);

namespace
{
    const TCHAR* ChatPlatformName(EPTChatPlatform P) { return P == EPTChatPlatform::Kick ? TEXT("Kick") : TEXT("Twitch"); }

    // Valores por defecto de Kick (la key pública de Pusher que kick.com le da a todos los navegadores).
    const TCHAR* KickPusherDefault = TEXT("wss://ws-us2.pusher.com/app/32cbd69e4b950bf97679?protocol=7&client=js&version=8.4.0&flash=false");

    // URL de [LocalParty] en DefaultGame.ini (va ENTRE COMILLAS: sin comillas el "//" es comentario) o el default.
    FString ConfigUrl(const TCHAR* Key, const FString& Default)
    {
        FString V;
        if (GConfig->GetString(TEXT("LocalParty"), Key, V, GGameIni))
        {
            V.TrimStartAndEndInline();
            V.TrimQuotesInline();
            if (V.Contains(TEXT("://"))) return V;
        }
        return Default;
    }

    FString UnescapeIrcTag(const FString& In)
    {
        FString Out;
        for (int32 i = 0; i < In.Len(); ++i)
        {
            if (In[i] != TEXT('\\') || i + 1 >= In.Len()) { Out.AppendChar(In[i]); continue; }
            const TCHAR N = In[++i];
            Out.AppendChar(N == TEXT('s') ? TEXT(' ') : N == TEXT(':') ? TEXT(';') : N == TEXT('n') || N == TEXT('r') ? TEXT(' ') : N);
        }
        return Out;
    }

    // "[emote:37226:KEKW]" de Kick → se quita (no es parte de lo que la persona escribió).
    FString StripKickEmotes(const FString& In)
    {
        FString Out;
        int32 i = 0;
        while (i < In.Len())
        {
            if (In.Mid(i, 7) == TEXT("[emote:"))
            {
                const int32 End = In.Find(TEXT("]"), ESearchCase::CaseSensitive, ESearchDir::FromStart, i);
                if (End != INDEX_NONE) { i = End + 1; continue; }
            }
            Out.AppendChar(In[i++]);
        }
        return Out.TrimStartAndEnd();
    }
}

FPTStreamChat::~FPTStreamChat()
{
    CloseSocket();
}

FString FPTStreamChat::NormalizeChannel(EPTChatPlatform InPlatform, const FString& In)
{
    // Aceptar el link entero: quedarse con el último tramo de la ruta.
    FString S = In.TrimStartAndEnd();
    int32 Q;
    if (S.FindChar(TEXT('?'), Q)) S.LeftInline(Q);
    while (S.EndsWith(TEXT("/"))) S.LeftChopInline(1);
    int32 Slash;
    if (S.FindLastChar(TEXT('/'), Slash)) S.RightChopInline(Slash + 1);
    if (S.EndsWith(TEXT(":"))) S.Reset(); // "https:" suelto (la consola corta lo que sigue a "//")
    S.RemoveFromStart(TEXT("@"));
    S.RemoveFromStart(TEXT("#"));
    S = S.ToLower();

    FString Out;
    for (TCHAR C : S)
    {
        const bool bAlnum = (C >= TEXT('a') && C <= TEXT('z')) || (C >= TEXT('0') && C <= TEXT('9'));
        if (bAlnum) Out.AppendChar(C);
        else if (C == TEXT('_') || C == TEXT('-'))
            Out.AppendChar(InPlatform == EPTChatPlatform::Kick ? TEXT('-') : TEXT('_')); // Kick: "Mi_Canal" → "mi-canal"
    }
    return Out.Left(InPlatform == EPTChatPlatform::Kick ? 40 : 25);
}

void FPTStreamChat::Start(const FString& InChannel)
{
    const FString Norm = NormalizeChannel(Platform, InChannel);
    if (Norm == Channel && Status != EStatus::Off) return; // ya está en ese canal
    Stop();
    Channel = Norm;
    if (Channel.IsEmpty()) return;
    Attempts = 0;
    KickChatroomId = 0;
    Connect();
}

void FPTStreamChat::Stop()
{
    CloseSocket();
    Status = EStatus::Off;
    Channel.Reset();
    LastError.Reset();
    ReconnectAt = 0.0;
    bLookupPending = false; // la respuesta que llegue tarde se ignora (ver LookupKickChatroom)
}

void FPTStreamChat::CloseSocket()
{
    if (!Socket.IsValid()) return;
    Socket->OnConnected().Clear();
    Socket->OnConnectionError().Clear();
    Socket->OnClosed().Clear();
    Socket->OnMessage().Clear();
    Socket->Close();
    Socket.Reset();
}

void FPTStreamChat::Fail(const FString& Why, bool bRetry)
{
    // El socket NO se cierra acá: Fail se llama desde sus propios callbacks. Se reemplaza en el próximo
    // Connect (desde Tick), fuera de ellos.
    LastError = Why;
    Status = EStatus::Error;
    if (!bRetry)
    {
        ReconnectAt = 0.0;
        UE_LOG(LogPTStreamChat, Warning, TEXT("%s (%s): %s. Sin reintentos hasta cambiar el canal."), ChatPlatformName(Platform), *Channel, *Why);
        return;
    }
    const float Delay = FMath::Min(30.f, 2.f * FMath::Pow(2.f, (float)FMath::Min(Attempts, 4)));
    ++Attempts;
    ReconnectAt = FPlatformTime::Seconds() + Delay;
    UE_LOG(LogPTStreamChat, Warning, TEXT("%s (%s): %s. Reintento en %.0fs."), ChatPlatformName(Platform), *Channel, *Why, Delay);
}

void FPTStreamChat::Tick()
{
    const double Now = FPlatformTime::Seconds();
    if (Status == EStatus::Error && ReconnectAt > 0.0 && Now >= ReconnectAt)
    {
        ReconnectAt = 0.0;
        Connect();
        return;
    }
    // Kick (Pusher): si no se manda nada en 120 s el servidor corta. Un ping por minuto.
    if (Platform == EPTChatPlatform::Kick && Status == EStatus::Connected && Socket.IsValid() && Now - LastPingAt > 60.0)
    {
        LastPingAt = Now;
        Socket->Send(TEXT("{\"event\":\"pusher:ping\",\"data\":{}}"));
    }
}

void FPTStreamChat::Connect()
{
    if (Channel.IsEmpty()) return;
    Status = EStatus::Connecting;
    if (Platform == EPTChatPlatform::Kick && KickChatroomId == 0) { LookupKickChatroom(); return; }
    ConnectSocket();
}

void FPTStreamChat::LookupKickChatroom()
{
    if (bLookupPending) return;
    bLookupPending = true;
    const TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
    Req->SetURL(ConfigUrl(TEXT("KickApiUrl"), TEXT("https://kick.com/api/v2/channels")) / Channel);
    Req->SetVerb(TEXT("GET"));
    Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
    Req->SetHeader(TEXT("User-Agent"), TEXT("Mozilla/5.0 (Windows NT 10.0; Win64; x64) Sculpturillo"));
    const TWeakPtr<FPTStreamChat> Weak = AsShared();
    const FString AskedFor = Channel;
    Req->OnProcessRequestComplete().BindLambda([Weak, AskedFor](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
    {
        const TSharedPtr<FPTStreamChat> Self = Weak.Pin();
        if (!Self.IsValid() || !Self->bLookupPending || Self->Channel != AskedFor) return; // se apagó o cambió de canal
        Self->bLookupPending = false;
        const int32 Code = Resp.IsValid() ? Resp->GetResponseCode() : 0;
        if (Code == 404) { Self->Fail(TEXT("canal no encontrado"), /*bRetry=*/false); return; }
        if (!bOk || Code != 200) { Self->Fail(FString::Printf(TEXT("kick.com respondió %d"), Code)); return; }

        TSharedPtr<FJsonObject> J;
        const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
        const TSharedPtr<FJsonObject>* Room = nullptr;
        double Id = 0.0;
        if (FJsonSerializer::Deserialize(R, J) && J.IsValid() && J->TryGetObjectField(TEXT("chatroom"), Room)
            && Room && (*Room)->TryGetNumberField(TEXT("id"), Id) && Id > 0.0)
        {
            Self->KickChatroomId = (int64)Id;
            Self->ConnectSocket();
        }
        else
        {
            Self->Fail(TEXT("respuesta de kick.com sin chatroom"));
        }
    });
    Req->ProcessRequest();
}

void FPTStreamChat::ConnectSocket()
{
    CloseSocket();
    if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
        FModuleManager::Get().LoadModule(TEXT("WebSockets"));

    // Si Twitch / Kick cambian de dirección se puede pisar sin recompilar ([LocalParty] TwitchIrcUrl / KickPusherUrl).
    const FString Url = Platform == EPTChatPlatform::Kick
        ? ConfigUrl(TEXT("KickPusherUrl"), KickPusherDefault)
        : ConfigUrl(TEXT("TwitchIrcUrl"), TEXT("wss://irc-ws.chat.twitch.tv:443"));

    Socket = FWebSocketsModule::Get().CreateWebSocket(Url);
    const TWeakPtr<FPTStreamChat> Weak = AsShared();
    Socket->OnConnected().AddLambda([Weak]()
    {
        const TSharedPtr<FPTStreamChat> Self = Weak.Pin();
        if (!Self.IsValid() || !Self->Socket.IsValid()) return;
        if (Self->Platform == EPTChatPlatform::Twitch)
        {
            // Usuario anónimo de solo lectura (Twitch acepta cualquier "justinfan" + número sin contraseña).
            Self->Socket->Send(TEXT("CAP REQ :twitch.tv/tags twitch.tv/commands"));
            Self->Socket->Send(TEXT("PASS SCHMOOPIIE"));
            Self->Socket->Send(FString::Printf(TEXT("NICK justinfan%d"), FMath::RandRange(10000, 99999)));
            Self->Socket->Send(FString::Printf(TEXT("JOIN #%s"), *Self->Channel));
        }
        // Kick: se suscribe al llegar "pusher:connection_established".
    });
    Socket->OnConnectionError().AddLambda([Weak](const FString& Err)
    {
        if (const TSharedPtr<FPTStreamChat> Self = Weak.Pin()) Self->Fail(Err);
    });
    Socket->OnClosed().AddLambda([Weak](int32 Code, const FString& Reason, bool)
    {
        if (const TSharedPtr<FPTStreamChat> Self = Weak.Pin())
            Self->Fail(FString::Printf(TEXT("cerrado (%d %s)"), Code, *Reason));
    });
    Socket->OnMessage().AddLambda([Weak](const FString& Msg)
    {
        const TSharedPtr<FPTStreamChat> Self = Weak.Pin();
        if (!Self.IsValid()) return;
        if (Self->Platform == EPTChatPlatform::Twitch) Self->HandleTwitch(Msg);
        else Self->HandleKick(Msg);
    });
    Socket->Connect();
    UE_LOG(LogPTStreamChat, Log, TEXT("Conectando al chat de %s: %s"), ChatPlatformName(Platform), *Channel);
}

void FPTStreamChat::HandleTwitch(const FString& Raw)
{
    TArray<FString> Lines;
    Raw.ParseIntoArray(Lines, TEXT("\r\n"));
    for (FString Line : Lines)
    {
        if (Line.StartsWith(TEXT("PING")))
        {
            if (Socket.IsValid()) Socket->Send(TEXT("PONG") + Line.RightChop(4));
            continue;
        }

        // [@tags ]:prefijo COMANDO params
        FString Tags;
        if (Line.StartsWith(TEXT("@")))
        {
            int32 Sp;
            if (!Line.FindChar(TEXT(' '), Sp)) continue;
            Tags = Line.Mid(1, Sp - 1);
            Line.RightChopInline(Sp + 1);
        }
        FString Prefix;
        if (Line.StartsWith(TEXT(":")))
        {
            int32 Sp;
            if (!Line.FindChar(TEXT(' '), Sp)) continue;
            Prefix = Line.Mid(1, Sp - 1);
            Line.RightChopInline(Sp + 1);
        }
        FString Command, Params;
        if (!Line.Split(TEXT(" "), &Command, &Params)) Command = Line;

        if (Command == TEXT("JOIN") || Command == TEXT("ROOMSTATE"))
        {
            if (Status != EStatus::Connected)
                UE_LOG(LogPTStreamChat, Log, TEXT("Leyendo el chat de Twitch: #%s"), *Channel);
            Status = EStatus::Connected;
            Attempts = 0;
            LastError.Reset();
        }
        else if (Command == TEXT("RECONNECT"))
        {
            Fail(TEXT("Twitch pidió reconectar"));
            return;
        }
        else if (Command == TEXT("NOTICE") && Params.Contains(TEXT("authentication failed")))
        {
            Fail(TEXT("Twitch rechazó la conexión"));
            return;
        }
        else if (Command == TEXT("PRIVMSG"))
        {
            FString Target, Text;
            if (!Params.Split(TEXT(" :"), &Target, &Text)) continue;
            FPTStreamChatMessage M;
            M.Platform = EPTChatPlatform::Twitch;
            M.Text = Text;
            // /me (ACTION): "\x01ACTION texto\x01"
            if (M.Text.StartsWith(TEXT("\x01" "ACTION ")))
            {
                M.Text.RightChopInline(8);
                M.Text.RemoveFromEnd(TEXT("\x01"));
            }
            TArray<FString> Pairs;
            Tags.ParseIntoArray(Pairs, TEXT(";"));
            for (const FString& P : Pairs)
            {
                FString K, V;
                if (!P.Split(TEXT("="), &K, &V)) continue;
                if (K == TEXT("display-name")) M.UserName = UnescapeIrcTag(V);
                else if (K == TEXT("user-id")) M.UserId = V;
                else if (K == TEXT("color")) M.Color = V;
            }
            FString Login;
            if (!Prefix.Split(TEXT("!"), &Login, nullptr)) Login = Prefix;
            if (M.UserName.TrimStartAndEnd().IsEmpty()) M.UserName = Login;
            if (M.UserId.IsEmpty()) M.UserId = Login;
            if (!M.UserId.IsEmpty()) OnMessage.ExecuteIfBound(M);
        }
    }
}

void FPTStreamChat::HandleKick(const FString& Raw)
{
    TSharedPtr<FJsonObject> J;
    const TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(R, J) || !J.IsValid()) return;
    FString Event;
    J->TryGetStringField(TEXT("event"), Event);

    if (Event == TEXT("pusher:connection_established"))
    {
        if (Socket.IsValid())
            Socket->Send(FString::Printf(TEXT("{\"event\":\"pusher:subscribe\",\"data\":{\"auth\":\"\",\"channel\":\"chatrooms.%lld.v2\"}}"), KickChatroomId));
        LastPingAt = FPlatformTime::Seconds();
    }
    else if (Event == TEXT("pusher_internal:subscription_succeeded"))
    {
        UE_LOG(LogPTStreamChat, Log, TEXT("Leyendo el chat de Kick: %s (chatroom %lld)"), *Channel, KickChatroomId);
        Status = EStatus::Connected;
        Attempts = 0;
        LastError.Reset();
    }
    else if (Event == TEXT("pusher:ping"))
    {
        if (Socket.IsValid()) Socket->Send(TEXT("{\"event\":\"pusher:pong\",\"data\":{}}"));
    }
    else if (Event == TEXT("pusher:error"))
    {
        Fail(TEXT("error de Pusher"));
    }
    else if (Event == TEXT("App\\Events\\ChatMessageEvent"))
    {
        // "data" viene como TEXTO con otro JSON adentro.
        FString Data;
        if (!J->TryGetStringField(TEXT("data"), Data)) return;
        TSharedPtr<FJsonObject> D;
        const TSharedRef<TJsonReader<>> DR = TJsonReaderFactory<>::Create(Data);
        if (!FJsonSerializer::Deserialize(DR, D) || !D.IsValid()) return;
        const TSharedPtr<FJsonObject>* Sender = nullptr;
        if (!D->TryGetObjectField(TEXT("sender"), Sender) || !Sender) return;

        FPTStreamChatMessage M;
        M.Platform = EPTChatPlatform::Kick;
        FString Content;
        D->TryGetStringField(TEXT("content"), Content);
        M.Text = StripKickEmotes(Content);
        double Id = 0.0;
        if ((*Sender)->TryGetNumberField(TEXT("id"), Id)) M.UserId = FString::Printf(TEXT("%lld"), (int64)Id);
        (*Sender)->TryGetStringField(TEXT("username"), M.UserName);
        const TSharedPtr<FJsonObject>* Identity = nullptr;
        if ((*Sender)->TryGetObjectField(TEXT("identity"), Identity) && Identity)
            (*Identity)->TryGetStringField(TEXT("color"), M.Color);
        if (M.UserId.IsEmpty()) M.UserId = M.UserName.ToLower();
        if (!M.UserId.IsEmpty() && !M.Text.IsEmpty()) OnMessage.ExecuteIfBound(M);
    }
}
