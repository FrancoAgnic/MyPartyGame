// BUILD BATTLE (prototipo): todos esculpen la MISMA palabra a la vez y después se vota cada escultura.
// Son métodos de APTSculptGameMode en un archivo aparte (como PTSculptPlayerController_Gamepad.cpp), para
// no mezclarlos con el loop clásico por turnos.
//
// Una ronda (todo en el server):
//   StartBuildRound      → palabra al azar para todos + un LIENZO por jugador (copias del cubo del nivel,
//                          apiladas en el mismo lugar: cada máquina ve solo el suyo) + fase Drawing con el
//                          reloj de TurnDuration. El esculpido viaja por los mismos RPC/multicast de siempre.
//   EndBuildPhase        → orden de exhibición al azar (anónima).
//   ShowCurrentSculpture → fase Showcase: todos ven la misma escultura y la votan 1..5 (teclas). Nadie ve los
//                          votos; el autor no puede votarse. Si ya votaron todos, se acorta.
//   RevealRoundResults   → fase RoundResults: promedio por escultura → puntos → tabla replicada. La ganadora
//                          queda en exhibición.
//   AdvanceBuildRound    → próxima ronda, o EndGame (la pantalla de fin es la de siempre).

#include "PTSculptGameMode.h"
#include "PTSculptGameState.h"
#include "PTSculptPlayerController.h"
#include "PTSculptVolume.h"
#include "../Lobby/PTPlayerState.h"
#include "../PTGameInstance.h"
#include "../PTTextTable.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Engine/World.h"

bool APTSculptGameMode::IsBuildBattle() const
{
    if (IsLocalPartyGame() || IsTutorialGame()) return false; // una sola PC / tutorial: siempre clásico
    const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    return GI && GI->PendingMatchSettings.GameType == EPTGameType::BuildBattle;
}

EPTGameType APTSculptGameMode::ToggleGameType()
{
    UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    if (!GI) return EPTGameType::Classic;
    EPTGameType& Type = GI->PendingMatchSettings.GameType;
    Type = (Type == EPTGameType::BuildBattle) ? EPTGameType::Classic : EPTGameType::BuildBattle;

    // Sin partida en curso el cambio se ve ya (el HUD anuncia el modo); si no, aplica en la próxima.
    if (APTSculptGameState* G = GS())
        if (G->TurnPhase == EPTTurnPhase::WaitingForPlayers || G->TurnPhase == EPTTurnPhase::GameOver)
            G->bBuildBattle = IsBuildBattle();
    return Type;
}

APTSculptVolume* APTSculptGameMode::GetBuildCanvasFor(const APTPlayerState* PS) const
{
    if (!PS) return nullptr;
    for (const FBuildEntry& E : BuildEntries)
        if (E.Author.Get() == PS) return E.Canvas.Get();
    return nullptr;
}

// ── Lienzos ──────────────────────────────────────────────────────────────────

APTSculptVolume* APTSculptGameMode::SpawnBuildCanvas(APTPlayerState* PS)
{
    UWorld* W = GetWorld();
    APTSculptVolume* Base = APTSculptVolume::FindLevelCanvas(W);
    if (!W || !Base || !PS) return nullptr;

    // Mismo lugar que el cubo del nivel, en reposo (la animación de turno escala el actor entero → 1 = reposo).
    FTransform T = Base->GetActorTransform();
    T.SetScale3D(FVector::OneVector);

    // Dueño = su controller. El Owner se replica pero solo resuelve en la máquina de ESE jugador: cada uno
    // encuentra su lienzo y nadie puede saber de quién es cada escultura (exhibición anónima). Bots: sin dueño.
    APTSculptVolume* V = W->SpawnActorDeferred<APTSculptVolume>(Base->GetClass(), T, PS->GetOwningController(),
        nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!V) return nullptr;
    V->MarkAsBuildBattleCanvas();
    UGameplayStatics::FinishSpawningActor(V, T);
    return V;
}

void APTSculptGameMode::DestroyBuildCanvases()
{
    BuildEntries.Reset();
    ShowcaseOrder.Reset();
    ShowcasePos = 0;

    // Todos los lienzos (no solo los de BuildEntries): ninguno sobrevive a la ronda.
    if (UWorld* W = GetWorld())
    {
        TArray<APTSculptVolume*> ToDestroy;
        for (TActorIterator<APTSculptVolume> It(W); It; ++It)
            if (It->IsBuildBattleCanvas()) ToDestroy.Add(*It);
        for (APTSculptVolume* V : ToDestroy) V->Destroy();
    }

    if (APTSculptGameState* G = GS())
    {
        G->BBShowcaseVolume = nullptr;
        G->BBShowcaseIndex  = 0;
        G->BBShowcaseCount  = 0;
        G->BBRoundResults.Reset();
    }
}

