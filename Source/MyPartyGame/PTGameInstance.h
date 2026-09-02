// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 4 — GameInstance personalizado. Captura fallos de red/viaje y vuelve al MainMenu.

#pragma once
#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Engine/EngineTypes.h"
#include "PTMatchSettings.h"
#include "PTGameInstance.generated.h"

class USoundBase;
class USoundMix;
class USoundClass;
class UUserWidget;
class UPTInvitePopupWidget;

// Se dispara cuando cambia el banco de palabras elegido (elegir uno / volver a Default), para que la UI
// del lobby (Game Settings) refresque el texto "Word: ...".
DECLARE_MULTICAST_DELEGATE(FPTOnSelectedWordPackChanged);

UCLASS()
class MYPARTYGAME_API UPTGameInstance : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    /** El menú lo consume al reabrirse: devuelve el error y lo limpia. */
    UFUNCTION(BlueprintCallable, Category="Session")
    FString ConsumePendingConnectError();

    /** Deja un mensaje para mostrar al volver al menú (p.ej. "el anfitrión cerró la partida").
     *  También cancela cualquier reintento de reconexión pendiente: la partida ya no existe. */
    void SetPendingConnectError(const FString& InError);

    /** Llamar justo antes del ClientTravel exitoso a un servidor, para poder reintentar si se cae. */
    void NotifyJoinedServer(const FString& TravelURL);

    /** Punto ÚNICO para que un cliente viaje a una sesión (join manual, por código o reconexión).
     *  Anti-flood: si ya hay una conexión en curso, IGNORA el pedido (no abre una 2ª conexión al
     *  mismo host — eso es lo que apilaba conexiones duplicadas y CRASHEABA al host, ver .cpp).
     *  Arma el guard + watchdog y hace el ClientTravel. Devuelve false si se ignoró. */
    bool BeginClientTravel(const FString& TravelURL);

    // ── Reconectar a la última partida (pública o privada) desde Find Sessions ──
    // La URL de la última partida jugada (con password+nombre) sobrevive al volver al menú, así el
    // jugador puede reconectarse a mano aunque el auto-reintento ya se haya rendido. Sirve para
    // públicas Y privadas (la URL ya lleva el password, no hace falta re-ingresar el código).
    UFUNCTION(BlueprintCallable, Category="Session") bool HasLastGame() const { return !LastGameURL.IsEmpty(); }
    UFUNCTION(BlueprintCallable, Category="Session") void ReconnectToLastGame();
    UFUNCTION(BlueprintCallable, Category="Session") void ClearLastGame() { LastGameURL.Reset(); }

    // Config de partida que arma el HOST en el lobby (tiempo/rondas/revelado/categorías/
    // dificultad/CSV). El GameInstance sobrevive al seamless travel, así que el host la setea
    // en el lobby y APTSculptGameMode la lee al arrancar en Lvl-01. Es del host/servidor; a los
    // clientes se les replica un resumen aparte por el GameState (para mostrarla en el lobby).
    UPROPERTY(BlueprintReadWrite, Category="Match")
    FPTMatchSettings PendingMatchSettings;

    // Abre un diálogo nativo para elegir un .csv y carga sus palabras en
    // PendingMatchSettings.CustomWords (+ bUseCustomWords=true). Formato por línea:
    // "Palabra,Categoria,Dificultad" (Dificultad: Facil/Media/Dificil o 1/2/3; categoría y
    // dificultad opcionales). Devuelve cuántas palabras cargó (0 = canceló o archivo vacío).
    UFUNCTION(BlueprintCallable, Category="Match")
    int32 LoadCustomWordsFromCSVDialog();

    // Parsea un CSV de una ruta concreta (lo usa el diálogo; separado para testear).
    int32 LoadCustomWordsFromCSVFile(const FString& Path);

    // Vuelve al banco default (descarta las palabras del CSV).
    UFUNCTION(BlueprintCallable, Category="Match")
    void ClearCustomWords();

    // ── Bancos de palabras de la comunidad (Workshop / locales) ──
    // Selecciona un banco por su Id (de UPTWordPackSubsystem::GetPacks) → carga sus palabras en
    // PendingMatchSettings (reusa LoadCustomWordsFromCSVFile). Lo llama el host desde el lobby.
    UFUNCTION(BlueprintCallable, Category="WordPack") void SelectWordPack(const FString& PackId);
    // Vuelve al banco por defecto del juego.
    UFUNCTION(BlueprintCallable, Category="WordPack") void SelectDefaultWordBank();
    // Abre el diálogo "Examinar", elige un CSV y lo publica al Workshop como banco de palabras
    // (título = nombre del archivo). El resultado llega por UPTWordPackSubsystem::OnWordPackPublished.
    UFUNCTION(BlueprintCallable, Category="WordPack") void PublishWordPackFromDialog();
    // Título del banco elegido (vacío = default), para mostrarlo en el lobby.
    UPROPERTY(BlueprintReadOnly, Category="WordPack") FString SelectedWordPackTitle;
    // Notifica a la UI que cambió el banco elegido (ver SelectWordPack/SelectDefaultWordBank).
    FPTOnSelectedWordPackChanged OnSelectedWordPackChanged;

    // ── Modo captura / espectador dev (trailer/screenshots) ─────────────────────────────────
    // Cuando está activo, la UI del juego reemplaza LOCALMENTE los nombres de Steam por "Player N"
    // (no se replica; es solo para grabar sin exponer nicks). Lo prende/apaga el comando PTSpectate.
    UFUNCTION(BlueprintCallable, Category="Capture") void SetCaptureMode(bool bOn) { bCaptureMode = bOn; }
    UFUNCTION(BlueprintCallable, Category="Capture") bool IsCaptureMode() const { return bCaptureMode; }
    // Oculta por completo los nombres flotantes (nametags) — comando PTHideNames. Independiente del
    // modo captura: podés grabar con nombres "Player N" o directamente sin nombres.
    UFUNCTION(BlueprintCallable, Category="Capture") void SetHideNames(bool bOn) { bHideNames = bOn; }
    UFUNCTION(BlueprintCallable, Category="Capture") bool AreNamesHidden() const { return bHideNames; }
    // Devuelve el nombre a mostrar respetando el modo captura: "Player N" (índice estable por orden en
    // el PlayerArray del GameState) o, si no está en captura, cadena vacía (el llamador usa el real).
    FString GetCaptureName(const class APlayerState* PS) const;

    // ── Sonidos globales de UI ──────────────────────────────────────────────
    // Se asignan UNA vez (en BP_GameInstance o los class defaults) y se aplican a TODOS los botones
    // que ya existen, sin tocar cada uno: ApplyUIButtonSounds recorre el árbol del widget y setea el
    // Hovered/Pressed del estilo de cada UButton (Slate los reproduce solo). Los widgets raíz lo
    // llaman en su NativeConstruct.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI Sound") USoundBase* UIHoverSound = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI Sound") USoundBase* UIClickSound = nullptr;

    /** Aplica los sonidos de UI a todos los botones del widget (recursivo, entra a sub-widgets). */
    UFUNCTION(BlueprintCallable, Category="UI Sound")
    void ApplyUIButtonSounds(class UUserWidget* Root) const;

    // ── Volumen por Sound Class (Música / Efectos) ──────────────────────────
    // Asigná estos 3 assets en BP_GameInstance: un Sound Mix base y las dos Sound Classes.
    // Cada sonido del juego debe tener asignada su Class (los efectos → SFXClass; la música → MusicClass).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") USoundMix*   AudioMix   = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") USoundClass* MusicClass = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") USoundClass* SFXClass   = nullptr;

    /** Setea y aplica el volumen (0..1) guardándolo en el settings. */
    UFUNCTION(BlueprintCallable, Category="Audio") void  SetMusicVolume(float V);
    UFUNCTION(BlueprintCallable, Category="Audio") void  SetSFXVolume(float V);
    UFUNCTION(BlueprintCallable, Category="Audio") float GetMusicVolume() const;
    UFUNCTION(BlueprintCallable, Category="Audio") float GetSFXVolume() const;
    /** Aplica ambos volúmenes al mix (SetSoundMixClassOverride + push). Se llama al arrancar y en cada mapa. */
    UFUNCTION(BlueprintCallable, Category="Audio") void  ApplyAudioMix();

    // ── Música del menú/lobby (loop, persiste entre menú↔lobby, para en Lvl-01) ─────────────
    // Asigná el SoundWave/Cue en BP_GameInstance. El asset debe: (1) tener Sound Class = SC_Music
    // (para que el slider de música lo afecte), y (2) ser LOOPING. Suena en el mapa MainMenu (que es
    // a la vez menú y lobby) y se corta en Lvl-01 (ahí va otra música).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Audio") USoundBase* MenuMusic = nullptr;
    // Nombre del mapa donde suena la música (el lobby corre sobre este mismo mapa). Editable por si cambia.
    UPROPERTY(EditAnywhere, Category="Audio") FString MenuMusicMapName = TEXT("MainMenu");
    // Fade-in al arrancar la música: sube de 0 al volumen del settings en estos segundos (0 = sin fade).
    UPROPERTY(EditAnywhere, Category="Audio") float MenuMusicFadeInSeconds = 2.5f;

    // ── Reacción del personaje a la música (envelope follower) ──────────────────────────────
    // Submix dedicado para analizar la energía de la música (el personaje del lobby "baila" con ella).
    // Creá un Sound Submix en el editor y asignalo acá; la música se rutea a él por código.
    UPROPERTY(EditAnywhere, Category="Audio") class USoundSubmix* MusicAnalysisSubmix = nullptr;
    // Ganancia sobre la amplitud cruda del envelope → 0..1. Subilo si el personaje reacciona poco.
    UPROPERTY(EditAnywhere, Category="Audio") float MusicEnergyGain = 3.0f;
    // Energía actual de la música (0..1, suavizada). La lee el AnimInstance del lobby.
    UFUNCTION(BlueprintCallable, Category="Audio") float GetMusicEnergy() const { return MusicEnergy; }
    // Energía SOLO de las frecuencias ALTAS (agudos/notas altas), 0..1. Es lo que usa el baile:
    // reacciona a las notas altas y si se repiten rápido, el personaje se mueve rápido.
    UFUNCTION(BlueprintCallable, Category="Audio") float GetMusicHighEnergy() const { return MusicHighEnergy; }
    // Ganancia sobre la banda aguda (subir si reacciona poco).
    UPROPERTY(EditAnywhere, Category="Audio") float MusicHighGain = 2.0f;

    // ── Sync por BPM (baile clavado al ritmo, sin latencia de detección) ────────────────────
    // Poné el BPM real de la canción. BeatOffset alinea el primer beat / compensa latencia constante.
    // LoopSeconds = duración del track (para re-alinear la fase en cada loop); 0 = fase continua.
    UPROPERTY(EditAnywhere, Category="Audio") float MenuMusicBPM        = 120.f;
    UPROPERTY(EditAnywhere, Category="Audio") float MenuMusicBeatOffset = 0.f;
    UPROPERTY(EditAnywhere, Category="Audio") float MenuMusicLoopSeconds = 0.f;
    // Beats transcurridos desde que arrancó la música (float, fase continua para el sway). -1 si no suena.
    UFUNCTION(BlueprintCallable, Category="Audio") float GetMenuMusicBeats() const;

