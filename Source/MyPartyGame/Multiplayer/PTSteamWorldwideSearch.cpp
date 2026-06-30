// Implementación de FPTSteamWorldwideSearch y FPTSteamDirectJoin.
#include "PTSteamWorldwideSearch.h"

#if PT_WITH_STEAM

// ===== SteamSessionKeys internos (copiados de SteamSessionKeys.h del engine plugin) =========
// El engine codifica las FName custom de FOnlineSessionSettings como "<NOMBRE>_<tipo>":
//   String → _s   Bool → _b   Int32 → _i
// Además, las claves fijas del engine tienen su propio define.
#define PT_STEAMKEY_NUMPUBCONN         "NUMPUBCONN"
#define PT_STEAMKEY_NUMOPENPUBCONN     "NUMOPENPUBCONN"
#define PT_STEAMKEY_OWNINGNAME         "OWNINGNAME"
#define PT_STEAMKEY_P2PADDR            "P2PADDR"
#define PT_STEAMKEY_P2PPORT            "P2PPORT"
#define PT_STEAMKEY_MATCH_TYPE         "MATCH_TYPE_s"
#define PT_STEAMKEY_HAS_PASSWORD       "HAS_PASSWORD_b"
#define PT_STEAMKEY_SERVER_NAME        "SERVER_NAME_s"
#define PT_STEAMKEY_CODE_HASH          "CODE_HASH_s"

// ===== FPTSteamWorldwideSearch ===============================================================

FPTSteamWorldwideSearch::FPTSteamWorldwideSearch()
{
	// LobbyDataCallback es STEAM_CALLBACK_MANUAL — no se registra hasta que Start() lo pida.
}

FPTSteamWorldwideSearch::~FPTSteamWorldwideSearch()
{
	LobbyDataCallback.Unregister();
}

void FPTSteamWorldwideSearch::Start(int32 MaxResults, const FString& InCodeHash,
                                    TFunction<void(TArray<FPTLobbyEntry>&&, bool)> InCallback)
{
	if (!SteamMatchmaking())
	{
		InCallback({}, false);
		return;
	}

	Callback   = MoveTemp(InCallback);
	PendingIds.Reset();
	Collected.Reset();

	// Registrar callback global para LobbyDataUpdate_t solo durante esta búsqueda.
	LobbyDataCallback.Register(this, &FPTSteamWorldwideSearch::OnLobbyDataUpdateInternal);

	// Filtros antes de RequestLobbyList (acumulativos, se aplican a la próxima llamada).
	SteamMatchmaking()->AddRequestLobbyListDistanceFilter(k_ELobbyDistanceFilterWorldwide);
	SteamMatchmaking()->AddRequestLobbyListStringFilter(PT_STEAMKEY_MATCH_TYPE, "PartyLobby",
	                                                    k_ELobbyComparisonEqual);
	SteamMatchmaking()->AddRequestLobbyListFilterSlotsAvailable(1);

	if (!InCodeHash.IsEmpty())
	{
		SteamMatchmaking()->AddRequestLobbyListStringFilter(
			PT_STEAMKEY_CODE_HASH, TCHAR_TO_ANSI(*InCodeHash), k_ELobbyComparisonEqual);
	}

	SteamMatchmaking()->AddRequestLobbyListResultCountFilter(MaxResults);

	SteamAPICall_t hCall = SteamMatchmaking()->RequestLobbyList();
	MatchListCallResult.Set(hCall, this, &FPTSteamWorldwideSearch::OnLobbyMatchList);
}

void FPTSteamWorldwideSearch::OnLobbyMatchList(LobbyMatchList_t* pParam, bool bIOFailure)
{
	if (bIOFailure || !pParam || !SteamMatchmaking())
	{
		LobbyDataCallback.Unregister();
		Callback({}, false);
		return;
	}

	const int32 Count = (int32)pParam->m_nLobbiesMatching;
	UE_LOG(LogTemp, Log, TEXT("[PTSteamWorldwide] RequestLobbyList: %d lobby(s) encontradas"), Count);

	if (Count == 0)
	{
		LobbyDataCallback.Unregister();
		Callback({}, true);
		return;
	}

	for (int32 i = 0; i < Count; ++i)
	{
		CSteamID LobbyId = SteamMatchmaking()->GetLobbyByIndex(i);
		if (LobbyId.IsValid())
		{
			PendingIds.Add(LobbyId.ConvertToUint64());
			SteamMatchmaking()->RequestLobbyData(LobbyId);
		}
	}

	if (PendingIds.IsEmpty())
	{
		LobbyDataCallback.Unregister();
		Callback({}, true);
	}
}

