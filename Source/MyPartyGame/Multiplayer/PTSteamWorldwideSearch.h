// Búsqueda directa de Steam lobbies con distancia WORLDWIDE (sin el filtro regional del engine).
// Esta clase no es un UObject para que UHT no procese los macros STEAM_CALLBACK_MANUAL/CCallResult.
#pragma once

#if PT_WITH_STEAM

#include "CoreMinimal.h"
#include "steam/steam_api.h"
#include "steam/isteammatchmaking.h"

// Datos extraídos de una lobby de Steam (sin depender de los tipos privados de OnlineSubsystemSteam).
struct FPTLobbyEntry
{
	uint64  LobbyId              = 0;
	FString Name;
	int32   NumPublicConnections = 0;
	int32   NumOpenPublic        = 0;
	int32   CurPlayers           = 0; // jugadores actuales (clave propia CUR_PLAYERS que el host actualiza)
	bool    bHasPassword         = false; // ahora = "privada solo amigos"
	FString P2PAddr;   // Steam64 ID del host como string, ej. "76561198XXXXXXXX" (= dueño de la sesión)
	int32   P2PPort    = 7777;
};

// -------------------------------------------------------------------------
// FPTSteamWorldwideSearch — forward-declarado como 'struct' en el header del
// subsistema, así que la definición también debe ser struct.
// Lanza RequestLobbyList con k_ELobbyDistanceFilterWorldwide y recopila los
// datos de cada lobby. Invocar Start(); el callback se dispara al terminar.
// -------------------------------------------------------------------------
struct FPTSteamWorldwideSearch
{
	FPTSteamWorldwideSearch();
	~FPTSteamWorldwideSearch();

	void Start(int32 MaxResults,
	           TFunction<void(TArray<FPTLobbyEntry>&&, bool bSuccess)> InCallback);

private:
	void OnLobbyMatchList(LobbyMatchList_t* pParam, bool bIOFailure);
	// STEAM_CALLBACK_MANUAL declara tanto el CCallback como la función — no redeclarar manualmente.
	STEAM_CALLBACK_MANUAL(FPTSteamWorldwideSearch, OnLobbyDataUpdateInternal,
	                      LobbyDataUpdate_t, LobbyDataCallback);
	void CheckComplete();

	CCallResult<FPTSteamWorldwideSearch, LobbyMatchList_t> MatchListCallResult;
	TArray<uint64>        PendingIds;
	TArray<FPTLobbyEntry> Collected;
	TFunction<void(TArray<FPTLobbyEntry>&&, bool)> Callback;
};

// -------------------------------------------------------------------------
// FPTSteamDirectJoin — envuelve JoinLobby + LobbyEnter_t.
// Callback: (bool bSuccess, FString ConnectURL)  "steam.[Steam64Id]:[Port]"
// -------------------------------------------------------------------------
struct FPTSteamDirectJoin
{
	FPTSteamDirectJoin();

	void JoinLobby(uint64 LobbyId,
	               TFunction<void(bool bSuccess, FString ConnectURL)> InCallback);

private:
	void OnLobbyEnter(LobbyEnter_t* pParam, bool bIOFailure);

	CCallResult<FPTSteamDirectJoin, LobbyEnter_t> JoinCallResult;
	TFunction<void(bool, FString)> Callback;
	uint64 PendingLobbyId = 0;
};

#endif // PT_WITH_STEAM
