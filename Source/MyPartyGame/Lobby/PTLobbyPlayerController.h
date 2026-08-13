// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 3 — PlayerController del lobby. Agrega el Input Mapping Context de lobby.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "../Sculpt/PTSculptVolume.h" // EPTEditMode / EPTStampShape para el modo esculpir-cabeza
#include "PTLobbyPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UPTLobbyEscapeMenuWidget;
class UPTMainMenuWidget;
class UPTLobbyHUDWidget;
class APTSculptVolume;
class ACameraActor;
class UUserWidget;
class UProceduralMeshComponent;
class UStaticMeshComponent;
class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UAnimationAsset;
struct FInputActionValue;

UCLASS()
class MYPARTYGAME_API APTLobbyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    /**
     * Hook de arranque del template: solo flippea APTGameState::LobbyState a Starting si quien
     * llama es el host. El template no decide qué pasa después de eso — eso es trabajo de cada
     * juego (ver bUseSeamlessTravel en PTLobbyGameMode).
     */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby")
    void Server_RequestStartGame();

    /** Toggle de "listo". Lo llama el botón Ready del HUD (UPTLobbyHUDWidget). */
    UFUNCTION(Server, Reliable, BlueprintCallable, Category = "Lobby")
    void Server_SetReady(bool bInReady);

    // ── Estado del modo cabeza (lo lee la hotbar del modo G para resaltar la herramienta) ──
    bool          IsHeadSculptMode()      const { return bHeadSculptMode; }
    EPTEditMode   GetHeadEditMode()       const { return HeadEditMode; }
    bool          IsHeadEyesToolActive()  const { return bHeadEyesTool; }
    bool          IsHeadColorPickerOpen() const { return bHeadColorActive; }
    EPTStampShape GetHeadStampShape()     const { return HeadStampShape; }
    bool          IsHeadStampOutside()    const { return bHeadStampOutside; }
    // Sub-modo "pintar el CUERPO" (solo con la herramienta Paint; se alterna con SHIFT). false =
    // foco en la cabeza (pinta la arcilla); true = foco en el cuerpo (pinta la piel del personaje).
    bool          IsBodyPaintMode()       const { return bBodyPaintMode; }
    /** true si estás editando un SLOT DE CUERPO (solo pintar; sin volumen de cabeza). Para que el
     *  HUD muestre únicamente la herramienta de pintar y colapse las demás. */
    bool          IsHeadBodyOnlyEdit()    const { return bHeadSculptBodyOnly; }
    /** true mientras se mantiene ALT en la cabeza (detalle en capa aparte). Para resaltar el hotbar. */
    bool          IsHeadSurfaceSnapActive() const { return bHeadSurfaceSnap; }
    /** true si la herramienta actual pinta (Paint en cabeza, o pintar el cuerpo). Para la barra de presupuesto. */
    bool          IsHeadPaintingTool()    const { return bHeadSculptMode && !bHeadEyesTool && (bBodyPaintMode || HeadEditMode == EPTEditMode::Paint); }
    /** Presupuesto de pintura 0..1 (peso PNG replicable / máximo permitido). */
    float         GetPaintBudget01()      const;
    bool          IsPaintBudgetFull()     const;
    /** Progreso 0..1 del "borrar todo" (Backspace mantenido). 0 durante la ventana del toque (undo). */
    float GetHeadClearHoldProgress() const
    {
        if (!bHeadClearHeld || HeadClearHoldTime <= HeadUndoTapMaxTime) return 0.f;
        return FMath::Clamp((HeadClearHoldTime - HeadUndoTapMaxTime) / FMath::Max(0.01f, HeadClearHoldDuration - HeadUndoTapMaxTime), 0.f, 1.f);
    }
    /** Segundos que faltan para borrar todo (para el contador del cuadrito). */
    float GetHeadClearHoldRemaining() const
    { return bHeadClearHeld ? FMath::Max(0.f, HeadClearHoldDuration - HeadClearHoldTime) : HeadClearHoldDuration; }

    /** true si la herramienta actual del modo G usa formas: SOLO Agregar. Borrar usa siempre esfera,
     *  Paint usa el círculo, Ojos esfera, y el pintado del cuerpo tampoco → en todos esos se colapsa
     *  la barra de formas (igual que al equipar el ojo). */
    bool HeadToolUsesShapes() const
    {
        return !bHeadEyesTool && !bBodyPaintMode && HeadEditMode == EPTEditMode::Add;
    }