// ── Ronda ────────────────────────────────────────────────────────────────────

void APTSculptGameMode::StartBuildRound()
{
    APTSculptGameState* G = GS();
    if (!G) return;
    if (WordBank.Num() == 0) SeedDefaultWords();

    TArray<APTPlayerState*> Players = GetActivePlayers();
    if (Players.Num() < MinToStart()) { GoToWaiting(); return; }

    DestroyBuildCanvases(); // los de la ronda anterior

    // Una palabra al azar para TODOS (no se repite en la partida, igual que en el clásico).
    TArray<FPTWordEntry> Pool = BuildEligibleWordPool();
    if (Pool.Num() == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("[BuildBattle] No hay palabras: vuelvo a la espera."));
        GoToWaiting();
        return;
    }
    CurrentWord = Pool[FMath::RandRange(0, Pool.Num() - 1)];
    UsedWordKeys.Add(CurrentWord.Primary());

    // Un lienzo por jugador.
    for (APTPlayerState* PT : Players)
    {
        PT->bHasGuessedThisTurn = false;
        FBuildEntry& E = BuildEntries.AddDefaulted_GetRef();
        E.Author     = PT;
        E.AuthorName = PT->GetDisplayNameSafe();
        E.Canvas     = SpawnBuildCanvas(PT);
    }

    // La palabra NO es secreta: va completa (en cada idioma) en las máscaras → el HUD la muestra arriba.
    const int32 NumLangs = FMath::Max(1, PTText::GetAvailableLanguages().Num());
    G->MaskedWords.SetNum(NumLangs);
    for (int32 L = 0; L < NumLangs; ++L) G->MaskedWords[L] = CurrentWord.ForLang(L).ToUpper();
    G->RefreshLocalMasked();

    G->CurrentSculptor      = nullptr; // esculpen todos
    G->bTurnEndedAllGuessed = false;
    G->TurnPhase            = EPTTurnPhase::Drawing;
    G->TurnEndServerTime    = G->GetServerWorldTimeSeconds() + TurnDuration;
    G->OnTurnPhaseChanged.Broadcast();

    // Aviso por chat, a cada uno con la palabra en SU idioma.
    for (APTPlayerState* PT : Players)
        if (APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(PT->GetOwningController()))
            PC->Client_SystemLine(TEXT("BB_CHAT_ROUND_START"), CurrentWord.ForLang(PT->GetLanguageIndex()).ToUpper(), 0);

    UE_LOG(LogTemp, Log, TEXT("[BuildBattle] Ronda %d/%d: todos esculpen '%s' (%d lienzos, %.0fs)."),
        G->CurrentRound, G->TotalRounds, *CurrentWord.Primary(), BuildEntries.Num(), TurnDuration);

    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &APTSculptGameMode::EndBuildPhase, TurnDuration, false);

    BuildBotCursor.Reset();
    GetWorldTimerManager().ClearTimer(BuildBotTimer);
    if (BuildBots.Num() > 0)
        GetWorldTimerManager().SetTimer(BuildBotTimer, this, &APTSculptGameMode::BuildBotsSculptTick,
                                        FMath::Max(0.05f, BuildBotStampInterval), true, 0.5f);
}

void APTSculptGameMode::EndBuildPhase()
{
    GetWorldTimerManager().ClearTimer(BuildBotTimer);

    ShowcaseOrder.Reset();
    for (int32 i = 0; i < BuildEntries.Num(); ++i)
        if (BuildEntries[i].Canvas.IsValid()) ShowcaseOrder.Add(i);
    // Orden al azar (Fisher-Yates): que el orden no delate de quién es cada una.
    for (int32 i = ShowcaseOrder.Num() - 1; i > 0; --i)
        ShowcaseOrder.Swap(i, FMath::RandRange(0, i));
    ShowcasePos = 0;

    if (ShowcaseOrder.Num() == 0) { AdvanceBuildRound(); return; }
    if (APTSculptGameState* G = GS()) G->Multicast_SystemLine(TEXT("BB_CHAT_VOTE_START"), FString(), 0);
    ShowCurrentSculpture();
}

