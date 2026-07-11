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
    Super::PostLogin(NewPlayer); // el lobby hace RestartPlayer acá → ya hay pawn
    StartPawnFlying(NewPlayer);
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

    StartPawnFlying(C);
    CheckStart();
}

void APTSculptGameMode::StartPawnFlying(AController* C) const
{
    if (APTLobbyCharacter* Char = C ? Cast<APTLobbyCharacter>(C->GetPawn()) : nullptr)
        Char->SetFlyingMode(true);
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
        GetActivePlayers().Num() >= MinPlayersToStart &&
        !bStartScheduled)
    {
        // Arranque DIFERIDO: si venimos de seamless travel desde el lobby, los PlayerState y
        // controllers de todos recién se están asentando. Empezar ya puede elegir escultor sobre
        // un PlayerState que el motor todavía va a reemplazar → a nadie le toca. Esperar StartDelay
        // lo estabiliza. StartGame re-valida al dispararse.
        bStartScheduled = true;
        GetWorldTimerManager().SetTimer(StartDelayTimer, this, &APTSculptGameMode::StartGame,
                                        StartDelay, false);
    }
}

void APTSculptGameMode::StartGame()
{
    bStartScheduled = false;

    APTSculptGameState* G = GS();
    if (!G) return;

    TArray<APTPlayerState*> Players = GetActivePlayers();
    if (Players.Num() < MinPlayersToStart) { GoToWaiting(); return; }

    // Puntajes en cero y rondas desde el principio.
    for (APTPlayerState* PT : Players) PT->GameScore = 0;
    G->CurrentSculptor  = nullptr;       // el primer turno elige escultor al azar
    G->CurrentRound     = 1;
    G->TotalRounds      = NumRounds;
    TurnsLeftThisRound  = Players.Num();  // una ronda = todos esculpen una vez

    StartChoosingPhase();
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

    // Elegir N palabras distintas al azar del pool elegible (categorías + dificultad del host).
    CurrentChoices.Reset();
    TArray<FString> Pool = BuildEligibleWordPool();
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

    RevealedPos.Reset();
    G->MaskedWord        = MakeMasked(CurrentWord);
    G->TurnPhase         = EPTTurnPhase::Drawing;
    G->TurnEndServerTime = G->GetServerWorldTimeSeconds() + TurnDuration;
    G->OnTurnPhaseChanged.Broadcast();

    // Ir revelando letras de a poco hasta ~RevealFraction al final del turno.
    ScheduleLetterReveals();

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

    GetWorldTimerManager().ClearTimer(RevealTimer);
    G->TurnPhase  = EPTTurnPhase::TurnEnd;
    G->MaskedWord = CurrentWord.ToUpper(); // revelar la palabra a todos (en MAYÚSCULA) durante la pausa.
    G->OnTurnPhaseChanged.Broadcast();

    // Anunciar la palabra por el chat (línea de sistema), también en mayúscula.
    G->Multicast_ChatLine(FString(), FString::Printf(TEXT("La palabra era: %s"), *CurrentWord.ToUpper()),
                          EPTChatType::System);
    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Fin de turno. La palabra era '%s'."), *CurrentWord);

    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &APTSculptGameMode::AdvanceTurn,
                                    TurnEndDuration, false);
}

void APTSculptGameMode::AdvanceTurn()
{
    APTSculptGameState* G = GS();
    if (!G) return;

    // Se cerró un turno. ¿Se completó la ronda?
    if (--TurnsLeftThisRound <= 0)
    {
        if (G->CurrentRound >= NumRounds) { EndGame(); return; } // última ronda → fin
        G->CurrentRound   += 1;
        TurnsLeftThisRound = FMath::Max(1, GetActivePlayers().Num());
        G->OnTurnPhaseChanged.Broadcast(); // refrescar "Ronda X/Y" en el HUD
    }
    StartChoosingPhase();
}

