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

    /** true si la herramienta actual del modo G usa formas (Add/Erase/Paint; Ojos no). */
    bool HeadToolUsesShapes() const
    {
        return !bHeadEyesTool && (HeadEditMode == EPTEditMode::Add
                              || HeadEditMode == EPTEditMode::Erase
                              || HeadEditMode == EPTEditMode::Paint);
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
    int32 DefaultMaxPlayers = 8;

    // ── Esculpir cabeza custom (tecla G) ────────────────────────────────────
    // BP con el ClayMaterial asignado y un BoundsBox chico (tamaño cabeza). Se spawnea al entrar.
    UPROPERTY(EditAnywhere, Category="Head")
    TSubclassOf<APTSculptVolume> HeadVolumeClass;

    // Encuadre del "banco de esculpido": distancia adelante del personaje, altura, y distancia de cámara.
    UPROPERTY(EditAnywhere, Category="Head") float HeadFrontDistance = 140.f;
    UPROPERTY(EditAnywhere, Category="Head") float HeadUpOffset      = 60.f;
    UPROPERTY(EditAnywhere, Category="Head") float HeadCamDistance   = 220.f;
    // Radio de la esfera de sculpt (donde caen los stamps si el raycast no pega en la arcilla).
    UPROPERTY(EditAnywhere, Category="Head") float HeadRadius        = 55.f;
    // Velocidad de órbita de la cámara con WASD (grados/seg).
    UPROPERTY(EditAnywhere, Category="Head") float HeadOrbitSpeed    = 100.f;
    // Tamaño del pincel (se ajusta con la rueda del mouse).
    UPROPERTY(EditAnywhere, Category="Head") float HeadBrushSize     = 22.f;
    // Color de pintura actual (por ahora fijo/piel; el selector de color va después).
    UPROPERTY(EditAnywhere, Category="Head") FLinearColor HeadPaintColor = FLinearColor(0.95f, 0.78f, 0.66f);

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
    UPROPERTY(EditAnywhere, Category="Head") UStaticMesh* HeadPreviewMeshAdd   = nullptr;
    UPROPERTY(EditAnywhere, Category="Head") UStaticMesh* HeadPreviewMeshErase = nullptr;
    UPROPERTY(EditAnywhere, Category="Head") float        HeadPreviewMeshBaseSize = 100.f;

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
    UPROPERTY() ACameraActor*    HeadCam    = nullptr;
    bool bHeadSculptMode = false;
    void EnterHeadSculpt();
    void ExitHeadSculpt();

    /** Hotbar del modo G (parent = PTHeadSculptHUDWidget). Asignar en BP_LobbyPlayerController.
     *  Se crea al entrar al modo cabeza y se saca al salir. */
    UPROPERTY(EditAnywhere, Category="Lobby")
    TSubclassOf<class UPTHeadSculptHUDWidget> HeadHUDClass;

    UPROPERTY() class UPTHeadSculptHUDWidget* HeadHUD = nullptr;

    // Órbita de cámara (WASD) alrededor del volumen de cabeza.
    float HeadOrbitYaw   = 0.f;
    float HeadOrbitPitch = -8.f;
    void  UpdateHeadCam(); // reposiciona HeadCam según yaw/pitch alrededor del centro del volumen

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
    void UpdateHeadPreview(const FVector* At, const FVector& Normal); // At=nullptr → oculta el preview

    // Color picker (mantener RMB), reusando el WBP del gameplay: se abre, se tickea con el cursor
    // y al soltar se aplica el color a HeadPaintColor (esculpís/pintás con ese color).
    UPROPERTY() UUserWidget* HeadColorPicker = nullptr;
    bool bHeadColorActive = false;
    void OnHeadColorPickPressed();
    void OnHeadColorPickReleased();

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
    /** Forma efectiva: Esfera si es Ojos (colocan esferas aparte); si no, la elegida. */
    EPTStampShape EffectiveHeadShape() const { return bHeadEyesTool ? EPTStampShape::Sphere : HeadStampShape; }
    void OnHeadStampPressed();  void OnHeadStampReleased();
    void OnHeadScrollUp();      void OnHeadScrollDown();
    void OnHeadModeAdd();       void OnHeadModeErase();  void OnHeadModePaint();  void OnHeadModeEyes();
    void OnHeadCycleShape();    // TAB: cicla Esfera→Cubo→Cilindro→Cono

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