void APTSculptGameMode::ShowCurrentSculpture()
{
    APTSculptGameState* G = GS();
    if (!G) return;
    if (!ShowcaseOrder.IsValidIndex(ShowcasePos)) { RevealRoundResults(); return; }

    FBuildEntry& E = BuildEntries[ShowcaseOrder[ShowcasePos]];
    if (!E.Canvas.IsValid()) { AdvanceShowcase(); return; } // el lienzo ya no existe: saltear

    // Bots: votan al toque (al azar, nunca la propia).
    for (const TWeakObjectPtr<APTPlayerState>& B : BuildBots)
        if (APTPlayerState* Bot = B.Get())
            if (Bot != E.Author.Get()) E.Votes.Add(Bot, FMath::RandRange(2, 5));

    G->BBShowcaseVolume  = E.Canvas.Get();
    G->BBShowcaseIndex   = ShowcasePos + 1;
    G->BBShowcaseCount   = ShowcaseOrder.Num();
    G->TurnPhase         = EPTTurnPhase::Showcase;
    G->TurnEndServerTime = G->GetServerWorldTimeSeconds() + ShowcaseDuration;
    G->OnTurnPhaseChanged.Broadcast();
    ShowcaseStartTime    = G->GetServerWorldTimeSeconds();

    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &APTSculptGameMode::AdvanceShowcase, ShowcaseDuration, false);
    MaybeShortenShowcase(); // jugando solo (o solo con bots) puede que ya no falte nadie
}

void APTSculptGameMode::AdvanceShowcase()
{
    ++ShowcasePos;
    ShowCurrentSculpture();
}

int32 APTSculptGameMode::CountPendingVoters(const FBuildEntry& E) const
{
    int32 Pending = 0;
    for (APTPlayerState* PT : GetActivePlayers())
        if (PT != E.Author.Get() && !E.Votes.Contains(PT)) ++Pending;
    return Pending;
}

void APTSculptGameMode::MaybeShortenShowcase()
{
    APTSculptGameState* G = GS();
    if (!G || G->TurnPhase != EPTTurnPhase::Showcase || !ShowcaseOrder.IsValidIndex(ShowcasePos)) return;
    if (CountPendingVoters(BuildEntries[ShowcaseOrder[ShowcasePos]]) > 0) return;

    const double Now       = G->GetServerWorldTimeSeconds();
    const double Remaining = G->TurnEndServerTime - Now;
    const double Target    = FMath::Max((double)ShowcaseAllVotedGrace, ShowcaseStartTime + ShowcaseMinDuration - Now);
    if (Target >= Remaining) return;

    G->TurnEndServerTime = Now + Target;
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &APTSculptGameMode::AdvanceShowcase,
                                    FMath::Max(0.05f, (float)Target), false);
}

void APTSculptGameMode::HandleBuildBattleVote(APTPlayerState* Voter, int32 Stars)
{
    const APTSculptGameState* G = GS();
    if (!G || !G->IsBuildBattle() || G->TurnPhase != EPTTurnPhase::Showcase || !Voter) return;
    if (!ShowcaseOrder.IsValidIndex(ShowcasePos)) return;

    FBuildEntry& E = BuildEntries[ShowcaseOrder[ShowcasePos]];
    if (E.Author.Get() == Voter) return;              // la propia no se vota
    if (!GetActivePlayers().Contains(Voter)) return;  // espectadores no votan
    E.Votes.Add(Voter, FMath::Clamp(Stars, 1, 5));    // pisa el anterior: se puede cambiar el voto
    MaybeShortenShowcase();
}

void APTSculptGameMode::RevealRoundResults()
{
    APTSculptGameState* G = GS();
    if (!G) return;

    // Promedio de cada escultura → puntos (5 de promedio = BuildBattleMaxPoints).
    TArray<TPair<FPTBuildBattleResult, APTSculptVolume*>> Rows;
    for (const FBuildEntry& E : BuildEntries)
    {
        FPTBuildBattleResult R;
        R.Player = E.Author.Get();
        R.Name   = R.Player ? R.Player->GetDisplayNameSafe() : E.AuthorName;
        int32 Sum = 0;
        for (const TPair<TWeakObjectPtr<APTPlayerState>, int32>& V : E.Votes) Sum += V.Value;
        R.NumVotes = E.Votes.Num();
        R.AvgStars = (R.NumVotes > 0) ? (float)Sum / (float)R.NumVotes : 0.f;
        R.Points   = FMath::RoundToInt(R.AvgStars / 5.f * (float)BuildBattleMaxPoints);
        if (R.Player) R.Player->GameScore += R.Points;
        Rows.Emplace(R, E.Canvas.Get());
    }
    Rows.StableSort([](const TPair<FPTBuildBattleResult, APTSculptVolume*>& A,
                       const TPair<FPTBuildBattleResult, APTSculptVolume*>& B)
    {
        if (A.Key.AvgStars != B.Key.AvgStars) return A.Key.AvgStars > B.Key.AvgStars;
        return A.Key.NumVotes > B.Key.NumVotes;
    });

    G->BBRoundResults.Reset();
    for (const TPair<FPTBuildBattleResult, APTSculptVolume*>& Row : Rows) G->BBRoundResults.Add(Row.Key);
    G->BBShowcaseVolume  = Rows.Num() > 0 ? Rows[0].Value : nullptr; // la ganadora queda en exhibición
    G->BBShowcaseIndex   = 0;
    G->BBShowcaseCount   = 0;
    G->TurnPhase         = EPTTurnPhase::RoundResults;
    G->TurnEndServerTime = G->GetServerWorldTimeSeconds() + RoundResultsDuration;
    G->OnTurnPhaseChanged.Broadcast();

    if (Rows.Num() > 0 && Rows[0].Key.NumVotes > 0)
        G->Multicast_SystemLine(TEXT("BB_CHAT_ROUND_WINNER"), Rows[0].Key.Name, Rows[0].Key.Points);

    UE_LOG(LogTemp, Log, TEXT("[BuildBattle] Resultados ronda %d:"), G->CurrentRound);
    for (const FPTBuildBattleResult& R : G->BBRoundResults)
        UE_LOG(LogTemp, Log, TEXT("[BuildBattle]   %s  %.2f (%d votos)  +%d"), *R.Name, R.AvgStars, R.NumVotes, R.Points);

    GetWorldTimerManager().ClearTimer(PhaseTimer);
    GetWorldTimerManager().SetTimer(PhaseTimer, this, &APTSculptGameMode::AdvanceBuildRound,
                                    RoundResultsDuration, false);
}