private:
    UPROPERTY(Transient) class UAudioComponent* MenuMusicComp = nullptr;
    float MusicEnergy = 0.f;
    float MusicHighEnergy = 0.f;
    bool  bCaptureMode = false;
    bool  bHideNames   = false;
    bool  bEnvelopeBound = false;
    bool  bSpectrumBound = false;
    double MusicStartWorldTime = 0.0; // instante (World time) en que arrancó la música, para la fase por BPM
    // Callback del envelope follower del submix de música → actualiza MusicEnergy.
    UFUNCTION() void OnMusicEnvelope(const TArray<float>& Envelope);
    // Polling del análisis espectral (banda aguda) → actualiza MusicHighEnergy. Corre por timer.
    void PollSpectrum();
    FTimerHandle SpectrumPollHandle;
    // Arranca/corta la música del menú según el mapa cargado (lo llama OnPostLoadMap).
    void UpdateMenuMusic(UWorld* World);
public:

    // ── Sonidos de esculpido (compartidos gameplay Lvl-01 + editar skins en el lobby) ──────────
    // Se asignan UNA vez acá. Los loops (Add/Erase/Paint) deben ser sonidos LOOPING. Todos 3D si se
    // asigna SculptAttenuation. Los usa UPTSculptSoundComponent en ambos controladores.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sculpt Sound") USoundBase* SndAddLoop      = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sculpt Sound") USoundBase* SndEraseLoop    = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sculpt Sound") USoundBase* SndPaintLoop    = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sculpt Sound") USoundBase* SndEyes         = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sculpt Sound") USoundBase* SndUndoSimple   = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sculpt Sound") USoundBase* SndUndoClearAll = nullptr;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sculpt Sound") USoundBase* SndColorPick    = nullptr; // elegir/guardar color
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Sculpt Sound") class USoundAttenuation* SculptAttenuation = nullptr;

    // ── Popup de invitación (F3) ────────────────────────────────────────────────
    // WBP del popup (deriva de PTInvitePopupWidget). Asignar en BP_GameInstance. Si está vacío, no
    // se muestra popup (igual llega la invitación por el overlay de Steam).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Invites") TSubclassOf<UPTInvitePopupWidget> InvitePopupClass;
    // Segundos que dura el popup en pantalla antes de auto-cerrarse (rechazando).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Invites") float InvitePopupSeconds = 12.f;

