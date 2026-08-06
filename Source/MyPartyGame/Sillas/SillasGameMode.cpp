// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasGameMode.h"
#include "SillasBalanceData.h"
#include "SillasGameState.h"
#include "SillasPawnCazador.h"
#include "SillasPawnSilla.h"
#include "SillasPlayerController.h"
#include "SillasPlayerState.h"
#include "SillasHUD.h"
#include "SillasSenuelo.h"
#include "EngineUtils.h"
#include "MultiplayerSessionsSubsystem.h"
#include "Engine/TargetPoint.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ASillasGameMode::ASillasGameMode()
{
    GameStateClass        = ASillasGameState::StaticClass();
    PlayerControllerClass = ASillasPlayerController::StaticClass();
    PlayerStateClass      = ASillasPlayerState::StaticClass();
    HUDClass              = ASillasHUD::StaticClass();

    // Simétrico con PTLobbyGameMode: el viaje arena → lobby al terminar el match
    // también tiene que ser seamless para no tirar la sesión Steam.
    bUseSeamlessTravel = true;

    // Ruta por defecto del asset de balance (se crea en el editor; ver
    // SillasBalanceData.h). Editable por si algún mapa quiere otro tuning.
    BalanceAsset = TSoftObjectPtr<USillasBalanceData>(
        FSoftObjectPath(TEXT("/Game/Sillas/Data/DA_SillasBalance.DA_SillasBalance")));

    // Pawns por rol. El mannequin del template queda solo como pawn de la fase
    // Esperando (llegada del lobby, antes de la primera ronda).
    PawnSillaClass   = ASillasPawnSilla::StaticClass();
    PawnCazadorClass = ASillasPawnCazador::StaticClass();
    SenueloClass     = ASillasSenuelo::StaticClass();
    static ConstructorHelpers::FClassFinder<APawn> Mannequin(
        TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
    if (Mannequin.Succeeded())
    {
        DefaultPawnClass = Mannequin.Class;
    }
}

void ASillasGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);

    Balance = BalanceAsset.LoadSynchronous();
    if (!Balance)
    {
        // Sin asset todavía: defaults de C++. El playtest serio necesita el asset
        // (tocar números sin recompilar), pero esto mantiene el modo funcional.
        Balance = NewObject<USillasBalanceData>(this);
        UE_LOG(LogTemp, Warning,
               TEXT("[Sillas] DA_SillasBalance no encontrado; usando defaults de C++."));
    }

    // D8 — config del host, viaja como opciones en la URL desde el lobby
    // (ASillasLobbyGameMode las agrega al ServerTravel). 0 = usar defaults.
    RondasOverride    = UGameplayStatics::GetIntOption(Options, TEXT("Rondas"), 0);
    CazadoresOverride = UGameplayStatics::GetIntOption(Options, TEXT("Cazadores"), 0);
}

void ASillasGameMode::BeginPlay()
{
    Super::BeginPlay();

    // El GameState no viaja con el seamless travel: volver a poblar la info de
    // sesión desde el subsistema del host, igual que hace el lobby (PTLobbyGameMode).
    if (ASillasGameState* GS = SillasGS())
    {
        // Compartir el balance con los clientes (ver comentario en ASillasGameState).
        GS->Balance = Balance;

        if (UMultiplayerSessionsSubsystem* Sessions =
                GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>())
        {
            GS->SessionDisplayName = Sessions->GetPendingSessionName();
            GS->SessionCode        = Sessions->GetGeneratedSessionCode();
            GS->MaxPlayers         = Sessions->GetPendingMaxPlayers();
        }
    }

    // Arranque de la primera ronda tras dar aire al seamless travel.
    GetWorldTimerManager().SetTimer(
        FaseTimer, this, &ASillasGameMode::IniciarRonda,
        FMath::Max(0.1f, Balance->EsperaInicialSeg), /*bLoop=*/false);
}

// ------------------------- Flujo de ronda (Fase 1) -------------------------