void APTSculptGameMode::AdvanceBuildRound()
{
    APTSculptGameState* G = GS();
    if (!G) return;
    if (G->CurrentRound >= NumRounds) { EndGame(); return; } // pantalla de fin de siempre (puntaje acumulado)
    G->CurrentRound += 1;
    StartBuildRound();
}

// ── Bots de prueba (DEV) ─────────────────────────────────────────────────────

void APTSculptGameMode::AddBuildBattleBots(int32 Count)
{
    UWorld* W = GetWorld();
    if (!W) return;

    for (int32 i = 0; i < FMath::Clamp(Count, 1, 8); ++i)
    {
        // PlayerState SIN controller, como los jugadores del celular: entra al PlayerArray y juega igual.
        FActorSpawnParameters P;
        P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        P.ObjectFlags |= RF_Transient;
        UClass* Cls = PlayerStateClass ? PlayerStateClass.Get() : APTPlayerState::StaticClass();
        APTPlayerState* PS = W->SpawnActor<APTPlayerState>(Cls, P);
        if (!PS) continue;

        const FString Name = FString::Printf(TEXT("Bot %d"), NextBuildBotId++);
        PS->SetPlayerId(NextPhonePlayerId++);
        PS->SetPlayerName(Name);
        PS->DisplayName = Name;
        PS->PartyColor  = FLinearColor::MakeFromHSV8((uint8)FMath::RandRange(0, 255), 170, 235);
        PS->GameScore   = 0;
        BuildBots.Add(PS);
        UE_LOG(LogTemp, Log, TEXT("[BuildBattle] DEV: bot '%s' agregado."), *Name);
    }
    CheckStart(); // con los bots puede que ya alcance para arrancar
}

void APTSculptGameMode::BuildBotsSculptTick()
{
    const APTSculptGameState* G = GS();
    if (!G || G->TurnPhase != EPTTurnPhase::Drawing) return;

    for (const TWeakObjectPtr<APTPlayerState>& B : BuildBots)
    {
        APTPlayerState* Bot = B.Get();
        APTSculptVolume* V = GetBuildCanvasFor(Bot);
        FTransform BoxXf;
        FVector    Ext;
        if (!V || !V->GetCanvasBox(BoxXf, Ext)) continue;

        // Paseo al azar desde abajo del centro (espacio local del lienzo): sale un bicho de bolas y bloques.
        FVector& Cur = BuildBotCursor.FindOrAdd(Bot, FVector(0.f, 0.f, -Ext.Z * 0.4f));
        Cur += FMath::VRand() * Ext.GetMin() * 0.12f;
        Cur = Cur.BoundToBox(-Ext * 0.55f, Ext * 0.55f);

        const bool bPaint = FMath::FRand() < 0.15f; // de vez en cuando pinta encima, con otro tono
        const EPTStampShape Shape = (FMath::FRand() < 0.6f)
            ? EPTStampShape::Sphere : static_cast<EPTStampShape>(FMath::RandRange(1, 8));
        const FLinearColor Col = bPaint ? FLinearColor::MakeRandomColor() : Bot->PartyColor;
        V->Multicast_ApplyStamp(BoxXf.TransformPosition(Cur), Shape, FMath::FRandRange(80.f, 200.f),
            bPaint ? EPTEditMode::Paint : EPTEditMode::Add, Col,
            FRotator(FMath::FRandRange(-45.f, 45.f), FMath::FRandRange(0.f, 360.f), 0.f),
            /*bDetail=*/false, FVector::OneVector);
    }
}