protected:
    virtual void BeginPlay() override;
    virtual void SetupInputComponent() override;
    virtual void PlayerTick(float DeltaTime) override; // aplica stamps mientras se mantiene el click en modo cabeza
    // La posesión del pawn (sobre todo en clientes, que la reciben por replicación después del
    // BeginPlay) fija la vista al personaje; re-asegurar acá la cámara diorama.
    virtual void AcknowledgePossession(APawn* P) override;

    /** Le manda al servidor quién soy (nombre + idioma) y le pide las cabezas de todos.
     *  Reintenta solo si el PlayerState todavía no replicó. */
    void PushIdentityToServer();

    /** Heartbeat de sincronización de cabezas: re-pide las que falten (auto-repara carreras de carga). */
    void HeadSyncHeartbeat();
    FTimerHandle HeadSyncTimer;

    /** Asignar IMC_Lobby en el Blueprint derivado BP_LobbyPlayerController. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
    UInputMappingContext* LobbyMappingContext;

    /** Acción de Escape (crear el Input Action en el editor y asignarlo acá y en LobbyMappingContext). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
    UInputAction* EscapeMenuAction;

    /** Asignar WBP_LobbyEscapeMenu (o derivado) en el Blueprint derivado. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Lobby")
    TSubclassOf<UPTLobbyEscapeMenuWidget> EscapeMenuWidgetClass;

    void ToggleEscapeMenu(const FInputActionValue& Value);
    void OnPressedStartGame();

    // ── Cámara fija diorama (menú/lobby interactivo) ────────────────────────
    /** Tag de la ACameraActor del nivel que se usa como cámara fija compartida. */
    UPROPERTY(EditAnywhere, Category="Diorama")
    FName DioramaCameraTag = TEXT("DioramaCam");

    // ── Overlay 2D (menú diegético) ──────────────────────────────────────────
    // Sin sesión activa (NM_Standalone, recién abierto el juego): Crear/Unirse/Opciones.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
    TSubclassOf<UPTMainMenuWidget> MainMenuWidgetClass;

    // Ya en una sesión (host tras ?listen o cliente tras join): lista de jugadores + Ready.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI")
    TSubclassOf<UPTLobbyHUDWidget> LobbyHUDWidgetClass;

    // Path del propio nivel combinado (MainMenu = Lobby). Al crear sesión se autoviaja acá
    // mismo con "?listen" (self-travel, ver UPTMainMenuWidget::OnCreateSession).
    UPROPERTY(EditAnywhere, Category="UI")
    FString SelfMapPath = TEXT("/Game/Template/levels/MainMenu");

    UPROPERTY(EditAnywhere, Category="UI")
    int32 DefaultMaxPlayers = 10;

    // ── Esculpir cabeza custom (tecla G) ────────────────────────────────────
    // BP con el ClayMaterial asignado y un BoundsBox chico (tamaño cabeza). Se spawnea al entrar.
    UPROPERTY(EditAnywhere, Category="Head")
    TSubclassOf<APTSculptVolume> HeadVolumeClass;

    // Encuadre del "banco de esculpido": distancia adelante del personaje, altura, y distancia de cámara.
    UPROPERTY(EditAnywhere, Category="Head") float HeadFrontDistance = 140.f;
    UPROPERTY(EditAnywhere, Category="Head") float HeadUpOffset      = 60.f;
    UPROPERTY(EditAnywhere, Category="Head") float HeadCamDistance   = 220.f;
    // Encuadre al pintar el CUERPO (SHIFT en modo Paint): distancia de cámara y píxeles de brocha.
    UPROPERTY(EditAnywhere, Category="Head") float BodyCamDistance   = 400.f;
    UPROPERTY(EditAnywhere, Category="Head") float BodyPaintBrushScale = 2.f; // px = HeadBrushSize * esto
    // Radio de la esfera de sculpt (donde caen los stamps si el raycast no pega en la arcilla).
    UPROPERTY(EditAnywhere, Category="Head") float HeadRadius        = 55.f;
    // Velocidad de órbita de la cámara con WASD (grados/seg).
    UPROPERTY(EditAnywhere, Category="Head") float HeadOrbitSpeed    = 100.f;
    // Tamaño del pincel (se ajusta con la rueda del mouse).
    UPROPERTY(EditAnywhere, Category="Head") float HeadBrushSize     = 22.f;
    // Color de pintura actual (default BLANCO; se cambia con el color picker).
    UPROPERTY(EditAnywhere, Category="Head") FLinearColor HeadPaintColor = FLinearColor::White;

    // Materiales de preview de la brocha (como el gameplay): el fantasma que sigue al cursor.
    // Asignar los mismos del sculpt de gameplay (Add = arcilla, Erase = rojo/hueco).
    UPROPERTY(EditAnywhere, Category="Head") UMaterialInterface* HeadPreviewMatAdd   = nullptr;
    UPROPERTY(EditAnywhere, Category="Head") UMaterialInterface* HeadPreviewMatErase = nullptr;
    // Preview del modo Paint: se apoya SOBRE la malla (como el gameplay). Sólo este modo lo usa.
    UPROPERTY(EditAnywhere, Category="Head") UStaticMesh*        HeadPreviewMeshPaint = nullptr;
    UPROPERTY(EditAnywhere, Category="Head") UMaterialInterface* HeadPreviewMatPaint  = nullptr;
    // Preview del modo OJOS (tecla 4): mesh + material del fantasma. Si el mesh es null, esfera.
    UPROPERTY(EditAnywhere, Category="Head") UStaticMesh*        HeadEyePreviewMesh   = nullptr;
    UPROPERTY(EditAnywhere, Category="Head") UMaterialInterface* HeadEyePreviewMat    = nullptr;

    // Meshes propios para el preview de cada tool (opcional). Si se asignan, se usan en vez del
    // mesh procedural de la brocha, escalados al tamaño del pincel (base = HeadPreviewMeshBaseSize).
    UPROPERTY(EditAnywhere, Category="Head") UStaticMesh* HeadPreviewMeshErase = nullptr;
    UPROPERTY(EditAnywhere, Category="Head") float        HeadPreviewMeshBaseSize = 100.f;

    // Preview de AGREGAR: un mesh por FORMA (asigná el que quieras a cada uno). Si alguno queda null,
    // esa forma usa el preview procedural. HeadPreviewMeshAdd = fallback general si no hay por-forma.
    UPROPERTY(EditAnywhere, Category="Head") UStaticMesh* HeadPreviewMeshAdd      = nullptr;
    UPROPERTY(EditAnywhere, Category="Head|Shapes") UStaticMesh* HeadShapeMeshSphere   = nullptr;
    UPROPERTY(EditAnywhere, Category="Head|Shapes") UStaticMesh* HeadShapeMeshCube     = nullptr;
    UPROPERTY(EditAnywhere, Category="Head|Shapes") UStaticMesh* HeadShapeMeshCylinder = nullptr;
    UPROPERTY(EditAnywhere, Category="Head|Shapes") UStaticMesh* HeadShapeMeshCone     = nullptr;

    // Animación de pose recta a forzar mientras esculpís (si null, usa la del BP_LobbyCharacter).
    UPROPERTY(EditAnywhere, Category="Head") UAnimationAsset* HeadSculptPoseAnim = nullptr;

    // WBP del color picker (el mismo del gameplay, WBP_ColorPicker). Se abre manteniendo RMB.
    UPROPERTY(EditAnywhere, Category="Head") TSubclassOf<UUserWidget> HeadColorPickerClass;

    // Entra/sale del modo esculpir-cabeza: spawnea el volumen + cámara, congela el movimiento.
    void ToggleHeadSculptMode();