void APTSculptGameMode::EndGame()
{
    APTSculptGameState* G = GS();
    if (!G) return;

    GetWorldTimerManager().ClearTimer(PhaseTimer);
    G->TurnPhase         = EPTTurnPhase::GameOver;
    G->CurrentSculptor   = nullptr;
    G->MaskedWord        = FString();
    G->TurnEndServerTime = 0.0;
    G->OnTurnPhaseChanged.Broadcast();

    // Anunciar el ganador (el de mayor puntaje) por el chat.
    APTPlayerState* Winner = nullptr;
    for (APTPlayerState* PT : GetActivePlayers())
        if (!Winner || PT->GameScore > Winner->GameScore) Winner = PT;

    if (Winner)
        G->Multicast_ChatLine(FString(),
            FString::Printf(TEXT("¡Ganó %s con %d puntos!"), *Winner->GetPlayerName(), Winner->GameScore),
            EPTChatType::System);

    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Fin de la partida. Ganador: %s"),
           Winner ? *Winner->GetPlayerName() : TEXT("(nadie)"));
}

void APTSculptGameMode::AwardGuessPoints(APTPlayerState* Guesser)
{
    APTSculptGameState* G = GS();
    if (!G || !Guesser) return;

    // Más rápido = más puntos: interpola de Max (todo el tiempo restante) a Min (sin tiempo).
    const float Frac = (TurnDuration > 0.f)
        ? FMath::Clamp(G->GetTurnSecondsRemaining() / TurnDuration, 0.f, 1.f) : 0.f;
    const int32 GuesserPts = MinGuessPoints + FMath::RoundToInt((MaxGuessPoints - MinGuessPoints) * Frac);
    Guesser->GameScore += GuesserPts;

    // El escultor gana por cada acierto (premia una escultura reconocible).
    if (G->CurrentSculptor)
        G->CurrentSculptor->GameScore += SculptorPointsPerGuess;
}

void APTSculptGameMode::RequestPlayAgain(APTPlayerState* Requester)
{
    APTSculptGameState* G = GS();
    if (!G || G->TurnPhase != EPTTurnPhase::GameOver) return;
    if (!Requester || !Requester->bIsHost) return; // solo el anfitrión decide
    StartGame();
}

void APTSculptGameMode::RequestReturnToLobby(APTPlayerState* Requester)
{
    if (!Requester || !Requester->bIsHost) return; // solo el anfitrión decide
    GetWorldTimerManager().ClearTimer(PhaseTimer);
    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Volviendo al lobby: %s"), *LobbyMapPath);
    GetWorld()->ServerTravel(LobbyMapPath + TEXT("?listen"), /*bAbsolute=*/true);
}

