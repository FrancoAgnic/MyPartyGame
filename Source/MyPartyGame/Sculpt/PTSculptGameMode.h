// GameMode de la partida de Sculpturillo. Corre el loop por turnos (servidor manda):
// elige escultor al azar → 3 palabras → esculpe con reloj → rota. Loop infinito por ahora.

#pragma once
#include "CoreMinimal.h"
#include "../Lobby/PTLobbyGameMode.h"
#include "../PTMatchSettings.h"
#include "PTSculptGameMode.generated.h"

class APTPlayerState;
class APTSculptGameState;
class APTSculptVolume;
class APTMapEnvironment;

// Hereda de APTLobbyGameMode (AGameMode) para reusar su posesión/pawn/reconexión que
// SÍ funciona; solo cambia el PlayerController, el GameState y agrega el loop por turnos.
UCLASS()
class MYPARTYGAME_API APTSculptGameMode : public APTLobbyGameMode
{
    GENERATED_BODY()

public:
    APTSculptGameMode();

    // ── Tuneables (editar en BP_SculptGameMode) ─────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 MinPlayersToStart = 2;
    // Mínimo EFECTIVO: 1 si el GameInstance tiene bSoloTest (comando "PTSolo"), si no MinPlayersToStart.
    int32 MinToStart() const;
    // DEV: marca bSoloTest y arranca la partida con 1 jugador (lo llama el comando "PTSolo").
    void SoloStart();
    UPROPERTY(EditDefaultsOnly, Category="Game") float TurnDuration      = 90.f; // seg de dibujo
    UPROPERTY(EditDefaultsOnly, Category="Game") float ChooseDuration    = 15.f; // seg para elegir palabra
    // Audiencia: el streamer elige desde su página privada (más pasos: abrir/mirar el navegador off-stream),
    // así que le damos más tiempo. Se usa en vez de ChooseDuration cuando IsAudienceGame().
    UPROPERTY(EditDefaultsOnly, Category="Game|Audience") float AudienceChooseDuration = 45.f;
    // Segundos efectivos para elegir palabra según el modo.
    float EffectiveChooseDuration() const { return IsAudienceGame() ? AudienceChooseDuration : ChooseDuration; }
    UPROPERTY(EditDefaultsOnly, Category="Game") float TurnEndDuration   = 5.f;  // pausa de reveal
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 WordChoiceCount   = 3;    // palabras ofrecidas

    // ── Puntajes y rondas (estilo Skribbl) ──────────────────────────────────
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 NumRounds          = 10;  // rondas antes de terminar
    // Puntos del que adivina: interpola de Max (adivina al instante) a Min (sobre la hora).
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 MaxGuessPoints     = 100;
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 MinGuessPoints     = 25;
    // El escultor gana esto por cada jugador que adivina su escultura.
    UPROPERTY(EditDefaultsOnly, Category="Game") int32 SculptorPointsPerGuess = 25;
    // Modo audiencia: puntos por ORDEN de acierto. El 1.º se lleva AudienceFirstGuessPoints, cada uno de
    // los siguientes la mitad del anterior, sin bajar de AudienceMinGuessPoints (100, 50, 25, 20, 20...).
    // La última palabra vale x AudienceLastWordMultiplier (final abierto: cualquiera puede dar vuelta).
    UPROPERTY(EditDefaultsOnly, Category="Game|Audience") int32 AudienceFirstGuessPoints = 100;
    UPROPERTY(EditDefaultsOnly, Category="Game|Audience") int32 AudienceMinGuessPoints = 20;
    UPROPERTY(EditDefaultsOnly, Category="Game|Audience") int32 AudienceLastWordMultiplier = 2;
    // Fracción de letras a revelar de a poco durante el turno (0.7 = 70% al final).
    UPROPERTY(EditDefaultsOnly, Category="Game") float RevealFraction = 0.7f;
    // Mapa del lobby (para "Volver al lobby" al terminar la partida). En el diseño de lobby
    // interactivo el lobby ES MainMenu (menú y sala fusionados), así que se vuelve ahí con
    // ?listen (queda como listen server → ShowLobbyOverlay muestra el LobbyHUD para re-jugar).
    // El viejo Lobby.umap quedó sin uso.
    UPROPERTY(EditDefaultsOnly, Category="Game") FString LobbyMapPath = TEXT("/Game/Template/levels/MainMenu");

