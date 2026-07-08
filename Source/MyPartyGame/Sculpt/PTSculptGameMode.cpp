#include "PTSculptGameMode.h"
#include "PTSculptGameState.h"
#include "PTSculptPlayerController.h"
#include "PTSculptVolume.h"
#include "../Lobby/PTPlayerState.h"
#include "../Lobby/PTLobbyCharacter.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

APTSculptGameMode::APTSculptGameMode()
{
    // DefaultPawnClass, PlayerStateClass, bUseSeamlessTravel y bDelayedStart + la posesión
    // explícita (RestartPlayer en PostLogin) se heredan de APTLobbyGameMode. Solo cambiamos
    // el PlayerController (con las tools de esculpido) y el GameState (estado de la partida).
    PlayerControllerClass = APTSculptPlayerController::StaticClass();
    GameStateClass        = APTSculptGameState::StaticClass();
}

void APTSculptGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (WordBank.Num() == 0) SeedDefaultWords();
}

APTSculptGameState* APTSculptGameMode::GS() const
{
    return GetGameState<APTSculptGameState>();
}

TArray<APTPlayerState*> APTSculptGameMode::GetActivePlayers() const
{
    TArray<APTPlayerState*> Out;
    if (const APTSculptGameState* G = GS())
    {
        for (APlayerState* PS : G->PlayerArray)
        {
            if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
            {
                if (!PT->IsInactive() && !PT->IsOnlyASpectator())
                    Out.Add(PT);
            }
        }
    }
    return Out;
}

void APTSculptGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    CheckStart();
}

void APTSculptGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
    Super::HandleSeamlessTravelPlayer(C);

    // El seamless travel NO llama PostLogin (donde el lobby hace RestartPlayer), así que
    // el jugador llega sin pawn = espectador. Lo posesionamos a mano acá.
    if (APlayerController* PC = Cast<APlayerController>(C))
        if (!PC->GetPawn())
            RestartPlayer(PC);

    CheckStart();
}

void APTSculptGameMode::Logout(AController* Exiting)
{
    APTSculptGameState* G = GS();
    const bool bWasSculptor =
        (G && Exiting && Exiting->PlayerState && Exiting->PlayerState == G->CurrentSculptor);

    Super::Logout(Exiting);

    if (!G) return;

    if (GetActivePlayers().Num() < MinPlayersToStart)
    {
        GoToWaiting();
    }
    else if (bWasSculptor &&
             (G->TurnPhase == EPTTurnPhase::Drawing || G->TurnPhase == EPTTurnPhase::ChoosingWord))
    {
        // Se fue el que esculpía en pleno turno: cerrar y pasar al siguiente.
        GetWorldTimerManager().ClearTimer(PhaseTimer);
        EndTurn();
    }
}

void APTSculptGameMode::CheckStart()
{
    APTSculptGameState* G = GS();
    if (!G) return;
    if (G->TurnPhase == EPTTurnPhase::WaitingForPlayers &&
        GetActivePlayers().Num() >= MinPlayersToStart)
    {
        StartChoosingPhase();
    }
}

void APTSculptGameMode::GoToWaiting()
{
    APTSculptGameState* G = GS();
    if (!G) return;
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    G->TurnPhase         = EPTTurnPhase::WaitingForPlayers;
    G->CurrentSculptor   = nullptr;
    G->MaskedWord        = FString();
    G->TurnEndServerTime = 0.0;
    G->OnTurnPhaseChanged.Broadcast();
    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Esperando jugadores (%d/%d)."),
           GetActivePlayers().Num(), MinPlayersToStart);
}

