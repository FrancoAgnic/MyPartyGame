// Cómo llegan los celulares al juego. Dos implementaciones con la MISMA interfaz:
//  · FPTLocalPartyServer : servidor HTTP/WebSocket en la PC (celulares en el mismo WiFi).
//  · FPTRelayTransport   : el juego se conecta a un relay en internet y los celulares entran con un
//                          código de sala (modo online / streamers). Ver Tools/Relay.
// El subsistema (UPTLocalPartySubsystem) no sabe cuál está usando.

#pragma once
#include "CoreMinimal.h"

class MYPARTYGAME_API IPTPartyTransport
{
public:
    DECLARE_DELEGATE_OneParam(FOnClientEvent, int32 /*ClientId*/);
    DECLARE_DELEGATE_TwoParams(FOnClientMessage, int32 /*ClientId*/, const FString& /*Text*/);

    FOnClientEvent   OnClientConnected;    // un celular abrió su conexión
    FOnClientEvent   OnClientDisconnected; // se cerró / se cayó
    FOnClientMessage OnClientMessage;      // mensaje de texto de un celular

    virtual ~IPTPartyTransport() = default;

    virtual void Tick() = 0;               // llamar cada frame (game thread)
    virtual void Stop() = 0;
    virtual bool IsRunning() const = 0;    // listo para recibir celulares
    virtual void Send(int32 ClientId, const FString& Text) = 0;
    virtual void Broadcast(const FString& Text) = 0;
    virtual void Disconnect(int32 ClientId) = 0;
};