    // Banco de palabras (español) con categoría + dificultad. Si queda vacío se siembra con la
    // lista por defecto (100 palabras categorizadas). Editable en BP_SculptGameMode.
    UPROPERTY(EditDefaultsOnly, Category="Game") TArray<FPTWordEntry> WordBank;

    // Config de partida elegida por el host en el lobby (tiempo/rondas/revelado/categorías/
    // dificultad/CSV). Solo servidor. La copia desde el GameInstance al arrancar (Fase B).
    // Default = sin filtros → entran todas las palabras del banco.
    FPTMatchSettings MatchSettings;

    // Copia PendingMatchSettings del GameInstance a MatchSettings y aplica los tuneables
    // numéricos (tiempo/rondas/revelado) a las variables del GameMode, con clamps de seguridad.
    void ApplyMatchSettingsFromGameInstance();

    // Al llegar a Lvl-01 por seamless travel, los PlayerState/controllers de todos recién se
    // asientan; arrancar la partida en ese instante puede elegir escultor sobre un PlayerState
    // que el motor todavía va a reemplazar → a nadie le toca esculpir. Esperar este delay antes
    // del primer turno lo estabiliza (ver CheckStart).
    UPROPERTY(EditDefaultsOnly, Category="Game") float StartDelay = 2.0f;

    // Llamado por el PlayerController del escultor cuando elige una de las 3 palabras.
    void HandleWordChosen(APTPlayerState* Chooser, int32 ChoiceIndex);

    // Llamado desde el sistema de chat (Fase 4) cuando alguien acierta.
    void HandlePlayerGuessedCorrectly(APTPlayerState* Guesser);

    // Procesa un mensaje de chat en el servidor: detecta aciertos (anti-spoiler) y
    // difunde las líneas visibles a todos. Llamado por Server_SendChat del PC.
    void HandleChat(APTPlayerState* Sender, const FString& Message);

    // ¿El texto coincide con la palabra secreta del turno? (normaliza mayúsc./tildes). Devuelve el ÍNDICE
    // de idioma de la traducción que coincidió (0=ES,1=EN,...), o INDEX_NONE si no coincide ninguna. Así
    // sabemos EN QUÉ IDIOMA adivinó (para mostrar la bandera). Si coincide con el idioma preferido
    // (PreferLang, el del que escribe), se devuelve ESE (evita falsos "otro idioma" con palabras iguales).
    // Server-only: la palabra real nunca sale de acá.
    int32 DoesGuessMatch(const FString& Guess, int32 PreferLang = INDEX_NONE) const;

    // ¿El texto está CERCA de alguna traducción (plural / 1-2 letras de diferencia) sin ser exacto?
    // Para avisarle "¡casi!" al que escribió. Server-only.
    bool IsCloseGuess(const FString& Guess) const;

    // Al terminar la partida (fase GameOver), el HUD del anfitrión llama a una de estas
    // vía el PlayerController. Solo el host puede; se ignora si lo pide otro.
    void RequestPlayAgain(APTPlayerState* Requester);
    void RequestReturnToLobby(APTPlayerState* Requester);

    // ── Modo LOCAL (party con celulares, ver UPTLocalPartySubsystem) ────────
    // Una sola PC (la TV) con el joystick; cada jugador es una PlayerState SIN controller que existe
    // en su celular. La partida no arranca sola: la empieza el "VIP" desde su celular.
    UPROPERTY(EditDefaultsOnly, Category="LocalParty") int32 LocalPartyMinPlayers = 2;