void APTSculptGameMode::HandlePlayerGuessedCorrectly(APTPlayerState* Guesser)
{
    APTSculptGameState* G = GS();
    if (!G || G->TurnPhase != EPTTurnPhase::Drawing) return;
    if (!Guesser || Guesser == G->CurrentSculptor || Guesser->bHasGuessedThisTurn) return;

    Guesser->bHasGuessedThisTurn = true;
    AwardGuessPoints(Guesser); // puntos al que adivina + al escultor

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

void APTSculptGameMode::ScheduleLetterReveals()
{
    GetWorldTimerManager().ClearTimer(RevealTimer);
    RevealQueue.Reset();

    // Posiciones de letras (ignorando espacios).
    TArray<int32> Letters;
    for (int32 i = 0; i < CurrentWord.Len(); ++i)
        if (!FChar::IsWhitespace(CurrentWord[i])) Letters.Add(i);

    const int32 NumToReveal = FMath::FloorToInt(FMath::Clamp(RevealFraction, 0.f, 0.95f) * Letters.Num());
    if (NumToReveal <= 0) return;

    // Barajar y tomar las primeras NumToReveal (orden de revelado al azar).
    for (int32 i = Letters.Num() - 1; i > 0; --i) Letters.Swap(i, FMath::RandRange(0, i));
    for (int32 i = 0; i < NumToReveal; ++i) RevealQueue.Add(Letters[i]);

    // Repartir los revelados a lo largo del turno (el último cae cerca del final).
    const float Interval = TurnDuration / (NumToReveal + 1);
    GetWorldTimerManager().SetTimer(RevealTimer, this, &APTSculptGameMode::RevealNextLetter, Interval, true);
}

void APTSculptGameMode::RevealNextLetter()
{
    APTSculptGameState* G = GS();
    if (!G || G->TurnPhase != EPTTurnPhase::Drawing || RevealQueue.Num() == 0)
    {
        GetWorldTimerManager().ClearTimer(RevealTimer);
        return;
    }
    RevealedPos.Add(RevealQueue[0]);
    RevealQueue.RemoveAt(0);
    G->MaskedWord = BuildMaskedWord(); // solo lo ven los que adivinan (el escultor ve la real)
    if (RevealQueue.Num() == 0) GetWorldTimerManager().ClearTimer(RevealTimer);
}

FString APTSculptGameMode::BuildMaskedWord() const
{
    FString Out;
    for (int32 i = 0; i < CurrentWord.Len(); ++i)
    {
        const TCHAR C = CurrentWord[i];
        if (FChar::IsWhitespace(C))      Out += TEXT("   ");
        else if (RevealedPos.Contains(i)) { Out.AppendChar(FChar::ToUpper(C)); Out += TEXT(" "); } // revelada en MAYÚSCULA
        else                              Out += TEXT("_ ");
    }
    return Out.TrimStartAndEnd();
}

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
            // Minúsculas acentuadas.
            case TCHAR(0x00E1): case TCHAR(0x00E0): case TCHAR(0x00E4): case TCHAR(0x00E2): C = 'a'; break; // á à ä â
            case TCHAR(0x00E9): case TCHAR(0x00E8): case TCHAR(0x00EB): case TCHAR(0x00EA): C = 'e'; break; // é è ë ê
            case TCHAR(0x00ED): case TCHAR(0x00EC): case TCHAR(0x00EF): case TCHAR(0x00EE): C = 'i'; break; // í ì ï î
            case TCHAR(0x00F3): case TCHAR(0x00F2): case TCHAR(0x00F6): case TCHAR(0x00F4): C = 'o'; break; // ó ò ö ô
            case TCHAR(0x00FA): case TCHAR(0x00F9): case TCHAR(0x00FC): case TCHAR(0x00FB): C = 'u'; break; // ú ù ü û
            case TCHAR(0x00F1): C = 'n'; break; // ñ → n
            // MAYÚSCULAS acentuadas: FChar::ToLower solo baja ASCII (A-Z), así que 'Á' NO se
            // convierte a 'á' y sin estas cases se escaparía sin normalizar (bug: "Árbol" no
            // matcheaba). Mapear directo a la base minúscula.
            case TCHAR(0x00C1): case TCHAR(0x00C0): case TCHAR(0x00C4): case TCHAR(0x00C2): C = 'a'; break; // Á À Ä Â
            case TCHAR(0x00C9): case TCHAR(0x00C8): case TCHAR(0x00CB): case TCHAR(0x00CA): C = 'e'; break; // É È Ë Ê
            case TCHAR(0x00CD): case TCHAR(0x00CC): case TCHAR(0x00CF): case TCHAR(0x00CE): C = 'i'; break; // Í Ì Ï Î
            case TCHAR(0x00D3): case TCHAR(0x00D2): case TCHAR(0x00D6): case TCHAR(0x00D4): C = 'o'; break; // Ó Ò Ö Ô
            case TCHAR(0x00DA): case TCHAR(0x00D9): case TCHAR(0x00DC): case TCHAR(0x00DB): C = 'u'; break; // Ú Ù Ü Û
            case TCHAR(0x00D1): C = 'n'; break; // Ñ → n
            default: break;
        }
        Out.AppendChar(C);
    }
    return Out;
}

