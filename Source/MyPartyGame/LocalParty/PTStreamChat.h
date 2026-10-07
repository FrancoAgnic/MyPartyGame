// Lector del CHAT del stream (Twitch o Kick) para el modo audiencia: la gente escribe !unirse en el
// chat del streamer y juega adivinando desde ahí, sin celular.
//
// Solo LEE (sin cuenta ni contraseña):
//   · Twitch: IRC por WebSocket con un usuario anónimo "justinfanNNNN" (wss://irc-ws.chat.twitch.tv).
//   · Kick: el WebSocket público (Pusher) que usa la propia página de kick.com; antes se busca el id
//     del chatroom del canal en https://kick.com/api/v2/channels/<canal>.
// Si se corta, se reconecta solo (con espera creciente, hasta 30 s).

#pragma once
#include "CoreMinimal.h"

class IWebSocket;

enum class EPTChatPlatform : uint8 { Twitch, Kick };

struct FPTStreamChatMessage
{
    EPTChatPlatform Platform = EPTChatPlatform::Twitch;
    FString UserId;   // id estable de la plataforma (no cambia si se cambia el nombre)
    FString UserName; // nombre para mostrar
    FString Color;    // "#RRGGBB" del chat (puede venir vacío)
    FString Text;
};

class MYPARTYGAME_API FPTStreamChat : public TSharedFromThis<FPTStreamChat>
{
public:
    enum class EStatus : uint8 { Off, Connecting, Connected, Error };
    DECLARE_DELEGATE_OneParam(FOnMessage, const FPTStreamChatMessage&);

    explicit FPTStreamChat(EPTChatPlatform InPlatform) : Platform(InPlatform) {}
    FPTStreamChat(const FPTStreamChat&) = delete;
    FPTStreamChat& operator=(const FPTStreamChat&) = delete;
    ~FPTStreamChat();

    /** Canal = nombre o link del canal ("micanal", "twitch.tv/micanal", "https://kick.com/micanal").
     *  Vacío = apagar. */
    void Start(const FString& InChannel);
    void Stop();
    /** Reintentos / keepalive. Llamar cada frame. */
    void Tick();

    EPTChatPlatform GetPlatform() const { return Platform; }
    EStatus GetStatus() const { return Status; }
    const FString& GetChannel() const { return Channel; }
    const FString& GetLastError() const { return LastError; }
    /** Error definitivo (ej. el canal de Kick no existe): no se reintenta hasta cambiar el canal. */
    bool HasGivenUp() const { return Status == EStatus::Error && ReconnectAt <= 0.0; }

    /** "https://www.twitch.tv/micanal" → "micanal" (minúsculas, solo caracteres válidos). */
    static FString NormalizeChannel(EPTChatPlatform Platform, const FString& In);

    FOnMessage OnMessage;

private:
    EPTChatPlatform Platform;
    EStatus Status = EStatus::Off;
    FString Channel;
    FString LastError;
    TSharedPtr<IWebSocket> Socket;
    int64   KickChatroomId = 0;
    bool    bLookupPending = false;
    double  ReconnectAt = 0.0;
    double  LastPingAt = 0.0;
    int32   Attempts = 0;

    void Connect();
    void CloseSocket();
    void Fail(const FString& Why, bool bRetry = true);
    void LookupKickChatroom();
    void ConnectSocket();
    void HandleTwitch(const FString& Raw);
    void HandleKick(const FString& Raw);
};
