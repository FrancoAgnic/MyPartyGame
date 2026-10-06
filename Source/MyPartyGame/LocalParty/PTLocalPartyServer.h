// Servidor HTTP + WebSocket MINIMO para el modo local (estilo Jackbox).
//
// Un solo puerto TCP: los celulares entran con el navegador a http://<IP-de-la-PC>:<puerto>/,
// reciben la página (archivos de Content/LocalParty/Web/) y esa página abre un WebSocket al
// mismo puerto (ruta /ws). Todo corre en el GAME THREAD con sockets no bloqueantes: Tick()
// acepta conexiones, lee, parsea y despacha. Sin dependencias externas (solo Sockets + Core).
//
// No es un servidor web de propósito general: alcanza para una LAN de living con ~10 celulares.

#pragma once
#include "CoreMinimal.h"

class FSocket;

class MYPARTYGAME_API FPTLocalPartyServer
{
public:
    DECLARE_DELEGATE_OneParam(FOnClientEvent, int32 /*ClientId*/);
    DECLARE_DELEGATE_TwoParams(FOnClientMessage, int32 /*ClientId*/, const FString& /*Text*/);

    FOnClientEvent   OnClientConnected;    // WebSocket abierto (handshake OK)
    FOnClientEvent   OnClientDisconnected; // WebSocket cerrado / caído
    FOnClientMessage OnClientMessage;      // frame de texto recibido

    FPTLocalPartyServer() = default;
    FPTLocalPartyServer(const FPTLocalPartyServer&) = delete;            // dueño de sockets: no copiable
    FPTLocalPartyServer& operator=(const FPTLocalPartyServer&) = delete;
    ~FPTLocalPartyServer();

    // Abre el socket de escucha. WebRoot = carpeta con index.html y demás archivos estáticos.
    bool Start(int32 InPort, const FString& InWebRoot);
    void Stop();
    bool IsRunning() const { return ListenSocket != nullptr; }
    int32 GetPort() const { return Port; }

    // Llamar en cada frame (game thread).
    void Tick();

    // Manda un frame de texto a un cliente WebSocket (o a todos).
    void Send(int32 ClientId, const FString& Text);
    void Broadcast(const FString& Text);
    void Disconnect(int32 ClientId);

    // IP de la PC en la red local (prefiere 192.168.x / 10.x / 172.16-31.x). Vacío si no hay.
    static FString GetLanIp();

private:
    struct FConn
    {
        int32         Id = 0;
        FSocket*      Socket = nullptr;
        TArray<uint8> In;          // bytes recibidos sin procesar
        TArray<uint8> Out;         // bytes pendientes de enviar (envíos parciales)
        bool          bWebSocket = false;
        bool          bCloseAfterSend = false; // HTTP plano: cerrar al vaciar Out
        bool          bDead = false;
        double        LastActivity = 0.0;
        TArray<uint8> Fragment;    // mensaje WS fragmentado en curso
    };

    FSocket*     ListenSocket = nullptr;
    int32        Port = 0;
    FString      WebRoot;
    int32        NextId = 1;
    TArray<TUniquePtr<FConn>> Conns;

    FConn* Find(int32 Id);
    void AcceptNew();
    void ReadFrom(FConn& C);
    void FlushOut(FConn& C);
    void ProcessHttp(FConn& C);
    void ProcessWebSocket(FConn& C);
    void ServeFile(FConn& C, const FString& Path);
    void QueueRaw(FConn& C, const uint8* Data, int32 Num);
    void QueueFrame(FConn& C, uint8 Opcode, const uint8* Payload, int32 Num);
    void CloseConn(FConn& C);
};