    // Bots: cada jugador del celular tiene un personaje en el mapa con su nombre y color, que flota
    // alrededor de la escultura (los globos de chat y el confeti de sus aciertos salen sobre él).
    UPROPERTY(EditDefaultsOnly, Category="LocalParty") bool bSpawnPartyBots = true;
    // En audiencia solo los primeros N espectadores tienen personaje (el resto juega igual, sin cuerpo).
    UPROPERTY(EditDefaultsOnly, Category="LocalParty") int32 MaxAudienceBots = 25;
    // Controller de los bots (para ajustar distancia / altura / cada cuánto se mueven en un BP hijo).
    UPROPERTY(EditDefaultsOnly, Category="LocalParty") TSubclassOf<class APTPartyBotController> PartyBotControllerClass;
    // Tutorial de Sculpi: el actor que lo dirige (lecciones, fantasmas, foto). Editable en BP.
    UPROPERTY(EditDefaultsOnly, Category="Tutorial") TSubclassOf<class APTTutorialDirector> TutorialDirectorClass;
    bool IsTutorialGame() const;
    /** Tutorial: el jugador esculpe sin turnos ni reloj (fase Drawing fija, él es el escultor). */
    void Tutorial_BeginFreeSculpt(APlayerController* PC);
    // Skins de los bots: al azar entre las del Locker de esta PC y las del Workshop suscritas.
    UPROPERTY(EditDefaultsOnly, Category="LocalParty") bool bBotsUseLockerSkins = true;
    UPROPERTY(EditDefaultsOnly, Category="LocalParty") bool bBotsUseWorkshopSkins = true;

    // ¿Esta partida corre en modo local? (flag del GameInstance, seteado al entrar desde el menú).
    bool IsLocalPartyGame() const;
    // Modo AUDIENCIA (local por internet, para streamers): siempre esculpe el streamer (la PC / TV),
    // la audiencia solo adivina desde el celular. El streamer elige y ve la palabra en SU celular.
    bool IsAudienceGame() const;
    // PlayerState de la PC (el streamer en modo audiencia). null si no hay.
    APTPlayerState* GetTVPlayerState() const;
    // El streamer eligió una de las 3 palabras desde su celular (modo audiencia).
    void LocalParty_ChooseAsHost(int32 ChoiceIndex);

    APTPlayerState* LocalParty_AddPlayer(const FString& Name, const FString& Language, FLinearColor Color);
    void LocalParty_RemovePlayer(APTPlayerState* PS);
    void LocalParty_RequestStart();
    void LocalParty_PlayAgain();
    void LocalParty_ExitToMenu();
    void LocalParty_Guess(APTPlayerState* PS, const FString& Text);
    // Mensaje de alguien que se unió desde el chat de Twitch / Kick: aparece en el chat de la TV y, si es
    // la palabra, cuenta como acierto.
    void LocalParty_ChatGuess(APTPlayerState* PS, const FString& Text);
    /** Audiencia: ¿es la última palabra de la partida? (vale más) */
    bool IsLastAudienceWord() const;
    void LocalParty_Choose(APTPlayerState* PS, int32 ChoiceIndex);
    // Opciones / palabra secreta EN EL IDIOMA del celular. Vacío si ese jugador no es el escultor.
    TArray<FString> LocalParty_GetChoicesFor(const APTPlayerState* PS) const;
    FString LocalParty_GetSecretWordFor(const APTPlayerState* PS) const;
    int32 LocalParty_GetMinPlayers() const { return MinToStart(); }