void ASillasGameMode::IniciarRonda()
{
    ASillasGameState* GS = SillasGS();
    if (!GS) return;

    GS->RondaActual++;
    GS->RondasTotales = RondasOverride > 0 ? RondasOverride : Balance->RondasPorMatch;
    GS->AgregarFeed(FString::Printf(TEXT("— Ronda %d de %d —"),
                                    GS->RondaActual, GS->RondasTotales));

    // Todos vuelven a ser sillas; se eligen los cazadores iniciales (D7/D8).
    AsignarRoles();

    // Señuelos frescos y cada jugador con el pawn de su rol, mezclados en el mapa.
    RepartirPawnsYSenuelos();

    SetFase(ESillasFase::IntroRonda, Balance->IntroRondaSeg);
    GetWorldTimerManager().SetTimer(
        FaseTimer, this, &ASillasGameMode::EmpezarMusica, Balance->IntroRondaSeg, false);

    UE_LOG(LogTemp, Log, TEXT("[Sillas] Ronda %d: %d sillas, %d jugadores."),
           GS->RondaActual, GS->SillasVivas, GS->PlayerArray.Num());
}

void ASillasGameMode::EmpezarMusica()
{
    // D7b: las sillas que completaron el Silencio anterior puntúan.
    OtorgarPuntosSupervivencia();

    // D13: suena la música → los cazadores bailan (corta cualquier caminata de
    // cola en curso). La ventana SEGURA de las sillas para reposicionarse.
    for (TActorIterator<ASillasPawnCazador> It(GetWorld()); It; ++It)
    {
        It->EmpezarBaileServer();
    }

    const float Dur = DuracionMusicaActual();
    SetFase(ESillasFase::Musica, Dur);
    GetWorldTimerManager().SetTimer(
        FaseTimer, this, &ASillasGameMode::EmpezarSilencio, Dur, false);
}

void ASillasGameMode::EmpezarSilencio()
{
    // D7b: las sillas que completaron la Música puntúan.
    OtorgarPuntosSupervivencia();

    // Se corta la música: fin del baile, empieza la caza.
    for (TActorIterator<ASillasPawnCazador> It(GetWorld()); It; ++It)
    {
        It->TerminarBaileServer();
    }

    const float Dur = DuracionSilencioActual();
    SetFase(ESillasFase::Silencio, Dur);
    GetWorldTimerManager().SetTimer(
        FaseTimer, this, &ASillasGameMode::EmpezarMusica, Dur, false);
}

void ASillasGameMode::OtorgarPuntosSupervivencia()
{
    ASillasGameState* GS = SillasGS();
    if (!GS) return;

    // Solo cuenta una fase de juego real completada (Musica o Silencio).
    if (GS->Fase != ESillasFase::Musica && GS->Fase != ESillasFase::Silencio) return;

    for (APlayerState* PS : GS->PlayerArray)
    {
        if (ASillasPlayerState* SPS = Cast<ASillasPlayerState>(PS))
        {
            if (SPS->Rol == ESillasRole::Silla && !SPS->bEliminadoEstaRonda)
            {
                SPS->Server_SumarPuntos(Balance->PuntosPorFaseSobrevivida);
            }
        }
    }
}

void ASillasGameMode::TerminarRonda()
{
    ASillasGameState* GS = SillasGS();
    if (!GS) return;

    // D7b: bonus a la última silla viva (la ganadora de la ronda). La regla de
    // balance de referencia — ganar vivo ≥ mejor cazador — se tunea en el asset.
    for (APlayerState* PS : GS->PlayerArray)
    {
        ASillasPlayerState* SPS = Cast<ASillasPlayerState>(PS);
        if (SPS && SPS->Rol == ESillasRole::Silla && !SPS->bEliminadoEstaRonda)
        {
            SPS->Server_SumarPuntos(Balance->BonusUltimoVivo);
            GS->AgregarFeed(FString::Printf(TEXT("%s gana la ronda (+%d)"),
                            *SPS->DisplayName, Balance->BonusUltimoVivo));
        }
    }

    SetFase(ESillasFase::FinRonda, Balance->FinRondaSeg);

    if (GS->RondaActual >= GS->RondasTotales)
    {
        GetWorldTimerManager().SetTimer(
            FaseTimer, this, &ASillasGameMode::TerminarMatch, Balance->FinRondaSeg, false);
    }
    else
    {
        GetWorldTimerManager().SetTimer(
            FaseTimer, this, &ASillasGameMode::IniciarRonda, Balance->FinRondaSeg, false);
    }
}