private:
    UPROPERTY() UPTLobbyEscapeMenuWidget* EscapeMenuWidget = nullptr;

    // Fija la vista a la cámara diorama y bloquea el look (el mouse es para la UI). Reintenta
    // hasta encontrar la cámara (puede no estar lista en BeginPlay / al poseer el pawn).
    void SetupDioramaView();
    bool bDioramaReady = false;
    FTimerHandle DioramaRetry;

    // Input diegético del lobby: GameAndUI + cursor visible + sin captura de mouse (el look
    // está bloqueado, así que el mouse es 100% para la UI). Se re-aplica al cerrar el menú de
    // Escape, que si no dejaría el input en GameOnly (correcto para el juego FPS, no para el lobby).
    void ApplyDioramaInputMode();

    // Sin sesión (todavía no viajamos) → overlay Crear/Unirse; en sesión → HUD de lobby (Ready).
    void ShowLobbyOverlay();

    // Estado del modo esculpir-cabeza.
    UPROPERTY() APTSculptVolume* HeadVolume = nullptr;
    // Material dinámico que muestrea la textura 2D de pintura de la cabeza (proyección esférica). Se
    // aplica a la malla viva del volumen para ver la pintura mientras esculpís/pintás.
    UPROPERTY() UMaterialInstanceDynamic* HeadPaintMID = nullptr;
    UPROPERTY() ACameraActor*    HeadCam    = nullptr;
    bool bHeadSculptMode = false;
    void EnterHeadSculpt();
    void ExitHeadSculpt(bool bSaveChanges = true); // true=guardar+equipar; false=descartar (restaura equipado)
