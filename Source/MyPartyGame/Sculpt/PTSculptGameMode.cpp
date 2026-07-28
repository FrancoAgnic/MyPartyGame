#include "PTSculptGameMode.h"
#include "PTSculptGameState.h"
#include "PTSculptPlayerController.h"
#include "PTSculptVolume.h"
#include "../Lobby/PTPlayerState.h"
#include "../Lobby/PTLobbyCharacter.h"
#include "../PTGameInstance.h"
#include "../PTWordBank.h"
#include "../PTTextTable.h"
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
        Char->ApplyGameplayMovementMode(); // vuelo obligatorio + sin orientar al movimiento
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

void APTSculptGameMode::ApplyMatchSettingsFromGameInstance()
{
    UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    if (!GI) return;

    MatchSettings = GI->PendingMatchSettings; // categorías/dificultad/CSV para BuildEligibleWordPool

    // Tuneables numéricos con clamps de seguridad (el host podría mandar cualquier cosa).
    TurnDuration   = FMath::Clamp(MatchSettings.TurnDuration, 15.f, 600.f);
    NumRounds      = FMath::Clamp(MatchSettings.NumRounds, 1, 20);
    RevealFraction = FMath::Clamp(MatchSettings.RevealFraction, 0.f, 0.95f);

    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Config: %.0fs/turno, %d rondas, revelado %.0f%%, cats=%d, difs=%d, customWords=%d"),
        TurnDuration, NumRounds, RevealFraction * 100.f,
        MatchSettings.ActiveCategories.Num(), MatchSettings.ActiveDifficulties.Num(),
        MatchSettings.bUseCustomWords ? MatchSettings.CustomWords.Num() : 0);
}