void ASillasGameMode::TerminarMatch()
{
    ASillasGameState* GS = SillasGS();
    if (!GS) return;

    // Podio: el HUD lo dibuja leyendo PuntosMatch; acá solo el anuncio.
    ASillasPlayerState* Campeon = nullptr;
    for (APlayerState* PS : GS->PlayerArray)
    {
        ASillasPlayerState* SPS = Cast<ASillasPlayerState>(PS);
        if (SPS && (!Campeon || SPS->PuntosMatch > Campeon->PuntosMatch))
        {
            Campeon = SPS;
        }
    }
    if (Campeon)
    {
        GS->AgregarFeed(FString::Printf(TEXT("*** %s gana el match con %d puntos ***"),
                        *Campeon->DisplayName, Campeon->PuntosMatch));
    }

    SetFase(ESillasFase::FinMatch, Balance->FinMatchSeg);
    GetWorldTimerManager().SetTimer(
        FaseTimer, this, &ASillasGameMode::VolverAlLobby, Balance->FinMatchSeg, false);
    UE_LOG(LogTemp, Log, TEXT("[Sillas] Match terminado."));
}

void ASillasGameMode::VolverAlLobby()
{
    // Seamless: la sesión Steam sigue viva y el lobby queda listo para otro
    // Start del host (los PlayerStates vuelven a las clases PT del template).
    UE_LOG(LogTemp, Log, TEXT("[Sillas] Volviendo al lobby: %s"), *LobbyMapPath);
    GetWorld()->ServerTravel(LobbyMapPath);
}

void ASillasGameMode::ResolverSentado(ASillasPawnCazador* Cazador, AActor* Objetivo)
{
    if (!HasAuthority() || !Cazador || !Objetivo) return;

    if (Cast<ASillasSenuelo>(Objetivo))
    {
        // D6: señuelo sólido — cazador adolorido, el señuelo ni se inmuta.
        Cazador->AplicarDolorServer();
        UE_LOG(LogTemp, Log, TEXT("[Sillas] %s se sentó en un señuelo: %.1fs de dolor."),
               *GetNameSafe(Cazador->GetPlayerState()), Balance->DuracionDolorSeg);
        return;
    }

    if (ASillasPawnSilla* Silla = Cast<ASillasPawnSilla>(Objetivo))
    {
        RomperSilla(Silla, Cazador);
    }
}