public:
    /** Confirmar la edición (Enter / botón): guarda en el slot, equipa y vuelve al Locker. */
    void ConfirmHeadEdit();
    /** Pedir salir de la edición (Escape / botón Back): muestra el popup de guardar/descartar. */
    void RequestHeadBack();
    /** Resolución del popup: true=guardar (como Confirmar), false=descartar y volver al Locker. */
    void ResolveHeadBack(bool bSave);
protected:

    // ── Locker (casillero): reemplaza a la tecla G ──
    // Slot que se está editando (-1 = ninguno). Al confirmar se guarda en ese slot + se equipa.
    int32 EditingHeadSlot = -1;
    int32 EditingBodySlot = -1;
    bool  bHeadSculptBodyOnly = false; // modo edición de un slot de CUERPO (sin volumen de cabeza)
public:
    /** Abre el Locker: colapsa el menú, lleva la cámara a la vista lateral (blend) y muestra el widget. */
    void OpenLocker();
    /** Cierra el Locker y vuelve al menú principal (cámara diorama). */
    void CloseLocker();
    bool IsLockerOpen() const { return bLockerOpen; }
    /** Equipa un slot (aplica al personaje y replica solo lo equipado). */
    void EquipHeadSlot(int32 Idx);
    void EquipBodySlot(int32 Idx);

    // Preview local (hover en el locker): muestra la combinación sin equipar; revert vuelve a lo equipado.
    void PreviewLookSlot(int32 Index, bool bHead);
    void RevertLookPreview();
    /** Entra a crear/editar un slot de cabeza o de cuerpo. */
    void EnterHeadSculptForSlot(int32 Idx);
    void EnterBodyPaintForSlot(int32 Idx);