    // ── BUILD BATTLE (prototipo; lógica en PTSculptGameMode_BuildBattle.cpp) ──────────────────────
    // Todos esculpen la MISMA palabra a la vez durante TurnDuration, cada uno en su lienzo. Después se
    // exhibe cada escultura (anónima, en orden al azar) y todos la votan de 1 a 5 sin ver los votos. Al
    // cerrar la votación de la ronda se revela la tabla y cada uno suma puntos según su promedio.
    /** ¿La partida es Build Battle? Lo elige el host en el lobby. Nunca en modo local ni en el tutorial. */
    bool IsBuildBattle() const;
    // Segundos que se exhibe cada escultura para votarla.
    UPROPERTY(EditDefaultsOnly, Category="Game|BuildBattle") float ShowcaseDuration = 15.f;
    // Si ya votaron todos (o no hay quién vote), la exhibición se acorta: quedan ShowcaseAllVotedGrace
    // segundos, pero nunca menos de ShowcaseMinDuration desde que apareció (para poder mirarla).
    UPROPERTY(EditDefaultsOnly, Category="Game|BuildBattle") float ShowcaseAllVotedGrace = 2.f;
    UPROPERTY(EditDefaultsOnly, Category="Game|BuildBattle") float ShowcaseMinDuration = 5.f;
    // Pausa con la tabla de la ronda (la ganadora queda en exhibición).
    UPROPERTY(EditDefaultsOnly, Category="Game|BuildBattle") float RoundResultsDuration = 10.f;
    // Puntos de una escultura votada 5 por todos. Escala con el promedio: 3.5 → 70.
    UPROPERTY(EditDefaultsOnly, Category="Game|BuildBattle") int32 BuildBattleMaxPoints = 100;
    // DEV (comando PTBBBots): cada cuánto agrega arcilla cada bot mientras se esculpe.
    UPROPERTY(EditDefaultsOnly, Category="Game|BuildBattle") float BuildBotStampInterval = 0.25f;

    /** [server] Lienzo de este jugador en la ronda actual. null si no tiene (entró a mitad de ronda). */
    APTSculptVolume* GetBuildCanvasFor(const APTPlayerState* PS) const;
    /** [server] Voto (1..5) a la escultura en exhibición. Se puede cambiar hasta que pasa a la próxima. */
    void HandleBuildBattleVote(APTPlayerState* Voter, int32 Stars);
    /** DEV: agrega N jugadores bot (con cuerpo, como los del modo local) que esculpen figuras y votan al azar. */
    void AddBuildBattleBots(int32 Count);
    /** DEV (comando PTBBDemo): este jugador también esculpe solo, igual que los bots (para ver una corrida entera). */
    void SetBuildBattleDemoPlayer(APTPlayerState* PS) { BuildDemoPlayer = PS; }
    /** DEV: alterna Clásico/Build Battle. Aplica a la próxima partida (o ya, si está esperando / terminó). */
    EPTGameType ToggleGameType();

public:
    /** [server] Manda la palabra secreta actual a TODOS los dev-espectadores (para verla al espectar
     *  al escultor). Word vacío = limpiar. */
    void SendSpectateWordToSpectators(const FString& Word);
    /** [server] Si el turno está en Dibujo, manda la palabra actual a UN controller (cuando pasa a
     *  espectador a mitad de turno). */
    void SendCurrentSpectateWordTo(class APTSculptPlayerController* PC);

protected:
    virtual void InitGameState() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void BeginPlay() override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void HandleSeamlessTravelPlayer(AController*& C) override;
    virtual void Logout(AController* Exiting) override;

private:
    FTimerHandle PhaseTimer;
    FTimerHandle RevealTimer;
    FTimerHandle StartDelayTimer;   // arranque diferido del primer turno (ver StartDelay)
    FTimerHandle PropMapLoadTimer;  // publica el id del mapa de props en el GameState (diferido)

    // Publica MatchMapModId en el GameState (server) para que cada máquina cargue su copia local.
    void LoadPropMapEnvironment();
    bool         bStartScheduled = false; // evita re-agendar el arranque en cada CheckStart
    bool         bLocalStartRequested = false; // modo local: el VIP tocó "Empezar" en su celular
    int32        NextPhonePlayerId = 1000;     // PlayerId de los jugadores de celular (no chocan con los reales)
    // Da personaje a los jugadores del celular que no tienen (respetando el tope de audiencia).
    void EnsurePartyBots();
    APawn* SpawnPartyBot(APTPlayerState* PS);
    void DestroyPartyBot(APTPlayerState* PS);

