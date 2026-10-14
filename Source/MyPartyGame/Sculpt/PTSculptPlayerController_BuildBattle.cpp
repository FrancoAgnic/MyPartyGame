// BUILD BATTLE (prototipo) del lado del PlayerController. Archivo aparte (como _Gamepad.cpp) para no
// engordar PTSculptPlayerController.cpp. La lógica del modo vive en PTSculptGameMode_BuildBattle.cpp.
//  · Local: Volume apunta a TU lienzo (toda la lógica de esculpido de siempre — preview, raymarch, límites —
//    queda igual), se crea el overlay del modo y las teclas 1-5 votan durante la exhibición.
//  · Server: los RPC de esculpido caen en el lienzo del jugador que los manda (ResolveServerVolume).

#include "PTSculptPlayerController.h"
#include "PTSculptGameMode.h"
#include "PTSculptGameState.h"
#include "PTSculptVolume.h"
#include "PTGameplayHUDWidget.h"
#include "PTBuildBattleWidget.h"
#include "../Lobby/PTPlayerState.h"
#include "../PTGameInstance.h"
#include "ProceduralMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"

namespace
{
    // Vértices dibujados de un lienzo (base + capas + bloques SVO + ojos): ¿tiene algo esculpido en ESTA máquina?
    int32 BB_CountVerts(const APTSculptVolume* V)
    {
        int32 N = 0;
        auto Add = [&N](const UProceduralMeshComponent* M)
        {
            if (!M) return;
            for (int32 s = 0; s < M->GetNumSections(); ++s)
                if (const FProcMeshSection* Sec = const_cast<UProceduralMeshComponent*>(M)->GetProcMeshSection(s))
                    N += Sec->ProcVertexBuffer.Num();
        };
        Add(V->GetMeshComponent());
        for (const UProceduralMeshComponent* M : V->GetDetailMeshes()) Add(M);
        for (const TPair<int32, UProceduralMeshComponent*>& C : V->GetSVOChunkMeshes()) Add(C.Value);
        return N;
    }
}

APTSculptVolume* APTSculptPlayerController::ResolveServerVolume()
{
    UWorld* W = GetWorld();
    // Build Battle: cada uno esculpe en SU lienzo (lo sabe el GameMode). Nunca caer al cubo compartido ni al
    // de otro: si no tiene (entró a mitad de ronda), no esculpe.
    if (const APTSculptGameState* G = W ? W->GetGameState<APTSculptGameState>() : nullptr)
        if (G->IsBuildBattle())
        {
            const APTSculptGameMode* GM = W->GetAuthGameMode<APTSculptGameMode>();
            return GM ? GM->GetBuildCanvasFor(GetPlayerState<APTPlayerState>()) : nullptr;
        }
    if (!IsValid(Volume)) Volume = APTSculptVolume::FindLevelCanvas(W);
    return Volume;
}

void APTSculptPlayerController::TickBuildBattle()
{
    UWorld* W = GetWorld();
    const APTSculptGameState* G = W ? W->GetGameState<APTSculptGameState>() : nullptr;
    if (!G || !G->IsBuildBattle())
    {
        // Se volvió al clásico (comando dev): nunca quedarse apuntando a un lienzo (se destruyen al arrancar).
        if (Volume && Volume->IsBuildBattleCanvas()) Volume = APTSculptVolume::FindLevelCanvas(W);
        return;
    }

    // Tu lienzo (o el cubo del nivel si todavía no llegó / no tenés): nunca el de otro jugador.
    APTSculptVolume* Want = G->GetLocalBuildCanvas();
    if (!Want) Want = APTSculptVolume::FindLevelCanvas(W);
    if (Want && Volume != Want) Volume = Want;

    LogBuildBattleState(G);

    if (!BuildBattleWidget)
    {
        BuildBattleWidget = CreateWidget<UPTBuildBattleWidget>(this, UPTBuildBattleWidget::StaticClass());
        if (BuildBattleWidget) BuildBattleWidget->AddToViewport(5);
    }

    // Votar con 1..5 (fila de números o numpad) mientras se exhibe una escultura que no es la tuya.
    if (G->TurnPhase != EPTTurnPhase::Showcase) { BBVoteShowcaseIndex = -1; return; }
    if (G->BBShowcaseIndex != BBVoteShowcaseIndex)
    {
        BBVoteShowcaseIndex = G->BBShowcaseIndex;
        BBMyVote = 0; // escultura nueva: todavía no la votaste
    }
    if (G->IsLocalShowcaseOwn() || (GameplayHUD && GameplayHUD->IsChatOpen())) return;

    static const FKey Row[5] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five };
    static const FKey Pad[5] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive };
    for (int32 i = 0; i < 5; ++i)
        if (WasInputKeyJustPressed(Row[i]) || WasInputKeyJustPressed(Pad[i]))
        {
            BBMyVote = i + 1;
            Server_BuildBattleVote(BBMyVote);
            break;
        }
}