void APTSculptGameMode::SeedDefaultWords()
{
    // Banco por defecto (100 palabras) categorizadas + dificultad (Fácil/Media/Difícil). El
    // matcheo normaliza mayúsculas/tildes, así que los jugadores pueden escribir sin acentos.
    // Las categorías son FName ASCII (para display se puede mapear con acentos aparte).
    using D = EPTWordDifficulty;
    WordBank = {
        // ── Animales ──
        {TEXT("Pez"),TEXT("Animales"),D::Facil}, {TEXT("Serpiente"),TEXT("Animales"),D::Facil},
        {TEXT("Caracol"),TEXT("Animales"),D::Media}, {TEXT("Tortuga"),TEXT("Animales"),D::Media},
        {TEXT("Pato"),TEXT("Animales"),D::Facil}, {TEXT("Gato"),TEXT("Animales"),D::Facil},
        {TEXT("Perro"),TEXT("Animales"),D::Facil}, {TEXT("Conejo"),TEXT("Animales"),D::Media},
        {TEXT("Elefante"),TEXT("Animales"),D::Media}, {TEXT("Jirafa"),TEXT("Animales"),D::Media},
        {TEXT("Pingüino"),TEXT("Animales"),D::Media}, {TEXT("Oso"),TEXT("Animales"),D::Facil},
        {TEXT("Rana"),TEXT("Animales"),D::Facil}, {TEXT("Pulpo"),TEXT("Animales"),D::Media},
        {TEXT("Estrella de mar"),TEXT("Animales"),D::Dificil}, {TEXT("Cangrejo"),TEXT("Animales"),D::Media},
        {TEXT("Gusano"),TEXT("Animales"),D::Facil}, {TEXT("Vaca"),TEXT("Animales"),D::Facil},
        {TEXT("Cerdo"),TEXT("Animales"),D::Facil}, {TEXT("Caballo"),TEXT("Animales"),D::Media},
        {TEXT("Dinosaurio"),TEXT("Animales"),D::Dificil}, {TEXT("Ballena"),TEXT("Animales"),D::Media},
        {TEXT("Araña"),TEXT("Animales"),D::Media}, {TEXT("Abeja"),TEXT("Animales"),D::Media},
        {TEXT("Mariposa"),TEXT("Animales"),D::Media},
        // ── Comida ──
        {TEXT("Manzana"),TEXT("Comida"),D::Facil}, {TEXT("Banana"),TEXT("Comida"),D::Facil},
        {TEXT("Pera"),TEXT("Comida"),D::Facil}, {TEXT("Zanahoria"),TEXT("Comida"),D::Media},
        {TEXT("Champiñón"),TEXT("Comida"),D::Dificil}, {TEXT("Helado"),TEXT("Comida"),D::Facil},
        {TEXT("Hamburguesa"),TEXT("Comida"),D::Media}, {TEXT("Pizza"),TEXT("Comida"),D::Facil},
        {TEXT("Dona"),TEXT("Comida"),D::Facil}, {TEXT("Huevo"),TEXT("Comida"),D::Facil},
        {TEXT("Galleta"),TEXT("Comida"),D::Media}, {TEXT("Torta"),TEXT("Comida"),D::Media},
        {TEXT("Pan"),TEXT("Comida"),D::Facil}, {TEXT("Queso"),TEXT("Comida"),D::Facil},
        {TEXT("Frutilla"),TEXT("Comida"),D::Media}, {TEXT("Piña"),TEXT("Comida"),D::Media},
        // ── Objetos ──
        {TEXT("Taza"),TEXT("Objetos"),D::Facil}, {TEXT("Silla"),TEXT("Objetos"),D::Facil},
        {TEXT("Mesa"),TEXT("Objetos"),D::Facil}, {TEXT("Lámpara"),TEXT("Objetos"),D::Media},
        {TEXT("Botella"),TEXT("Objetos"),D::Facil}, {TEXT("Vaso"),TEXT("Objetos"),D::Facil},
        {TEXT("Vela"),TEXT("Objetos"),D::Facil}, {TEXT("Balde"),TEXT("Objetos"),D::Media},
        {TEXT("Almohada"),TEXT("Objetos"),D::Media}, {TEXT("Reloj"),TEXT("Objetos"),D::Media},
        {TEXT("Llave"),TEXT("Objetos"),D::Facil}, {TEXT("Paraguas"),TEXT("Objetos"),D::Media},
        {TEXT("Cámara"),TEXT("Objetos"),D::Media}, {TEXT("Teléfono"),TEXT("Objetos"),D::Media},
        {TEXT("Cuchara"),TEXT("Objetos"),D::Facil},
        // ── Naturaleza ──
        {TEXT("Árbol"),TEXT("Naturaleza"),D::Facil}, {TEXT("Flor"),TEXT("Naturaleza"),D::Facil},
        {TEXT("Cactus"),TEXT("Naturaleza"),D::Media}, {TEXT("Montaña"),TEXT("Naturaleza"),D::Facil},
        {TEXT("Nube"),TEXT("Naturaleza"),D::Facil}, {TEXT("Sol"),TEXT("Naturaleza"),D::Facil},
        {TEXT("Luna"),TEXT("Naturaleza"),D::Facil}, {TEXT("Hoja"),TEXT("Naturaleza"),D::Facil},
        {TEXT("Roca"),TEXT("Naturaleza"),D::Facil}, {TEXT("Volcán"),TEXT("Naturaleza"),D::Media},
        {TEXT("Rayo"),TEXT("Naturaleza"),D::Media},
        // ── Vehiculos ──
        {TEXT("Auto"),TEXT("Vehiculos"),D::Facil}, {TEXT("Barco"),TEXT("Vehiculos"),D::Media},
        {TEXT("Avión"),TEXT("Vehiculos"),D::Media}, {TEXT("Cohete"),TEXT("Vehiculos"),D::Media},
        {TEXT("Bicicleta"),TEXT("Vehiculos"),D::Dificil}, {TEXT("Tren"),TEXT("Vehiculos"),D::Media},
        {TEXT("Globo aerostático"),TEXT("Vehiculos"),D::Dificil}, {TEXT("Submarino"),TEXT("Vehiculos"),D::Dificil},
        // ── Cuerpo ──
        {TEXT("Mano"),TEXT("Cuerpo"),D::Media}, {TEXT("Pie"),TEXT("Cuerpo"),D::Facil},
        {TEXT("Nariz"),TEXT("Cuerpo"),D::Media}, {TEXT("Diente"),TEXT("Cuerpo"),D::Media},
        {TEXT("Calavera"),TEXT("Cuerpo"),D::Media}, {TEXT("Corazón"),TEXT("Cuerpo"),D::Facil},
        {TEXT("Cerebro"),TEXT("Cuerpo"),D::Dificil}, {TEXT("Oreja"),TEXT("Cuerpo"),D::Media},
        // ── Ropa ──
        {TEXT("Sombrero"),TEXT("Ropa"),D::Media}, {TEXT("Zapato"),TEXT("Ropa"),D::Facil},
        {TEXT("Bota"),TEXT("Ropa"),D::Facil}, {TEXT("Corona"),TEXT("Ropa"),D::Media},
        {TEXT("Lentes"),TEXT("Ropa"),D::Media}, {TEXT("Corbata"),TEXT("Ropa"),D::Media},
        {TEXT("Mochila"),TEXT("Ropa"),D::Media}, {TEXT("Guante"),TEXT("Ropa"),D::Media},
        // ── Herramientas ──
        {TEXT("Martillo"),TEXT("Herramientas"),D::Media}, {TEXT("Pala"),TEXT("Herramientas"),D::Facil},
        {TEXT("Hacha"),TEXT("Herramientas"),D::Media}, {TEXT("Lápiz"),TEXT("Herramientas"),D::Facil},
        {TEXT("Pincel"),TEXT("Herramientas"),D::Media}, {TEXT("Tijera"),TEXT("Herramientas"),D::Media},
        // ── Fantasia ──
        {TEXT("Fantasma"),TEXT("Fantasia"),D::Facil}, {TEXT("Robot"),TEXT("Fantasia"),D::Media},
        {TEXT("Espada"),TEXT("Fantasia"),D::Facil}
    };
}

TArray<FString> APTSculptGameMode::BuildEligibleWordPool() const
{
    const TArray<FPTWordEntry>& Source =
        (MatchSettings.bUseCustomWords && MatchSettings.CustomWords.Num() > 0)
        ? MatchSettings.CustomWords : WordBank;

    auto Passes = [this](const FPTWordEntry& E)
    {
        const bool bCatOK  = MatchSettings.ActiveCategories.Num() == 0
                          || MatchSettings.ActiveCategories.Contains(E.Category);
        const bool bDiffOK = MatchSettings.ActiveDifficulties.Num() == 0
                          || MatchSettings.ActiveDifficulties.Contains(E.Difficulty);
        return bCatOK && bDiffOK;
    };

    TArray<FString> Pool;
    for (const FPTWordEntry& E : Source)
        if (!E.Word.IsEmpty() && Passes(E)) Pool.Add(E.Word);

    // Fallback: si el filtro no dejó ninguna, usar todas las de la fuente (no dejar sin palabras).
    if (Pool.Num() == 0)
        for (const FPTWordEntry& E : Source)
            if (!E.Word.IsEmpty()) Pool.Add(E.Word);

    return Pool;
}