    // Pool de skins para los bots (cabeza horneada + PNG del cuerpo). Se arma una vez por partida y se
    // reparte como una "bolsa": no se repite una skin hasta haber usado todas.
    struct FBotSkin { TArray<uint8> HeadBaked; TArray<uint8> BodyPNG; };
    TArray<FBotSkin> BotSkinPool;
    TArray<int32>    BotSkinBag;
    bool             bBotSkinPoolBuilt = false;
    void BuildBotSkinPool();
    const FBotSkin* NextBotSkin();
    class UPTLocalPartySubsystem* LocalParty() const; // null si no es modo local

    // Estado del turno (solo servidor). La palabra real vive acá (todas sus traducciones), jamás
    // se replica: a los clientes solo les llegan las MÁSCARAS.
    FPTWordEntry           CurrentWord;
    TArray<FPTWordEntry>   CurrentChoices; // las 3 opciones, cada una con todas sus traducciones
    int32                  TurnsLeftThisRound = 0; // turnos que faltan para cerrar la ronda

    // Palabras ya ESCULPIDAS en esta partida (la que se eligió de las 3, por su texto primario). No
    // vuelven a ofrecerse en ninguna ronda → cada palabra se modela una sola vez por partida. Las
    // opciones NO elegidas sí pueden reaparecer. Se limpia al empezar cada partida (StartGame).
    TSet<FString>          UsedWordKeys;

    // #3 — Puntaje guardado por SteamID: al irse un jugador (Logout) se guarda su GameScore, y si
    // RECONECTA (PostLogin) se le restaura, para que no pierda los puntos por una desconexión.
    TMap<FString, int32> SavedScores;

    // Revelado progresivo POR IDIOMA: cada traducción tiene letras distintas, así que lleva su
    // propia cola y su propio set de posiciones ya reveladas. Un elemento por idioma del CSV.
    TArray<TArray<int32>> RevealQueues;
    TArray<TSet<int32>>   RevealedPos;
    void ScheduleLetterReveals();
    void RevealNextLetter();
    // Envía a los clientes la máscara de CADA idioma y refresca la local del host.
    void PushMaskedWords();

    APTSculptGameState* GS() const;
    TArray<APTPlayerState*> GetActivePlayers() const;
    void ResetSculpture();    // al EMPEZAR el turno: el cubo reaparece con rebote (0→1)
    void CollapseSculpture();  // al TERMINAR el turno: el cubo colapsa (1→0) y borra la escultura
    // Espera entre el colapso (fin de turno) y el arranque del nuevo turno (para que el colapso se vea
    // completo antes de crecer). Mantener >= TurnCollapseTime del BP_SculptVolume.
    UPROPERTY(EditDefaultsOnly, Category="Game") float TurnCollapseDelay = 0.4f;

    // Devuelve las palabras (solo texto) elegibles según MatchSettings: filtra el banco (o las
    // CustomWords si bUseCustomWords) por categorías y dificultades activas. Si el filtro deja
    // el pool vacío, cae a todas las palabras de la fuente (para no romper la partida).
    TArray<FPTWordEntry> BuildEligibleWordPool() const;

    // En Lvl-01 se esculpe en un vacío sin piso: cada jugador arranca volando (no caminando).
    // Se llama server-side tras RestartPlayer para que la copia autoritativa arranque en vuelo
    // (que no caiga mientras replica). El bFlying local para el input lo activa el cliente en
    // APTSculptPlayerController::AcknowledgePossession. Solo pasa acá; el lobby camina.
    void StartPawnFlying(AController* C) const;