void ASillasGameMode::RomperSilla(ASillasPawnSilla* Silla, ASillasPawnCazador* Cazador)
{
    ASillasGameState* GS = SillasGS();
    if (!GS || !Silla) return;

    const FVector Lugar = Silla->GetActorLocation();
    AController* Victima = Silla->GetController();
    ASillasPlayerState* PS = Victima ? Victima->GetPlayerState<ASillasPlayerState>() : nullptr;

    UE_LOG(LogTemp, Log, TEXT("[Sillas] ¡Silla rota! %s cazó a %s."),
           *GetNameSafe(Cazador ? Cazador->GetPlayerState() : nullptr), *GetNameSafe(PS));

    // D7b: la captura puntúa + línea en el feed.
    ASillasPlayerState* PSCazador =
        Cazador ? Cazador->GetPlayerState<ASillasPlayerState>() : nullptr;
    if (PSCazador)
    {
        PSCazador->Server_SumarPuntos(Balance->PuntosPorCaptura);
    }
    GS->AgregarFeed(FString::Printf(TEXT("%s cazó a %s (+%d)"),
                    PSCazador ? *PSCazador->DisplayName : TEXT("¿?"),
                    PS ? *PS->DisplayName : TEXT("¿?"),
                    Balance->PuntosPorCaptura));

    // Teatro en todos los clientes + la silla desaparece.
    GS->Multicast_EfectoRoturaSilla(Lugar);
    Silla->Destroy();

    // El sentado termina la caminata del cazador (los puntos por captura llegan en Fase 5).
    if (Cazador)
    {
        Cazador->CancelarCapturaServer();
    }

    // D1 (infección): el eliminado se re-posee como cazador EN EL LUGAR de la
    // rotura, sin cortar el flujo de la ronda.
    if (Victima && PawnCazadorClass)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride =
            ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        if (APawn* Nuevo = GetWorld()->SpawnActor<APawn>(
                PawnCazadorClass, Lugar + FVector(0.f, 0.f, 40.f),
                FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f), Params))
        {
            Victima->Possess(Nuevo);
        }
    }

    // Al final: puede disparar FinRonda (D2) si quedó una sola silla viva.
    NotificarSillaEliminada(PS);
}

void ASillasGameMode::NotificarSillaEliminada(ASillasPlayerState* Eliminado)
{
    ASillasGameState* GS = SillasGS();
    if (!GS || !Eliminado || !HasAuthority()) return;

    Eliminado->Server_MarcarEliminado();
    Eliminado->Server_SetRol(ESillasRole::Cazador); // D1: infección
    GS->SillasVivas = FMath::Max(0, GS->SillasVivas - 1);

    // D2: queda una sola silla viva → gana la ronda.
    if (GS->SillasVivas <= 1)
    {
        GetWorldTimerManager().ClearTimer(FaseTimer);
        TerminarRonda();
    }
}

void ASillasGameMode::AsignarRoles()
{
    ASillasGameState* GS = SillasGS();
    if (!GS) return;

    // Juntar los PlayerStates del modo y resetearlos a Silla.
    TArray<ASillasPlayerState*> Jugadores;
    for (APlayerState* PS : GS->PlayerArray)
    {
        if (ASillasPlayerState* SPS = Cast<ASillasPlayerState>(PS))
        {
            SPS->Server_ResetRonda();
            Jugadores.Add(SPS);
        }
    }

    // D8: cazadores iniciales — configurado por el host desde el lobby
    // (?Cazadores=) o el default por cantidad de jugadores (1, o 2 desde el umbral).
    const int32 NumCazadores = FMath::Clamp(
        CazadoresOverride > 0
            ? CazadoresOverride
            : (Jugadores.Num() >= Balance->UmbralJugadoresParaDosCazadores ? 2 : 1),
        1, FMath::Max(1, Jugadores.Num() - 1));

    // D7: elegir cazadores al azar SIN repetir hasta agotar la lista.
    TArray<ASillasPlayerState*> Candidatos;
    for (ASillasPlayerState* J : Jugadores)
    {
        if (!YaFueronCazadorInicial.Contains(J)) Candidatos.Add(J);
    }
    if (Candidatos.Num() < NumCazadores)
    {
        // Se agotó la rotación: arranca de nuevo.
        YaFueronCazadorInicial.Reset();
        Candidatos = Jugadores;
    }

    for (int32 i = 0; i < NumCazadores && Candidatos.Num() > 0; ++i)
    {
        const int32 Idx = FMath::RandRange(0, Candidatos.Num() - 1);
        ASillasPlayerState* Elegido = Candidatos[Idx];
        Elegido->Server_SetRol(ESillasRole::Cazador);
        YaFueronCazadorInicial.Add(Elegido);
        Candidatos.RemoveAtSwap(Idx);
        Jugadores.Remove(Elegido);
    }

    GS->SillasVivas          = Jugadores.Num(); // los que quedaron como silla
    GS->SillasAlInicioDeRonda = Jugadores.Num();

    // Fase 1 (siguiente tarea): posesión de pawns Silla/Cazador según rol.
    // Con el pawn placeholder de la Fase 0 todos caminan igual por ahora.
}