void APTSculptPlayerController::LogBuildBattleState(const APTSculptGameState* G)
{
    // Una línea por cambio de fase / escultura exhibida: qué ve ESTA máquina. Sirve para depurar el prototipo
    // en red (cada cliente tiene sus lienzos y su visibilidad).
    const FString Key = FString::Printf(TEXT("%d|%d|%d|%d"), (int32)G->TurnPhase, G->CurrentRound, G->BBShowcaseIndex,
                                        G->BBRoundResults.Num());
    if (Key != BBLastLogKey) { BBLastLogKey = Key; BBLogCountdown = 3; return; }
    if (BBLogCountdown <= 0 || --BBLogCountdown > 0) return;

    const APTSculptVolume* Mine = G->GetLocalBuildCanvas();
    FString Canvases;
    int32 Count = 0;
    for (TActorIterator<APTSculptVolume> It(GetWorld()); It; ++It)
    {
        if (!It->IsBuildBattleCanvas()) continue;
        ++Count;
        Canvases += FString::Printf(TEXT(" [%s%s%s v=%d]"), *It->GetName(), *It == Mine ? TEXT(" MIO") : TEXT(""),
                                    It->IsHidden() ? TEXT("") : TEXT(" VISIBLE"), BB_CountVerts(*It));
    }
    const UEnum* PhaseEnum = StaticEnum<EPTTurnPhase>();
    UE_LOG(LogTemp, Log, TEXT("[BuildBattle][%s] fase=%s ronda=%d/%d exhibida=%d/%d mia=%s palabra='%s' lienzos=%d:%s"),
        HasAuthority() ? TEXT("host") : TEXT("cliente"),
        PhaseEnum ? *PhaseEnum->GetNameStringByValue((int64)G->TurnPhase) : TEXT("?"),
        G->CurrentRound, G->TotalRounds, G->BBShowcaseIndex, G->BBShowcaseCount,
        G->IsLocalShowcaseOwn() ? TEXT("si") : TEXT("no"), *G->MaskedWord, Count, *Canvases);
    for (const FPTBuildBattleResult& R : G->BBRoundResults)
        UE_LOG(LogTemp, Log, TEXT("[BuildBattle][tabla] %s %.2f (%d votos) +%d"), *R.Name, R.AvgStars, R.NumVotes, R.Points);
}

void APTSculptPlayerController::PTBBVote(int32 Stars)
{
    const APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
    if (!G || G->TurnPhase != EPTTurnPhase::Showcase || Stars < 1 || Stars > 5) return;
    BBMyVote = Stars;
    Server_BuildBattleVote(Stars);
}

void APTSculptPlayerController::PTBBScribble()
{
    // Por la vía NORMAL de esculpido (Server RPC → el server lo pone en TU lienzo → multicast a todos).
    if (!Volume || !CanLocalPlayerSculpt()) return;
    FTransform BoxXf;
    FVector    Ext;
    if (!Volume->GetCanvasBox(BoxXf, Ext)) return;
    Server_BeginStroke();
    for (int32 i = 0; i < 12; ++i)
        Server_ApplyStamp(BoxXf.TransformPosition(FMath::VRand() * Ext * 0.3f), EPTStampShape::Sphere, 160.f,
                          EPTEditMode::Add, FLinearColor::MakeRandomColor(), FRotator::ZeroRotator, false, FVector::OneVector);
    Server_EndStroke();
}

void APTSculptPlayerController::PTMatchTime(int32 Seconds, int32 Rounds)
{
    UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    if (!GI || !HasAuthority()) return;
    if (Seconds > 0) GI->PendingMatchSettings.TurnDuration = (float)Seconds;
    if (Rounds > 0)  GI->PendingMatchSettings.NumRounds = Rounds;
    UE_LOG(LogTemp, Log, TEXT("[PTMatchTime] Próxima partida: %.0fs por turno, %d rondas."),
        GI->PendingMatchSettings.TurnDuration, GI->PendingMatchSettings.NumRounds);
}

void APTSculptPlayerController::Server_BuildBattleVote_Implementation(int32 Stars)
{
    if (APTSculptGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APTSculptGameMode>() : nullptr)
        GM->HandleBuildBattleVote(GetPlayerState<APTPlayerState>(), Stars);
}

void APTSculptPlayerController::PTBuildBattle()
{
    APTSculptGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APTSculptGameMode>() : nullptr;
    if (!GM) { UE_LOG(LogTemp, Warning, TEXT("[BuildBattle] Solo el host puede cambiar el modo.")); return; }
    const EPTGameType Type = GM->ToggleGameType();
    UE_LOG(LogTemp, Log, TEXT("[BuildBattle] Modo: %s (ya, si la partida espera o terminó; si no, en la próxima)."),
        Type == EPTGameType::BuildBattle ? TEXT("Build Battle") : TEXT("Clasico"));
}

void APTSculptPlayerController::PTBBBots(int32 N)
{
    APTSculptGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<APTSculptGameMode>() : nullptr;
    if (!GM) { UE_LOG(LogTemp, Warning, TEXT("[BuildBattle] Solo el host puede agregar bots.")); return; }
    GM->AddBuildBattleBots(N > 0 ? N : 3);
}
