// Transporte del MODO ONLINE: el juego se conecta a un relay en internet (Tools/Relay) y los celulares
// entran con un código de sala, desde cualquier red. El relay solo reenvía mensajes; si se corta la
// conexión, se reconecta solo y retoma la MISMA sala (código + secreto).

#pragma once
#include "CoreMinimal.h"
#include "PTPartyTransport.h"

class IWebSocket;

class MYPARTYGAME_API FPTRelayTransport : public IPTPartyTransport
{
public:
    enum class EStatus : uint8 { Connecting, Ready, Error };

    FPTRelayTransport() = default;
    FPTRelayTransport(const FPTRelayTransport&) = delete;
    FPTRelayTransport& operator=(const FPTRelayTransport&) = delete;
    virtual ~FPTRelayTransport() override;

    /** RelayUrl = "wss://play.tudominio.com" (o "ws://127.0.0.1:8080" para probar en local). */
    void Start(const FString& InRelayUrl);

    EStatus GetStatus() const { return Status; }
    const FString& GetRoomCode() const { return RoomCode; }
    const FString& GetLastError() const { return LastError; }
    /** "https://play.tudominio.com" (lo que se abre en el celular, sin el código). */
    FString GetPublicBaseUrl() const;

    // IPTPartyTransport
    virtual void Tick() override;
    virtual void Stop() override;
    virtual bool IsRunning() const override { return Status == EStatus::Ready; }
    virtual void Send(int32 ClientId, const FString& Text) override;
    virtual void Broadcast(const FString& Text) override;
    virtual void Disconnect(int32 ClientId) override;

private:
    TSharedPtr<IWebSocket> Socket;
    FString RelayUrl;
    FString RoomCode;
    FString RoomSecret;
    FString LastError;
    EStatus Status = EStatus::Connecting;
    bool    bStopped = false;
    double  ReconnectAt = 0.0;
    int32   Attempts = 0;
    TSet<int32> OpenClients;
    // Relay v2: los Send del frame se juntan y salen en UN mensaje "_multi" en el próximo Tick.
    bool bRelayBatches = false;
    TArray<TPair<int32, FString>> PendingSends;
    void FlushPending();

    void Connect();
    void ScheduleReconnect(const FString& Why);
    void HandleMessage(const FString& Text);
    void SendJson(const TSharedRef<class FJsonObject>& Obj);
};