void ASillasGameMode::RepartirPawnsYSenuelos()
{
    ASillasGameState* GS = SillasGS();
    if (!GS) return;

    // Señuelos de la ronda anterior afuera (la densidad se decide por ronda).
    for (ASillasSenuelo* S : Senuelos)
    {
        if (IsValid(S)) S->Destroy();
    }
    Senuelos.Reset();
    PuntoAsignado.Reset();

    // Puntos de spawn del mapa (TargetPoints con tag "SillaSpawn"), mezclados.
    TArray<AActor*> Puntos;
    UGameplayStatics::GetAllActorsOfClassWithTag(
        GetWorld(), ATargetPoint::StaticClass(), FName(TEXT("SillaSpawn")), Puntos);
    for (int32 i = Puntos.Num() - 1; i > 0; --i)
    {
        Puntos.Swap(i, FMath::RandRange(0, i));
    }

    // Primero reservan punto las sillas-jugador; los señuelos toman el resto.
    int32 PuntoIdx = 0;
    for (APlayerState* PS : GS->PlayerArray)
    {
        ASillasPlayerState* SPS = Cast<ASillasPlayerState>(PS);
        AController* PC = SPS ? SPS->GetOwningController() : nullptr;
        if (!PC) continue;

        if (SPS->Rol == ESillasRole::Silla && Puntos.IsValidIndex(PuntoIdx))
        {
            PuntoAsignado.Add(PC, Puntos[PuntoIdx++]);
        }
    }

    // D9: densidad media de señuelos, limitada por los puntos que sobran.
    // (El greybox tiene 20 puntos; con el mapa real de Fase 6 suben si hace falta.)
    const int32 PuntosLibres    = Puntos.Num() - PuntoIdx;
    const int32 SenuelosDeseados = FMath::RandRange(Balance->SenuelosMin, Balance->SenuelosMax);
    const int32 NumSenuelos      = FMath::Min(SenuelosDeseados, PuntosLibres);
    if (NumSenuelos < SenuelosDeseados)
    {
        UE_LOG(LogTemp, Warning,
               TEXT("[Sillas] Señuelos recortados por falta de puntos: %d de %d deseados."),
               NumSenuelos, SenuelosDeseados);
    }

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    for (int32 i = 0; i < NumSenuelos; ++i)
    {
        const AActor* Punto = Puntos[PuntoIdx + i];
        const FVector2D Jitter = FMath::RandPointInCircle(Balance->JitterSpawnSenuelo);
        const FVector Loc = Punto->GetActorLocation() + FVector(Jitter.X, Jitter.Y, 60.f);
        const FRotator Rot(0.f, FMath::FRandRange(0.f, 360.f), 0.f);

        if (ASillasSenuelo* S = GetWorld()->SpawnActor<ASillasSenuelo>(
                SenueloClass ? *SenueloClass : ASillasSenuelo::StaticClass(), Loc, Rot, Params))
        {
            Senuelos.Add(S);
        }
    }

    // Respawn de cada jugador con el pawn de su rol (RestartPlayer usa nuestros
    // overrides de clase y punto de spawn).
    for (APlayerState* PS : GS->PlayerArray)
    {
        AController* PC = PS ? PS->GetOwningController() : nullptr;
        if (!PC) continue;

        if (APawn* Viejo = PC->GetPawn())
        {
            Viejo->Destroy();
        }
        RestartPlayer(PC);
    }
}