void APTSculptGameMode::StartGame()
{
    bStartScheduled = false;

    APTSculptGameState* G = GS();
    if (!G) return;

    // Tomar la config que armó el host en el lobby (viajó en el GameInstance).
    ApplyMatchSettingsFromGameInstance();

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
    G->MaskedWords.Reset();
    G->RefreshLocalMasked();
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
    TArray<FPTWordEntry> Pool = BuildEligibleWordPool();
    const int32 Want = FMath::Min(WordChoiceCount, Pool.Num());
    for (int32 i = 0; i < Want; ++i)
    {
        const int32 P = FMath::RandRange(0, Pool.Num() - 1);
        CurrentChoices.Add(Pool[P]);
        Pool.RemoveAtSwap(P);
    }

    CurrentWord          = FPTWordEntry();
    G->CurrentSculptor   = Sculptor;
    G->TurnPhase         = EPTTurnPhase::ChoosingWord;
    G->MaskedWords.Reset();
    G->RefreshLocalMasked();
    G->TurnEndServerTime = 0.0;
    G->OnTurnPhaseChanged.Broadcast();

    // Mandarle las opciones SOLO al escultor, EN SU IDIOMA (fallback si falta la traducción).
    const int32 SculptorLang = Sculptor->GetLanguageIndex();
    TArray<FString> ChoiceTexts;
    for (const FPTWordEntry& E : CurrentChoices) ChoiceTexts.Add(E.ForLang(SculptorLang));
    if (APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(Sculptor->GetOwningController()))
        PC->Client_ReceiveWordChoices(ChoiceTexts);

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
    RevealedPos.SetNum(FMath::Max(1, PTText::GetAvailableLanguages().Num()));
    PushMaskedWords(); // máscaras iniciales de todos los idiomas (todo "_")
    G->TurnPhase         = EPTTurnPhase::Drawing;
    G->TurnEndServerTime = G->GetServerWorldTimeSeconds() + TurnDuration;
    G->OnTurnPhaseChanged.Broadcast();

    // Ir revelando letras de a poco hasta ~RevealFraction al final del turno (en ambos idiomas).
    ScheduleLetterReveals();

    // El escultor recibe la palabra real EN SU IDIOMA (nadie más).
    if (G->CurrentSculptor)
        if (APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(G->CurrentSculptor->GetOwningController()))
            PC->Client_ReceiveSecretWord(CurrentWord.ForLang(G->CurrentSculptor->GetLanguageIndex()));

    // TODO Fase 3: resetear la escultura (limpiar el Volume) acá.

    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Esculpiendo '%s' por %.0fs."), *CurrentWord.Primary(), TurnDuration);

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
    // Revelar la palabra COMPLETA a todos en su idioma (cada uno ve la suya en MAYÚSCULA).
    const int32 NumLangs = FMath::Max(1, PTText::GetAvailableLanguages().Num());
    G->MaskedWords.SetNum(NumLangs);
    for (int32 L = 0; L < NumLangs; ++L) G->MaskedWords[L] = CurrentWord.ForLang(L).ToUpper();
    G->RefreshLocalMasked();
    G->OnTurnPhaseChanged.Broadcast();

    // Anunciar por el chat todas las traducciones DISTINTAS ("AUTO / CAR / CARRO"), sin repetir:
    // muchas palabras se escriben igual en varios idiomas y quedaría "PIZZA / PIZZA / PIZZA".
    TArray<FString> Seen;
    for (int32 L = 0; L < NumLangs; ++L)
    {
        const FString W = CurrentWord.ForLang(L).ToUpper();
        if (!W.IsEmpty() && !Seen.Contains(W)) Seen.Add(W);
    }
    G->Multicast_SystemLine(TEXT("CHAT_WORD_WAS"), FString::Join(Seen, TEXT(" / ")), 0);
    UE_LOG(LogTemp, Log, TEXT("[SculptGM] Fin de turno. La palabra era '%s'."), *CurrentWord.Primary());

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
    G->MaskedWords.Reset();
    G->RefreshLocalMasked();
    G->TurnEndServerTime = 0.0;
    G->OnTurnPhaseChanged.Broadcast();

    // Anunciar el ganador (el de mayor puntaje) por el chat.
    APTPlayerState* Winner = nullptr;
    for (APTPlayerState* PT : GetActivePlayers())
        if (!Winner || PT->GameScore > Winner->GameScore) Winner = PT;

    if (Winner)
        G->Multicast_SystemLine(TEXT("CHAT_WINNER"), Winner->GetPlayerName(), Winner->GameScore);

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
    if (G && G->TurnPhase == EPTTurnPhase::Drawing && CurrentWord.IsValidEntry())
    {
        const bool bEligibleGuesser = (Sender != G->CurrentSculptor && !Sender->bHasGuessedThisTurn);

        if (bEligibleGuesser && DoesGuessMatch(Text))
        {
            // Acierto: anunciar SIN el texto, luego marcar (puede cerrar el turno).
            G->Multicast_ChatLine(Name, FString(), EPTChatType::Correct);
            // Globo VERDE "adivinó la palabra" (nunca la palabra) + confetti sobre su cabeza.
            if (APTLobbyCharacter* Char = Cast<APTLobbyCharacter>(Sender->GetPawn()))
                // Texto vacío a propósito: con bGuess=true cada cliente pone el suyo traducido.
                Char->Multicast_ShowChatBubble(FString(), true);
            HandlePlayerGuessedCorrectly(Sender);
            return;
        }

        // Anti-spoiler: no dejar que NINGUNA traducción aparezca en el chat, en ningún idioma
        // (si no, un jugador podría spoilear a los demás escribiendo la palabra en otro idioma).
        const FString NT = Normalize(Text);
        for (const FString& W : CurrentWord.Words)
            if (!W.IsEmpty() && NT.Contains(Normalize(W)))
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
    if (!CurrentWord.IsValidEntry()) return false;
    // Cuenta como acierto si coincide con CUALQUIER traducción: escribís "Auto", "Car" o "Carro".
    const FString G = Normalize(Guess);
    for (const FString& W : CurrentWord.Words)
        if (!W.IsEmpty() && G == Normalize(W)) return true;
    return false;
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

void APTSculptGameMode::PushMaskedWords()
{
    APTSculptGameState* G = GS();
    if (!G) return;
    const int32 NumLangs = FMath::Max(1, PTText::GetAvailableLanguages().Num());
    G->MaskedWords.SetNum(NumLangs);
    for (int32 L = 0; L < NumLangs; ++L)
        G->MaskedWords[L] = MaskWord(CurrentWord.ForLang(L), RevealedPos.IsValidIndex(L) ? &RevealedPos[L] : nullptr);
    G->RefreshLocalMasked(); // el host es servidor: no recibe OnRep, refresca su máscara local acá
}

void APTSculptGameMode::ScheduleLetterReveals()
{
    GetWorldTimerManager().ClearTimer(RevealTimer);

    // Una cola por idioma: cada traducción tiene largo y letras distintas, así que se programan
    // por separado y todas avanzan a la par (un tick revela una letra de cada una).
    const int32 NumLangs = FMath::Max(1, PTText::GetAvailableLanguages().Num());
    RevealQueues.Reset();
    RevealQueues.SetNum(NumLangs);

    int32 MaxN = 0;
    for (int32 L = 0; L < NumLangs; ++L)
    {
        ScheduleRevealsFor(CurrentWord.ForLang(L), RevealFraction, RevealQueues[L]);
        MaxN = FMath::Max(MaxN, RevealQueues[L].Num());
    }
    if (MaxN <= 0) return;

    // Un solo timer revela la próxima letra de CADA idioma por tick → ambos avanzan a la par.
    const float Interval = TurnDuration / (MaxN + 1);
    GetWorldTimerManager().SetTimer(RevealTimer, this, &APTSculptGameMode::RevealNextLetter, Interval, true);
}

void APTSculptGameMode::RevealNextLetter()
{
    auto AnyPending = [this]()
    {
        for (const TArray<int32>& Q : RevealQueues) if (Q.Num() > 0) return true;
        return false;
    };

    APTSculptGameState* G = GS();
    if (!G || G->TurnPhase != EPTTurnPhase::Drawing || !AnyPending())
    {
        GetWorldTimerManager().ClearTimer(RevealTimer);
        return;
    }

    // Una letra de CADA idioma por tick: así todos los jugadores ven avanzar su palabra al mismo ritmo.
    for (int32 L = 0; L < RevealQueues.Num(); ++L)
    {
        if (RevealQueues[L].Num() == 0) continue;
        if (RevealedPos.IsValidIndex(L)) RevealedPos[L].Add(RevealQueues[L][0]);
        RevealQueues[L].RemoveAt(0);
    }
    PushMaskedWords();
    if (!AnyPending()) GetWorldTimerManager().ClearTimer(RevealTimer);
}

void APTSculptGameMode::ScheduleRevealsFor(const FString& Word, float Fraction, TArray<int32>& OutQueue)
{
    OutQueue.Reset();
    TArray<int32> Letters;
    for (int32 i = 0; i < Word.Len(); ++i)
        if (!FChar::IsWhitespace(Word[i])) Letters.Add(i);

    const int32 NumToReveal = FMath::FloorToInt(FMath::Clamp(Fraction, 0.f, 0.95f) * Letters.Num());
    if (NumToReveal <= 0) return;

    for (int32 i = Letters.Num() - 1; i > 0; --i) Letters.Swap(i, FMath::RandRange(0, i)); // barajar
    for (int32 i = 0; i < NumToReveal; ++i) OutQueue.Add(Letters[i]);
}

FString APTSculptGameMode::MaskWord(const FString& Word, const TSet<int32>* Revealed)
{
    FString Out;
    for (int32 i = 0; i < Word.Len(); ++i)
    {
        const TCHAR C = Word[i];
        if (FChar::IsWhitespace(C))                          Out += TEXT("   ");
        else if (Revealed && Revealed->Contains(i)) { Out.AppendChar(FChar::ToUpper(C)); Out += TEXT(" "); }
        else                                                 Out += TEXT("_ ");
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
    // Banco por defecto = Content/Localization/WordBank.csv. NO hardcodeado (ver PTWordBank).
    WordBank = PTWordBank::GetDefaultWords();
}

TArray<FPTWordEntry> APTSculptGameMode::BuildEligibleWordPool() const
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

    TArray<FPTWordEntry> Pool; // se lleva la entry entera (con las 2 traducciones)
    for (const FPTWordEntry& E : Source)
        if (E.IsValidEntry() && Passes(E)) Pool.Add(E);

    // Fallback: si el filtro no dejó ninguna, usar todas las de la fuente (no dejar sin palabras).
    if (Pool.Num() == 0)
        for (const FPTWordEntry& E : Source)
            if (E.IsValidEntry()) Pool.Add(E);

    return Pool;
}