private:
    void OnPostLoadMap(UWorld* LoadedWorld); // re-aplica el mix al cargar cada nivel
    void HandleNetworkFailure(UWorld* World, UNetDriver* NetDriver,
                              ENetworkFailure::Type FailureType, const FString& ErrorString);
    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType,
                             const FString& ErrorString);
    void ReturnToMainMenuWithError(const FString& ErrorString);

    // ── Invitaciones (F3) ──
    // Llega una invitación (subsistema) → mostrar el popup Aceptar/Rechazar.
    void HandleInviteReceived(const FString& FromName);
    void ShowInvitePopup(const FString& FromName);
    // Steam pide unirse a la partida de un amigo (rich presence "connect") → viajar al host.
    void HandleJoinRequestedById(const FString& HostSteamId);
    // Tras un JoinSession exitoso, arma la URL y viaja (punto único, guard anti-flood). Funciona
    // desde cualquier lado (menú o partida) → cubre aceptar el popup en medio de una partida.
    void TravelToResolvedSession();

    UPROPERTY() UPTInvitePopupWidget* ActivePopup = nullptr;

    // Reconexión — solo aplica a clientes (el host no se reconecta a sí mismo).
    bool TryReconnect(UWorld* World);
    void DoReconnectAttempt();

    // Watchdog: si un intento de conexión queda trabado (el host tarda en soltar la conexión vieja
    // → handshake rechazado → el motor reintenta el Browse solo), lo cortamos y volvemos al menú en
    // vez de dejar que siga abriendo conexiones (que apilan y crashean al host).
    void OnConnectWatchdog();

    // UN solo intento automático, y ESPACIADO ≥ que el ConnectionTimeout del host (6s, ver
    // DefaultEngine.ini): así cuando reintenta, el host YA soltó la conexión vieja del jugador y el
    // handshake entra limpio (sin colisión → sin storm → sin crash). Si igual falla, va al menú y el
    // jugador reintenta a mano.
    static constexpr int32 MaxReconnectAttempts      = 1;
    static constexpr float ReconnectRetryDelaySeconds = 7.0f;
    // Ventana máxima para completar una conexión antes de abortar al menú. Un join sano completa en
    // ~3-4s (routing SteamSockets + handshake + cargar mapa); 8s da margen y corta el storm temprano
    // (bien por debajo del InitialConnectTimeout=12s del motor).
    static constexpr float ConnectWatchdogSeconds     = 8.0f;

    FString PendingConnectError;
    FString PendingReconnectURL;
    FString LastGameURL; // última partida jugada (persiste al volver al menú, para reconectar a mano)
    int32   ReconnectAttemptsRemaining = 0;

    bool         bClientConnectPending = false; // hay un ClientTravel a una sesión en curso (guard anti-flood)
    FTimerHandle ConnectWatchdogHandle;
};