    void CheckStart();          // arranca si hay suficientes jugadores
    void StartGame();           // resetea puntajes/rondas y arranca el primer turno
    void StartChoosingPhase();  // elige escultor + 3 palabras
    void AutoChooseWord();      // si el escultor no elige a tiempo
    void BeginDrawing(int32 ChoiceIndex);
    void EndTurn();             // revela y agenda el próximo turno
    void AdvanceTurn();         // avanza ronda/turno o termina la partida
    void EndGame();             // fase GameOver: anuncia ganador y espera decisión del host
    int32 AwardGuessPoints(APTPlayerState* Guesser); // suma puntos al que adivina + al escultor; devuelve los del que adivina
    // Tapa con ••• cualquier traducción de la palabra actual dentro del texto (anti-spoiler del chat).
    FString MaskSpoilers(const FString& Text) const;
    void GoToWaiting();         // no hay suficientes jugadores

    // Máscara de una palabra: sin revelar = todo "_"; con Revealed = esas posiciones en MAYÚSCULA.
    static FString MaskWord(const FString& Word, const TSet<int32>* Revealed = nullptr);
    // Programa el revelado de una palabra: llena Queue con las posiciones a revelar (fracción).
    static void ScheduleRevealsFor(const FString& Word, float Fraction, TArray<int32>& OutQueue);
    static FString Normalize(const FString& In); // minúsculas, sin tildes, trim
    void SeedDefaultWords();

    // ── Build Battle (estado del server) ──
    struct FBuildEntry
    {
        TWeakObjectPtr<APTPlayerState>  Author;
        TWeakObjectPtr<APTSculptVolume> Canvas;
        FString                         AuthorName; // por si se va antes de los resultados
        TMap<TWeakObjectPtr<APTPlayerState>, int32> Votes; // votante → estrellas (1..5). No se replica.
    };
    TArray<FBuildEntry> BuildEntries;  // una por jugador que esculpe en esta ronda
    TArray<int32>       ShowcaseOrder; // índices de BuildEntries en orden de exhibición (al azar)
    int32               ShowcasePos = 0;
    double              ShowcaseStartTime = 0.0; // tiempo de server en que apareció la escultura actual

    void StartBuildRound();      // palabra nueva + un lienzo por jugador + reloj de esculpido
    void EndBuildPhase();        // se acabó el tiempo: arma el orden de exhibición
    void ShowCurrentSculpture(); // exhibe ShowcaseOrder[ShowcasePos] y abre la votación
    void AdvanceShowcase();
    void RevealRoundResults();   // cuenta votos, suma puntos y publica la tabla
    void AdvanceBuildRound();    // próxima ronda o fin de la partida
    void DestroyBuildCanvases();
    APTSculptVolume* SpawnBuildCanvas(APTPlayerState* PS);
    /** Votantes que todavía no votaron la escultura E (todos los activos menos el autor). */
    int32 CountPendingVoters(const FBuildEntry& E) const;
    /** Si ya no falta nadie por votar la escultura en exhibición, acorta su reloj (ver ShowcaseAllVotedGrace). */
    void MaybeShortenShowcase();

    // Bots de prueba (DEV): PlayerStates sin controller, como los jugadores del celular.
    TArray<TWeakObjectPtr<APTPlayerState>> BuildBots;
    TWeakObjectPtr<APTPlayerState> BuildDemoPlayer; // PTBBDemo: un jugador real que esculpe solo
    // Cada bot arma una figura (muñeco de nieve, árbol, hongo...) de a partes a lo largo del tiempo de esculpido.
    struct FBuildBotPlan { int32 Figure = 0; int32 Step = 0; float Scale = 1.f; };
    TMap<TWeakObjectPtr<APTPlayerState>, FBuildBotPlan> BuildBotPlans;
    double       BuildRoundStartTime = 0.0;
    FTimerHandle BuildBotTimer;
    int32        NextBuildBotId = 1;
    void BuildBotsSculptTick();
};