void APTSculptGameMode::StartChoosingPhase()
{
    APTSculptGameState* G = GS();
    if (!G) return;

    // Los jugadores de PIE pueden loguearse antes del BeginPlay del GameMode, así que el
    // turno puede arrancar con el banco todavía vacío. Sembrar acá lo garantiza.
    if (WordBank.Num() == 0) SeedDefaultWords();

    TArray<APTPlayerState*> Players = GetActivePlayers();
    if (Players.Num() < MinPlayersToStart) { GoToWaiting(); return; }

    // Resetear "adivinó" de todos al empezar el turno.
    for (APTPlayerState* PT : Players) PT->bHasGuessedThisTurn = false;

    // Lienzo en blanco para el nuevo turno (en todos los clientes).
    ResetSculpture();

    // Elegir el próximo escultor: el siguiente al actual en la lista (rota); si no hay
    // actual (primer turno o se fue), uno al azar.
    int32 NextIdx = FMath::RandRange(0, Players.Num() - 1);
    if (G->CurrentSculptor)
    {
        const int32 PrevIdx = Players.IndexOfByKey(G->CurrentSculptor);
        if (PrevIdx != INDEX_NONE) NextIdx = (PrevIdx + 1) % Players.Num();
    }
    APTPlayerState* Sculptor = Players[NextIdx];

    // Elegir N palabras distintas al azar.
    CurrentChoices.Reset();
    TArray<FString> Pool = WordBank;
    const int32 Want = FMath::Min(WordChoiceCount, Pool.Num());
    for (int32 i = 0; i < Want; ++i)
    {
        const int32 P = FMath::RandRange(0, Pool.Num() - 1);
        CurrentChoices.Add(Pool[P]);
        Pool.RemoveAtSwap(P);
    }

    CurrentWord          = FString();
    G->CurrentSculptor   = Sculptor;
    G->TurnPhase         = EPTTurnPhase::ChoosingWord;
    G->MaskedWord        = FString();
    G->TurnEndServerTime = 0.0;
    G->OnTurnPhaseChanged.Broadcast();

    // Mandarle las opciones SOLO al escultor.
    if (APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(Sculptor->GetOwningController()))
        PC->Client_ReceiveWordChoices(CurrentChoices);

    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Turno: esculpe '%s'. Eligiendo palabra (%d opciones)."),
           *Sculptor->GetPlayerName(), CurrentChoices.Num());

    // Si no elige a tiempo, se elige la primera automáticamente.
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &APTSculptGameMode::AutoChooseWord,
                                    ChooseDuration, false);
}

void APTSculptGameMode::AutoChooseWord()
{
    BeginDrawing(0);
}

void APTSculptGameMode::HandleWordChosen(APTPlayerState* Chooser, int32 ChoiceIndex)
{
    APTSculptGameState* G = GS();
    if (!G || G->TurnPhase != EPTTurnPhase::ChoosingWord) return;
    if (Chooser != G->CurrentSculptor) return; // solo el escultor elige
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    BeginDrawing(ChoiceIndex);
}

void APTSculptGameMode::BeginDrawing(int32 ChoiceIndex)
{
    APTSculptGameState* G = GS();
    if (!G || CurrentChoices.Num() == 0) return;

    ChoiceIndex = FMath::Clamp(ChoiceIndex, 0, CurrentChoices.Num() - 1);
    CurrentWord = CurrentChoices[ChoiceIndex];

    G->MaskedWord        = MakeMasked(CurrentWord);
    G->TurnPhase         = EPTTurnPhase::Drawing;
    G->TurnEndServerTime = G->GetServerWorldTimeSeconds() + TurnDuration;
    G->OnTurnPhaseChanged.Broadcast();

    // El escultor recibe la palabra real (nadie más).
    if (G->CurrentSculptor)
        if (APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(G->CurrentSculptor->GetOwningController()))
            PC->Client_ReceiveSecretWord(CurrentWord);

    // TODO Fase 3: resetear la escultura (limpiar el Volume) acá.

    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Esculpiendo '%s' por %.0fs."), *CurrentWord, TurnDuration);

    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &APTSculptGameMode::EndTurn,
                                    TurnDuration, false);
}

void APTSculptGameMode::EndTurn()
{
    APTSculptGameState* G = GS();
    if (!G) return;

    G->TurnPhase  = EPTTurnPhase::TurnEnd;
    G->MaskedWord = CurrentWord; // revelar la palabra a todos durante la pausa.
    G->OnTurnPhaseChanged.Broadcast();

    // Anunciar la palabra por el chat (línea de sistema).
    G->Multicast_ChatLine(FString(), FString::Printf(TEXT("La palabra era: %s"), *CurrentWord),
                          EPTChatType::System);
    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Fin de turno. La palabra era '%s'."), *CurrentWord);

    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &APTSculptGameMode::StartChoosingPhase,
                                    TurnEndDuration, false);
}

