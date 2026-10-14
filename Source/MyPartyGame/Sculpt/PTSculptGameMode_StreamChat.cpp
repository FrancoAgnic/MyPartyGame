// CHAT DEL STREAM en el modo CLÁSICO: métodos de APTSculptGameMode en un archivo aparte (como _BuildBattle /
// _Gamepad en el controller). La lectura del chat y la lista de espectadores viven en UPTStreamChatSubsystem;
// acá solo se crean / sacan sus PlayerStates y se mandan sus líneas a la pestaña del stream.

#include "PTSculptGameMode.h"
#include "PTSculptGameState.h"
#include "../Lobby/PTPlayerState.h"
#include "../PTGameInstance.h"
#include "Engine/World.h"

bool APTSculptGameMode::IsStreamChatGame() const
{
    if (IsLocalPartyGame() || IsTutorialGame()) return false; // ahí el chat lo maneja el modo local / no aplica
    const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    return GI && GI->PendingMatchSettings.bStreamChat;
}

void APTSculptGameMode::ProcessServerTravel(const FString& URL, bool bAbsolute)
{
    // Antes de viajar, no durante: si quedaban en el PlayerArray sin viajar, el GameState llegaba al otro mapa con
    // punteros muertos (crash). Super pasa la partida a LeavingMap → el subsistema no los vuelve a crear acá.
    for (APTPlayerState* V : GetStreamViewers()) V->Destroy();
    Super::ProcessServerTravel(URL, bAbsolute);
}

void APTSculptGameMode::StreamChat_SetActive(bool bActive, uint8 Platforms)
{
    if (APTSculptGameState* G = GS())
    {
        G->bStreamChat = bActive;
        G->StreamChatPlatforms = bActive ? Platforms : 0;
    }
}

TArray<APTPlayerState*> APTSculptGameMode::GetStreamViewers() const
{
    TArray<APTPlayerState*> Out;
    if (const APTSculptGameState* G = GS())
        for (APlayerState* PS : G->PlayerArray)
            if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
                if (PT->IsStreamViewer() && IsValid(PT)) Out.Add(PT);
    return Out;
}

APTPlayerState* APTSculptGameMode::StreamChat_AddViewer(const FString& Name, const FString& Language,
                                                        FLinearColor Color, uint8 Platform, bool bAnnounce)
{
    UWorld* W = GetWorld();
    if (!W) return nullptr;

    // PlayerState SIN controller (como los jugadores del celular): entra al PlayerArray, aparece en el marcador y
    // replica a todos. Lo que necesitaría un controller (popups privados) simplemente no le llega.
    FActorSpawnParameters P;
    P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    P.ObjectFlags |= RF_Transient;
    UClass* Cls = PlayerStateClass ? PlayerStateClass.Get() : APTPlayerState::StaticClass();
    APTPlayerState* PS = W->SpawnActor<APTPlayerState>(Cls, P);
    if (!PS) return nullptr;

    PS->SetPlayerId(NextPhonePlayerId++);
    PS->SetPlayerName(Name);
    PS->DisplayName    = Name;
    PS->Language       = Language;
    PS->PartyColor     = Color;
    PS->StreamPlatform = Platform;
    PS->GameScore      = 0;

    if (bAnnounce) PushStreamFeed(PS, TEXT("STREAM_JOINED"), EPTChatType::System, 0);
    UE_LOG(LogTemp, Log, TEXT("[StreamChat] '%s' se unió desde el chat (%s)."), *Name,
        Platform == 2 ? TEXT("Kick") : TEXT("Twitch"));
    return PS;
}

void APTSculptGameMode::StreamChat_RemoveViewer(APTPlayerState* PS, bool bAnnounce)
{
    if (!PS || !PS->IsStreamViewer()) return; // nunca sacar a un jugador real por acá
    if (bAnnounce) PushStreamFeed(PS, TEXT("STREAM_LEFT"), EPTChatType::System, 0);
    PS->Destroy(); // lo saca del PlayerArray (APlayerState::Destroyed)
}

void APTSculptGameMode::StreamChat_Message(APTPlayerState* PS, const FString& Text)
{
    // Como cualquier mensaje de chat: si es la palabra cuenta como acierto, si no se muestra (con la palabra
    // tapada si la nombra). HandleChat lo manda a la pestaña del stream por ser espectador.
    if (PS && PS->IsStreamViewer()) HandleChat(PS, Text);
}

void APTSculptGameMode::PushStreamFeed(const APTPlayerState* PS, const FString& Text, EPTChatType Type, int32 Points)
{
    APTSculptGameState* G = GS();
    if (!G || !PS) return;

    // Tope de líneas normales por segundo (cubeta de fichas): un chat de 200 personas no puede mandar 200
    // multicasts por segundo a cada amigo. Aciertos y avisos pasan siempre.
    if (Type == EPTChatType::Normal)
    {
        const double Now  = GetWorld()->GetRealTimeSeconds();
        const double Rate = FMath::Max(0.5, (double)StreamFeedMaxLinesPerSec);
        StreamFeedTokens = FMath::Min(Rate, StreamFeedTokens + (Now - StreamFeedLastRefill) * Rate);
        StreamFeedLastRefill = Now;
        if (StreamFeedTokens < 1.0) return;
        StreamFeedTokens -= 1.0;
    }
    G->Multicast_StreamChatLine(PS->GetPlayerName(), PS->StreamPlatform, Text, Type, Points);
}