UClass* ASillasGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    const ASillasGameState* GS = SillasGS();
    const ASillasPlayerState* SPS =
        InController ? InController->GetPlayerState<ASillasPlayerState>() : nullptr;

    // Antes de la primera ronda (recién llegados del lobby) todos pasean con el
    // pawn default. Se mira RondaActual y no la Fase: al repartir pawns en
    // IniciarRonda la fase todavía es la anterior.
    if (!GS || !SPS || GS->RondaActual <= 0)
    {
        return Super::GetDefaultPawnClassForController_Implementation(InController);
    }

    return SPS->Rol == ESillasRole::Cazador
        ? PawnCazadorClass.Get()
        : PawnSillaClass.Get();
}

AActor* ASillasGameMode::FindPlayerStart_Implementation(AController* Player, const FString& IncomingName)
{
    // Silla con punto reservado en el reparto → nace mezclada entre señuelos.
    if (TObjectPtr<AActor>* Punto = Player ? PuntoAsignado.Find(Player) : nullptr)
    {
        if (IsValid(*Punto)) return *Punto;
    }
    // Cazadores (y cualquier caso sin punto) usan los PlayerStarts del mapa.
    return Super::FindPlayerStart_Implementation(Player, IncomingName);
}

void ASillasGameMode::SetFase(ESillasFase NuevaFase, float DuracionSeg)
{
    if (ASillasGameState* GS = SillasGS())
    {
        GS->Fase = NuevaFase;
        GS->FaseTerminaEnServerTime = GS->GetServerWorldTimeSeconds() + DuracionSeg;
        GS->OnRep_Fase(); // el host no recibe su propio OnRep
    }
}

float ASillasGameMode::DuracionMusicaActual() const
{
    const ASillasGameState* GS = SillasGS();
    if (!GS || GS->SillasAlInicioDeRonda <= 0) return Balance->MusicaSegBase;

    // D12: f = fracción de sillas eliminadas en la ronda → menos música.
    const float f = 1.f - (float)GS->SillasVivas / (float)GS->SillasAlInicioDeRonda;
    return FMath::Max(Balance->MusicaSegMin,
                      Balance->MusicaSegBase * (1.f - f * Balance->FactorIntensificacion));
}

float ASillasGameMode::DuracionSilencioActual() const
{
    const ASillasGameState* GS = SillasGS();
    if (!GS || GS->SillasAlInicioDeRonda <= 0) return Balance->SilencioSegBase;

    // D12: más silencio (más caza) hacia el final de la ronda.
    const float f = 1.f - (float)GS->SillasVivas / (float)GS->SillasAlInicioDeRonda;
    return FMath::Min(Balance->SilencioSegMax,
                      Balance->SilencioSegBase * (1.f + f * Balance->FactorIntensificacion));
}

ASillasGameState* ASillasGameMode::SillasGS() const
{
    return GetGameState<ASillasGameState>();
}

// --------------- Integración con el flujo de sesión (Fase 0) ---------------

void ASillasGameMode::PreLogin(const FString& Options, const FString& Address,
                               const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    if (!ErrorMessage.IsEmpty()) return;

    // Join-in-progress a una sala privada: misma validación que el lobby.
    // (Los que llegan por seamless travel NO pasan por acá — ya estaban conectados.)
    const FString Attempt = UGameplayStatics::ParseOption(Options, TEXT("Password"));
    if (UMultiplayerSessionsSubsystem* Sessions =
            GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>())
    {
        if (!Sessions->DoesHostPasswordMatch(Attempt))
        {
            ErrorMessage = TEXT("WrongPassword");
            UE_LOG(LogTemp, Warning, TEXT("[Sillas] PreLogin rechazado: contraseña incorrecta."));
        }
    }
}

void ASillasGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);

    // Solo conexiones directas (join-in-progress). Mismo criterio que el lobby:
    // el nombre real llegó como "?Name=" y el motor ya lo dejó en GetPlayerName().
    if (APTPlayerState* PS = NewPlayer->GetPlayerState<APTPlayerState>())
    {
        const bool bIsHost = NewPlayer->IsLocalController();
        PS->Server_SetHost(bIsHost);

        FString Name = NewPlayer->PlayerState ? NewPlayer->PlayerState->GetPlayerName() : FString();
        if (Name.IsEmpty())
        {
            Name = bIsHost ? TEXT("Host") : FString::Printf(TEXT("Player_%d"), PlayersJoined + 1);
        }
        PS->Server_SetDisplayName(Name);
    }

    // Quien entra a mitad de partida es cazador (D1: nadie espera afuera) —
    // salvo que la ronda no haya arrancado. No toca SillasVivas.
    if (ASillasPlayerState* SPS = NewPlayer->GetPlayerState<ASillasPlayerState>())
    {
        const ASillasGameState* GS = SillasGS();
        if (GS && GS->Fase != ESillasFase::Esperando)
        {
            SPS->Server_SetRol(ESillasRole::Cazador);
        }
    }

    ++PlayersJoined;
    UE_LOG(LogTemp, Log, TEXT("[Sillas] PostLogin. Jugadores conectados: %d"), PlayersJoined);
}

void ASillasGameMode::HandleSeamlessTravelPlayer(AController*& C)
{
    // El viaje desde el lobby reemplaza PC y PlayerState por las clases Sillas.
    // APTPlayerState::CopyProperties no copia sus campos propios, así que
    // DisplayName/bIsHost se capturan del PlayerState viejo ANTES del swap.
    FString OldName;
    bool bWasHost = false;
    if (const APTPlayerState* OldPS = C ? C->GetPlayerState<APTPlayerState>() : nullptr)
    {
        OldName  = OldPS->DisplayName;
        bWasHost = OldPS->bIsHost;
    }

    Super::HandleSeamlessTravelPlayer(C);

    if (APTPlayerState* PS = C ? C->GetPlayerState<APTPlayerState>() : nullptr)
    {
        // Fallback a GetPlayerName(): el motor sí copia el nombre nativo en el viaje.
        PS->Server_SetDisplayName(!OldName.IsEmpty() ? OldName : PS->GetPlayerName());
        PS->Server_SetHost(bWasHost);
    }

    ++PlayersJoined;
    UE_LOG(LogTemp, Log, TEXT("[Sillas] Llegó jugador por seamless travel. Conectados: %d"), PlayersJoined);
}

void ASillasGameMode::Logout(AController* Exiting)
{
    --PlayersJoined;
    UE_LOG(LogTemp, Log, TEXT("[Sillas] Logout. Jugadores conectados: %d"), PlayersJoined);

    if (ASillasGameState* GS = SillasGS())
    {
        if (const APlayerState* PSExiting = Exiting ? Exiting->PlayerState : nullptr)
        {
            GS->AgregarFeed(FString::Printf(TEXT("%s se desconectó"), *PSExiting->GetPlayerName()));
        }
    }

    // Una silla que se desconecta a mitad de ronda cuenta como eliminada — si
    // no, la ronda puede quedar esperando a un fantasma.
    if (ASillasPlayerState* SPS = Exiting ? Exiting->GetPlayerState<ASillasPlayerState>() : nullptr)
    {
        const ASillasGameState* GS = SillasGS();
        if (GS && SPS->Rol == ESillasRole::Silla && !SPS->bEliminadoEstaRonda &&
            (GS->Fase == ESillasFase::Musica || GS->Fase == ESillasFase::Silencio))
        {
            NotificarSillaEliminada(SPS);
        }
    }

    // Igual que el lobby: el último en irse apaga la luz (nada de salas fantasma).
    if (PlayersJoined <= 0)
    {
        if (UMultiplayerSessionsSubsystem* Sessions =
                GetGameInstance()->GetSubsystem<UMultiplayerSessionsSubsystem>())
        {
            UE_LOG(LogTemp, Log, TEXT("[Sillas] Último jugador se fue, destruyendo la sesión."));
            Sessions->DestroySession();
        }
    }

    Super::Logout(Exiting);
}