void APTSculptGameMode::HandlePlayerGuessedCorrectly(APTPlayerState* Guesser)
{
    APTSculptGameState* G = GS();
    if (!G || G->TurnPhase != EPTTurnPhase::Drawing) return;
    if (!Guesser || Guesser == G->CurrentSculptor || Guesser->bHasGuessedThisTurn) return;

    Guesser->bHasGuessedThisTurn = true;

    // ¿Adivinaron todos los que no esculpen? → cerrar el turno antes de tiempo.
    bool bAllGuessed = true;
    for (APTPlayerState* PT : GetActivePlayers())
    {
        if (PT == G->CurrentSculptor) continue;
        if (!PT->bHasGuessedThisTurn) { bAllGuessed = false; break; }
    }
    if (bAllGuessed)
    {
        GetWorldTimerManager().ClearTimer(PhaseTimer);
        EndTurn();
    }
}

void APTSculptGameMode::HandleChat(APTPlayerState* Sender, const FString& Message)
{
    if (!Sender) return;
    const FString Text = Message.TrimStartAndEnd().Left(200);
    if (Text.IsEmpty()) return;

    APTSculptGameState* G = GS();
    const FString Name = Sender->GetPlayerName();

    // Durante el dibujo: detectar aciertos y bloquear que la palabra aparezca en el chat.
    if (G && G->TurnPhase == EPTTurnPhase::Drawing && !CurrentWord.IsEmpty())
    {
        const bool bEligibleGuesser = (Sender != G->CurrentSculptor && !Sender->bHasGuessedThisTurn);

        if (bEligibleGuesser && DoesGuessMatch(Text))
        {
            // Acierto: anunciar SIN el texto, luego marcar (puede cerrar el turno).
            G->Multicast_ChatLine(Name, FString(), EPTChatType::Correct);
            // Globo VERDE "adivinó la palabra" (nunca la palabra) + confetti sobre su cabeza.
            if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(Sender->GetPawn()))
                Char->Multicast_ShowChatBubble(TEXT("¡Adivinó la palabra!"), true);
            HandlePlayerGuessedCorrectly(Sender);
            return;
        }

        // Anti-spoiler: nadie (ni el escultor, ni un adivinador con la palabra en una
        // frase, ni quien ya adivinó) puede hacer aparecer la palabra en el chat.
        if (Normalize(Text).Contains(Normalize(CurrentWord)))
            return; // se descarta silenciosamente
    }

    // Mensaje normal → a todos.
    if (G) G->Multicast_ChatLine(Name, Text, EPTChatType::Normal);

    // Globo de chat sobre la cabeza del que escribió (mensaje normal, color default).
    if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(Sender->GetPawn()))
        Char->Multicast_ShowChatBubble(Text, false);
}

bool APTSculptGameMode::DoesGuessMatch(const FString& Guess) const
{
    if (CurrentWord.IsEmpty()) return false;
    return Normalize(Guess) == Normalize(CurrentWord);
}

void APTSculptGameMode::ResetSculpture()
{
    if (APTSculptVolume* Vol = Cast<APTSculptVolume>(
            UGameplayStatics::GetActorOfClass(GetWorld(), APTSculptVolume::StaticClass())))
    {
        Vol->Multicast_ClearAll();
    }
}

// ── Helpers ─────────────────────────────────────────────────────────────────

FString APTSculptGameMode::MakeMasked(const FString& Word)
{
    FString Out;
    for (const TCHAR C : Word)
    {
        if (FChar::IsWhitespace(C)) Out += TEXT("   ");
        else                        Out += TEXT("_ ");
    }
    return Out.TrimStartAndEnd();
}