protected:
    /** WBP del Locker (deriva de UPTLockerWidget). Asignar en BP_LobbyPlayerController. */
    UPROPERTY(EditAnywhere, Category="Lobby") TSubclassOf<class UPTLockerWidget> LockerWidgetClass;
    UPROPERTY() class UPTLockerWidget* LockerWidget = nullptr;

    // Sonidos de esculpido (compartidos con el gameplay; assets en el GameInstance). Creado lazy.
    UPROPERTY() class UPTSculptSoundComponent* SculptSounds = nullptr;

    // ── Cámara del Locker (sigue la POSICIÓN del personaje, pero NO rota: dirección fija al mundo) ──
    // El personaje rota/se mueve; la cámara solo se desplaza con él manteniendo un offset y una rotación
    // FIJOS EN EL MUNDO. Editá estos valores para reencuadrar.
    UPROPERTY() ACameraActor* LockerCam = nullptr;
    // Offset en MUNDO respecto de la posición del personaje (la cámara se para en CharPos + este offset).
    UPROPERTY(EditAnywhere, Category="Locker") FVector  LockerCamOffset   = FVector(-260.f, 130.f, 60.f);
    // Rotación FIJA de la cámara en el mundo (no depende de hacia dónde mire el personaje).
    UPROPERTY(EditAnywhere, Category="Locker") FRotator LockerCamRotation = FRotator(-5.f, 25.f, 0.f);
    UPROPERTY(EditAnywhere, Category="Locker") float    LockerCamBlend    = 0.6f;

    bool bLockerOpen = false;
    bool bReturnToLockerAfterEdit = false; // al confirmar la edición, volver a la vista Locker (no al menú)
    bool bDiscardPopupOpen = false;        // el popup guardar/descartar está visible (edición)
    void SetupLockerCam();  // crea y ubica la LockerCam
    void UpdateLockerCam(); // la reubica siguiendo al personaje (dirección fija)

    /** Hotbar del modo G (parent = PTHeadSculptHUDWidget). Asignar en BP_LobbyPlayerController.
     *  Se crea al entrar al modo cabeza y se saca al salir. */
    UPROPERTY(EditAnywhere, Category="Lobby")
    TSubclassOf<class UPTHeadSculptHUDWidget> HeadHUDClass;

    UPROPERTY() class UPTHeadSculptHUDWidget* HeadHUD = nullptr;

    // Órbita de cámara (WASD) alrededor del volumen de cabeza.
    float HeadOrbitYaw   = 0.f;
    float HeadOrbitPitch = -8.f;
    void  UpdateHeadCam(); // recalcula el DESTINO de la cámara (la cámara se mueve hacia él suave)

    // Blend de cámara: UpdateHeadCam setea el destino; el tick interpola la cámara hacia él, así el
    // cambio cabeza↔cuerpo (y la órbita WASD) es suave en vez de un salto.
    FVector  DesiredCamLoc = FVector::ZeroVector;
    FRotator DesiredCamRot = FRotator::ZeroRotator;
    bool     bHeadCamInit  = false; // false = al entrar/primer frame se coloca de una (sin blend)
    UPROPERTY(EditAnywhere, Category="Head") float HeadCamBlendSpeed = 8.f;

    // Input mode del modo cabeza: GameAndUI + cursor pero con CaptureDuringMouseDown, así el LMB
    // llega al esculpido (el modo diorama usa NoCapture y los clicks solo iban a la UI).
    void ApplyHeadSculptInputMode();

    // Preview de la brocha que sigue al cursor (como el gameplay): actor con un ProcMesh (forma
    // procedural del sello) + un StaticMesh (mesh propio opcional), según el modo (Add / Erase).
    // Paint no muestra preview 3D.
    UPROPERTY() AActor*                   HeadPreviewActor  = nullptr;
    UPROPERTY() UProceduralMeshComponent* HeadPreviewMesh   = nullptr;
    UPROPERTY() UStaticMeshComponent*     HeadPreviewStatic = nullptr;
    float       HeadPreviewSize = -1.f;                 // cache para reconstruir sólo si cambia
    EPTEditMode HeadPreviewMode = EPTEditMode::Smooth;  // idem (valor inicial != Add/Erase)
    EPTStampShape HeadPreviewShapeCached = EPTStampShape::TriPrism; // idem para la forma (valor != esfera)
    float HeadPreviewGlow = 0.f; // brillo del preview: 0 en reposo, sube a 1 mientras esculpís (Add)
    // Presupuesto de pintura: la pintura (cabeza + cuerpo) no puede pasar de este peso replicable.
    // Un chunk = 8 KB; el server corta ~a los 50 chunks. Dejamos margen para la geometría.
    UPROPERTY(EditAnywhere, Category="Head") int32 MaxPaintChunks = 45;
    float PaintBudgetAccum = 0.f; // throttle del recálculo del peso mientras pintás
    void UpdateHeadPreview(const FVector* At, const FVector& Normal); // At=nullptr → oculta el preview

    // Color picker (mantener RMB), reusando el WBP del gameplay: se abre, se tickea con el cursor
    // y al soltar se aplica el color a HeadPaintColor (esculpís/pintás con ese color).
    UPROPERTY() UUserWidget* HeadColorPicker = nullptr;
    bool bHeadColorActive = false;
    void OnHeadColorPickPressed();
    void OnHeadColorPickReleased();
    void OnHeadColorSave(); // E: guarda el color actual en el anillo del picker

    // Overlay del lobby activo (MainMenu o LobbyHUD): se colapsa mientras esculpís la cabeza
    // para que el mouse no lo agarre la UI y llegue al esculpido (igual que el gameplay).
    UPROPERTY() UUserWidget* ActiveOverlay = nullptr;

    // Input de esculpido (solo activo en modo cabeza). Igual que el gameplay: LMB esculpe en el
    // modo actual; 1/2/3 cambian el modo (Add/Erase/Paint). RMB queda para el color (paso siguiente).
    bool bHeadStamping = false;
    EPTEditMode HeadEditMode = EPTEditMode::Add;
    // Forma del sello (como el gameplay): TAB cicla; al entrar en Borrar vuelve a Esfera.
    EPTStampShape HeadStampShape = EPTStampShape::Sphere;
    // true cuando la brocha quedó fuera del área de esculpido (para el icono 🚫 y para no sellar).
    bool bHeadStampOutside = false;
    /** Forma efectiva: solo Agregar usa la forma elegida. Ojos, Borrar y Paint usan Esfera/círculo. */
    EPTStampShape EffectiveHeadShape() const
    { return (bHeadEyesTool || HeadEditMode != EPTEditMode::Add) ? EPTStampShape::Sphere : HeadStampShape; }
    void OnHeadStampPressed();  void OnHeadStampReleased();
    void OnHeadScrollUp();      void OnHeadScrollDown();
    // Rotar el shape manteniendo la RUEDA del mouse y arrastrando (igual que el gameplay). Doble click
    // de rueda = reset de rotación.
    void OnHeadRotatePressed(); void OnHeadRotateReleased();
    FRotator HeadStampRotation = FRotator::ZeroRotator;
    bool     bHeadRotatingShape = false;
    float    LastHeadWheelPressTime = -10.f;
    // Para rotar EN EL LUGAR: se congela el punto del sello y se fija el cursor mientras rotás.
    float    HeadRotateCursorX = 0.f, HeadRotateCursorY = 0.f;
    FVector  HeadRotateFrozenPt = FVector::ZeroVector, HeadRotateFrozenNrm = FVector::UpVector;
    bool     bHeadRotateHasFrozen = false;
    UPROPERTY(EditAnywhere, Category="Head") float HeadShapeRotateSpeed = 0.5f;
    // Undo (BACKSPACE toque corto) / resetear (BACKSPACE mantenido HeadClearHoldDuration s). Por EVENTOS
    // (IsInputKeyDown(BackSpace) da false en este modo porque Slate consume Backspace). El contador se
    // acumula en PlayerTick — y va ANTES de los returns de color-picker/body-paint, si no, en modo
    // cuerpo nunca se ejecutaba. Backspace no tiene auto-repetición acá (verificado por log).
    void OnHeadClearPressed();  void OnHeadClearReleased();
    bool  bHeadClearHeld    = false; // true mientras se mantiene la tecla (para el HUD)
    float HeadClearHoldTime = 0.f;
    bool  bHeadClearFired   = false; // ya se disparó el reset en este mantener (evita un undo al soltar)
    UPROPERTY(EditAnywhere, Category="Head") float HeadClearHoldDuration = 3.f;   // mantener → borrar todo
    UPROPERTY(EditAnywhere, Category="Head") float HeadUndoTapMaxTime    = 0.35f; // toque → undo
    bool bHeadStrokeActive = false; // hay un trazo de geometría abierto (para el undo)
    // Orden de los trazos para el undo unificado: 0=geometría (volumen), 1=pintura cabeza, 2=pintura cuerpo.
    TArray<uint8> HeadUndoKinds;

    // ── ALT en la cabeza: detallar pegado a la superficie (como el gameplay) ──
    // Con Alt mantenido en Add, el sello se pega a la superficie de la cabeza (raymarch) para detallar
    // de cerca. Se congela un plano al 1er sello del trazo para no trepar hacia la cámara.
    bool    bHeadSurfaceSnap = false;
    void OnHeadSurfaceSnapPressed()  { bHeadSurfaceSnap = true;  }
    void OnHeadSurfaceSnapReleased() { bHeadSurfaceSnap = false; }
    FVector HeadStrokePlaneOrigin = FVector::ZeroVector;
    FVector HeadStrokePlaneNormal = FVector::ForwardVector;
    bool    bHeadStrokePlaneLocked = false;
    // Latcheado al iniciar el trazo (= Add + Alt): el trazo va a una CAPA de detalle aparte (lentes/
    // bigote/etc. que NO se fusionan con la cabeza), igual que en el gameplay.
    bool    bHeadStrokeIsDetail = false;
    void OnHeadModeAdd();       void OnHeadModeErase();  void OnHeadModePaint();  void OnHeadModeEyes();
    void OnHeadCycleShape();    // TAB: cicla Esfera→Cubo→Cilindro→Cono
    // SHIFT (solo en Paint): alterna foco cabeza (arcilla) ↔ cuerpo (piel del personaje).
    bool bBodyPaintMode = false;
    void OnHeadToggleBodyPaint();
    // Para trazos continuos al pintar el cuerpo: se interpola el movimiento del cursor EN PANTALLA
    // (cada paso hace su propio raycast) así el trazo no se corta al cruzar una costura del UV.
    FVector2D LastBodyCursor     = FVector2D::ZeroVector;
    bool      bHasLastBodyCursor = false;

    // ── Ojos (tecla 4): esferas aparte que se hornean junto a la cabeza ─────────
    bool bHeadEyesTool = false;                 // tecla 4 activa: colocar ojos con el LMB
    TArray<FVector4> HeadEyes;                  // ojos colocados (local al volumen: XYZ centro, W radio)
    UPROPERTY() UProceduralMeshComponent* HeadEyesLiveMesh = nullptr; // muestra los ojos ya colocados
    UPROPERTY() UMaterialInstanceDynamic* HeadPreviewMID   = nullptr; // preview con color en vivo
    bool bHeadPreviewEyesCached = false;        // cache para reconstruir el preview al togglear ojos
    void PlaceEyeAtCursor();                    // coloca un ojo donde apunta el cursor (sobre la malla)
    void RebuildEyesLiveMesh();                 // reconstruye la malla viva de ojos colocados
    // Punto de mundo donde cae el sello: raycast del cursor contra la arcilla; si no pega,
    // interseca la esfera de sculpt (radio HeadRadius) alrededor del centro del volumen.
    bool GetHeadStampPoint(FVector& OutWorld, FVector& OutNormal) const;
};