void FPTSteamWorldwideSearch::OnLobbyDataUpdateInternal(LobbyDataUpdate_t* pParam)
{
	if (!pParam) return;

	const uint64 LobbyId64 = pParam->m_ulSteamIDLobby;
	if (!PendingIds.Contains(LobbyId64)) return;
	PendingIds.Remove(LobbyId64);

	if (pParam->m_bSuccess && SteamMatchmaking())
	{
		CSteamID LobbyId(LobbyId64);

		FPTLobbyEntry Entry;
		Entry.LobbyId = LobbyId64;

		auto ReadKey = [&](const char* Key) -> FString
		{
			return FString(UTF8_TO_TCHAR(SteamMatchmaking()->GetLobbyData(LobbyId, Key)));
		};

		Entry.Name                = ReadKey(PT_STEAMKEY_SERVER_NAME);
		Entry.NumPublicConnections= FCString::Atoi(*ReadKey(PT_STEAMKEY_NUMPUBCONN));
		Entry.NumOpenPublic       = FCString::Atoi(*ReadKey(PT_STEAMKEY_NUMOPENPUBCONN));
		Entry.bHasPassword        = ReadKey(PT_STEAMKEY_HAS_PASSWORD) == TEXT("true");
		Entry.CodeHash            = ReadKey(PT_STEAMKEY_CODE_HASH);

		// P2PADDR almacena el Steam64 del host como string (ej. "76561198XXXXXXXX").
		// El engine a veces lo guarda con prefijo "steam." — lo quitamos por si acaso.
		FString P2PAddrRaw = ReadKey(PT_STEAMKEY_P2PADDR);
		P2PAddrRaw.RemoveFromStart(TEXT("steam."));
		Entry.P2PAddr = P2PAddrRaw;

		const FString PortStr = ReadKey(PT_STEAMKEY_P2PPORT);
		Entry.P2PPort = PortStr.IsEmpty() ? 7777 : FCString::Atoi(*PortStr);

		if (!Entry.P2PAddr.IsEmpty())
		{
			Collected.Add(MoveTemp(Entry));
		}
	}

	CheckComplete();
}

void FPTSteamWorldwideSearch::CheckComplete()
{
	if (PendingIds.IsEmpty())
	{
		LobbyDataCallback.Unregister();
		Callback(MoveTemp(Collected), true);
	}
}

// ===== FPTSteamDirectJoin ====================================================================

FPTSteamDirectJoin::FPTSteamDirectJoin() {}

void FPTSteamDirectJoin::JoinLobby(uint64 LobbyId,
                                   TFunction<void(bool, FString)> InCallback)
{
	if (!SteamMatchmaking())
	{
		InCallback(false, {});
		return;
	}

	Callback        = MoveTemp(InCallback);
	PendingLobbyId  = LobbyId;

	SteamAPICall_t hCall = SteamMatchmaking()->JoinLobby(CSteamID(LobbyId));
	JoinCallResult.Set(hCall, this, &FPTSteamDirectJoin::OnLobbyEnter);
}

void FPTSteamDirectJoin::OnLobbyEnter(LobbyEnter_t* pParam, bool bIOFailure)
{
	if (bIOFailure || !pParam || pParam->m_EChatRoomEnterResponse != k_EChatRoomEnterResponseSuccess)
	{
		Callback(false, {});
		return;
	}

	if (!SteamMatchmaking())
	{
		Callback(false, {});
		return;
	}

	CSteamID LobbyId(PendingLobbyId);

	// Leer dirección P2P del host desde los datos de la lobby.
	FString P2PAddr = FString(UTF8_TO_TCHAR(SteamMatchmaking()->GetLobbyData(LobbyId, PT_STEAMKEY_P2PADDR)));
	P2PAddr.RemoveFromStart(TEXT("steam."));

	// Si no hay P2PADDR en los datos de lobby, usar el SteamID del dueño directamente.
	if (P2PAddr.IsEmpty())
	{
		CSteamID OwnerID = SteamMatchmaking()->GetLobbyOwner(LobbyId);
		if (OwnerID.IsValid())
		{
			P2PAddr = FString::Printf(TEXT("%llu"), OwnerID.ConvertToUint64());
		}
	}

	const FString PortStr = FString(UTF8_TO_TCHAR(SteamMatchmaking()->GetLobbyData(LobbyId, PT_STEAMKEY_P2PPORT)));
	const int32   Port    = PortStr.IsEmpty() ? 7777 : FCString::Atoi(*PortStr);

	if (P2PAddr.IsEmpty())
	{
		Callback(false, {});
		return;
	}

	// URL de viaje compatible con SteamSockets: "steam.[Steam64Id]:[Port]"
	const FString ConnectURL = FString::Printf(TEXT("steam.%s:%d"), *P2PAddr, Port);
	Callback(true, ConnectURL);
}

#endif // PT_WITH_STEAM