FString APTSculptGameMode::Normalize(const FString& In)
{
    FString S = In.TrimStartAndEnd().ToLower();
    FString Out;
    Out.Reserve(S.Len());
    for (TCHAR C : S)
    {
        switch (C)
        {
            case TCHAR(0x00E1): case TCHAR(0x00E0): case TCHAR(0x00E4): case TCHAR(0x00E2): C = 'a'; break; // á à ä â
            case TCHAR(0x00E9): case TCHAR(0x00E8): case TCHAR(0x00EB): case TCHAR(0x00EA): C = 'e'; break; // é è ë ê
            case TCHAR(0x00ED): case TCHAR(0x00EC): case TCHAR(0x00EF): case TCHAR(0x00EE): C = 'i'; break; // í ì ï î
            case TCHAR(0x00F3): case TCHAR(0x00F2): case TCHAR(0x00F6): case TCHAR(0x00F4): C = 'o'; break; // ó ò ö ô
            case TCHAR(0x00FA): case TCHAR(0x00F9): case TCHAR(0x00FC): case TCHAR(0x00FB): C = 'u'; break; // ú ù ü û
            case TCHAR(0x00F1): C = 'n'; break; // ñ → n (para ser tolerante al escribir)
            default: break;
        }
        Out.AppendChar(C);
    }
    return Out;
}

void APTSculptGameMode::SeedDefaultWords()
{
    // Banco por defecto (100 palabras, de WordBank_ES.csv). El matcheo normaliza
    // mayúsculas/tildes, así que los jugadores pueden escribir sin acentos.
    WordBank = {
        TEXT("Pez"), TEXT("Serpiente"), TEXT("Caracol"), TEXT("Tortuga"), TEXT("Pato"),
        TEXT("Gato"), TEXT("Perro"), TEXT("Conejo"), TEXT("Elefante"), TEXT("Jirafa"),
        TEXT("Pingüino"), TEXT("Oso"), TEXT("Rana"), TEXT("Pulpo"), TEXT("Estrella de mar"),
        TEXT("Cangrejo"), TEXT("Gusano"), TEXT("Vaca"), TEXT("Cerdo"), TEXT("Caballo"),
        TEXT("Dinosaurio"), TEXT("Ballena"), TEXT("Araña"), TEXT("Abeja"), TEXT("Mariposa"),
        TEXT("Manzana"), TEXT("Banana"), TEXT("Pera"), TEXT("Zanahoria"), TEXT("Champiñón"),
        TEXT("Helado"), TEXT("Hamburguesa"), TEXT("Pizza"), TEXT("Dona"), TEXT("Huevo"),
        TEXT("Galleta"), TEXT("Torta"), TEXT("Pan"), TEXT("Queso"), TEXT("Frutilla"),
        TEXT("Taza"), TEXT("Silla"), TEXT("Mesa"), TEXT("Lámpara"), TEXT("Botella"),
        TEXT("Vaso"), TEXT("Vela"), TEXT("Balde"), TEXT("Almohada"), TEXT("Reloj"),
        TEXT("Llave"), TEXT("Paraguas"), TEXT("Cámara"), TEXT("Teléfono"), TEXT("Cuchara"),
        TEXT("Árbol"), TEXT("Flor"), TEXT("Cactus"), TEXT("Montaña"), TEXT("Nube"),
        TEXT("Sol"), TEXT("Luna"), TEXT("Hoja"), TEXT("Piña"), TEXT("Roca"),
        TEXT("Volcán"), TEXT("Rayo"), TEXT("Auto"), TEXT("Barco"), TEXT("Avión"),
        TEXT("Cohete"), TEXT("Bicicleta"), TEXT("Tren"), TEXT("Globo aerostático"), TEXT("Submarino"),
        TEXT("Mano"), TEXT("Pie"), TEXT("Nariz"), TEXT("Diente"), TEXT("Calavera"),
        TEXT("Corazón"), TEXT("Cerebro"), TEXT("Oreja"), TEXT("Sombrero"), TEXT("Zapato"),
        TEXT("Bota"), TEXT("Corona"), TEXT("Lentes"), TEXT("Corbata"), TEXT("Mochila"),
        TEXT("Guante"), TEXT("Martillo"), TEXT("Pala"), TEXT("Hacha"), TEXT("Lápiz"),
        TEXT("Pincel"), TEXT("Tijera"), TEXT("Fantasma"), TEXT("Robot"), TEXT("Espada")
    };
}
