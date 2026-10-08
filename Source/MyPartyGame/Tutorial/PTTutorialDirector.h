// Director del TUTORIAL de Sculpi (práctica de esculpido, sin conexión, en el mapa de juego).
//
// Lo crea APTSculptGameMode cuando UPTGameInstance::bTutorialMode está prendido (ver EnterTutorial).
// El jugador esculpe sin turnos ni reloj; este actor:
//  · trae a Sculpi (un personaje del juego con la skin "Frank Suit", Content/Tutorial/Sculpi.skin)
//    que flota al lado de la cámara y habla en el cuadro de diálogo (UPTTutorialWidget);
//  · lleva las lecciones en orden (mirar, moverse, volar, agregar, tamaño, formas, rotar, aplastar,
//    trazos rectos, Alt, borrar, deshacer, borrar todo, pintar, ojos) y las 3 palabras (iglú con guía,
//    hongo con media guía, perro libre);
//  · dibuja las GUÍAS FANTASMA (las mismas formas del sello, transparentes) y mide cuánto se rellenó
//    consultando el volumen (SampleWorldDensity) en puntos dentro de cada guía;
//  · al final saca la foto del perro (cuenta 3-2-1, captura sin interfaz), la pone en un marco tipo
//    polaroid ("Mi perro" + logo) y la manda a las capturas de Steam (si no hay Steam, a un PNG).
// Saltar: menú de pausa → "Saltar tutorial". Terminado o saltado, queda marcado como hecho.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PTSculptVolume.h"
#include "PTTutorialDirector.generated.h"

class APTSculptPlayerController;
class APTLobbyCharacter;
class UPTTutorialWidget;
class UProceduralMeshComponent;
class UMaterialInstanceDynamic;
class USoundBase;
class ACameraActor;

UENUM()
enum class EPTTutStep : uint8
{
    Intro, Look, Move, FlyUp, FlyDown,
    Add, Size, Shapes, Rotate, Squash, Lines, Alt,
    Erase, Undo, Clear, Paint, Eyes,
    Iglu, Hongo, Perro, Photo, End
};

/** Una parte de una guía: una forma de sello (como las que pone el jugador). */
struct FPTGhostPart
{
    EPTStampShape Shape = EPTStampShape::Sphere;
    FVector  Center = FVector::ZeroVector;
    float    Size = 200.f;                 // tamaño del sello (como StampSize)
    FVector  Scale = FVector::OneVector;   // escala no uniforme (como StampScale)
    FRotator Rot = FRotator::ZeroRotator;
};

/** Una guía (una o varias partes). Visible = se dibuja; si no, solo se mide (ej: el sombrero del hongo). */
struct FPTGhost
{
    TArray<FPTGhostPart> Parts;
    TArray<FVector> Samples;   // puntos dentro de la guía (para medir el relleno)
    TArray<UPrimitiveComponent*> Meshes;
    UMaterialInstanceDynamic* OverlayMID = nullptr;
    UMaterialInstanceDynamic* MID = nullptr;
    FLinearColor Color = FLinearColor(0.3f, 0.75f, 1.f, 1.f);
    bool bVisible = true;
    float Fill = 0.f;          // última medición (0..1)
};

UCLASS()
class MYPARTYGAME_API APTTutorialDirector : public AActor
{
    GENERATED_BODY()

public:
    APTTutorialDirector();
    /** DEV: saltar a una lección (comando PTTutStep N). */
    void DevJumpTo(int32 StepIndex);
    void DevFillGhosts(); // DEV: comando PTTutFill

    // ── Editables en un BP hijo (BP_TutorialDirector) si se quiere ajustar ──
    /** Skin de Sculpi (paquete de skin del Workshop, relativo a Content/). */
    UPROPERTY(EditAnywhere, Category="Tutorial") FString SculpiSkinFile = TEXT("Tutorial/Sculpi.skin");
    UPROPERTY(EditAnywhere, Category="Tutorial") TArray<TSoftObjectPtr<USoundBase>> VoiceSounds;
    UPROPERTY(EditAnywhere, Category="Tutorial") TSoftObjectPtr<USoundBase> SuccessSound;
    UPROPERTY(EditAnywhere, Category="Tutorial") TSoftObjectPtr<USoundBase> CountdownSound;
    UPROPERTY(EditAnywhere, Category="Tutorial") TSoftObjectPtr<USoundBase> ShutterSound;
    UPROPERTY(EditAnywhere, Category="Tutorial") TSoftObjectPtr<class UTexture2D> LogoTexture;
    /** Material de las guías fantasma (translúcido, se ve la arcilla adentro). Parámetro "Color". */
    UPROPERTY(EditAnywhere, Category="Tutorial") TSoftObjectPtr<class UMaterialInterface> GhostMaterial;
    /** Si GhostMaterial no existe, se prueban estos en orden. */
    UPROPERTY(EditAnywhere, Category="Tutorial") TArray<TSoftObjectPtr<class UMaterialInterface>> GhostMaterialFallbacks;
    /** Multiplica el "Color" que recibe el material de las guías (subilo si tu material es aditivo). */
    UPROPERTY(EditAnywhere, Category="Tutorial") float GhostIntensity = 1.f;
    /** Capa opcional encima de la guía (SetOverlayMaterial). */
    UPROPERTY(EditAnywhere, Category="Tutorial") TSoftObjectPtr<class UMaterialInterface> GhostOverlayMaterial;
    /** Malla de los aros de las lecciones de movimiento. */
    UPROPERTY(EditAnywhere, Category="Tutorial") TSoftObjectPtr<class UStaticMesh> RingMesh;
    /** Cuánto hay que rellenar cada guía (0..1). */
    UPROPERTY(EditAnywhere, Category="Tutorial") float LessonFill = 0.6f;
    UPROPERTY(EditAnywhere, Category="Tutorial") float WordFill = 0.7f;
    /** Segundos para el perro (la palabra libre). */
    UPROPERTY(EditAnywhere, Category="Tutorial") float PerroSeconds = 90.f;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void Tick(float DeltaSeconds) override;

private:
    // ── Flujo ──
    EPTTutStep Step = EPTTutStep::Intro;
    float StepTime = 0.f;
    bool  bStepDone = false;     // ya lo logró: espera un momento y pasa a la siguiente
    float DoneTimer = 0.f;
    int32 LineIndex = 0;         // para pasos con varias frases seguidas
    float LineWait = 0.f;
    bool  bStarted = false;
    bool  bExiting = false;

    void EnterStep(EPTTutStep S);
    void NextStep();
    void TickStep(float Dt);
    void CompleteStep();
    void Say(const TCHAR* Key);
    void SetHintsFor(std::initializer_list<FName> Actions);
    FText KeyText(FName Action) const;

    // ── Estado observado del jugador (por lección) ──
    float AccYaw = 0.f, LastYaw = 0.f;
    float SizeStart = 0.f; bool bSizeBigger = false, bSizeSmaller = false;
    bool  bSawRotate = false, bSawScale = false, bSawAxisStroke = false, bSawPicker = false, bSawSave = false;
    bool  bUndoDone = false, bClearDone = false;
    int32 DetailStart = 0, EyesStart = 0;
    FVector RingA = FVector::ZeroVector, RingB = FVector::ZeroVector, RingC = FVector::ZeroVector;
    float MoveStartDist = 1.f;

    // ── Mundo ──
    TWeakObjectPtr<APTSculptPlayerController> PC;
    TWeakObjectPtr<APTSculptVolume> Volume;
    UPROPERTY() APTLobbyCharacter* Sculpi = nullptr;
    UPROPERTY() UPTTutorialWidget* Widget = nullptr;
    UPROPERTY() USceneComponent* Root = nullptr;
    UPROPERTY() TArray<UObject*> KeepAlive; // mallas / materiales de las guías (GC)
    TArray<FPTGhost> Ghosts;
    FVector CanvasCenter = FVector::ZeroVector, CanvasExt = FVector(480.f);
    float FloorZ = 0.f;
    FVector Fwd = FVector::ForwardVector, Right = FVector::RightVector; // orientación "hacia el jugador"

    bool  SetupWorld();
    void  SpawnSculpi();
    void  TickSculpi(float Dt);
    void  ClearGhosts();
    int32 AddGhost(const TArray<FPTGhostPart>& Parts, const FLinearColor& Color, bool bVisible = true);
    float MeasureFill(int32 GhostIdx);
    float MeasurePainted(const FVector& Center, float Radius) const;
    void  ClearClay();
    void  PlaceClay(EPTStampShape Shape, const FVector& Center, float Size, FVector Scale = FVector::OneVector,
                    FRotator Rot = FRotator::ZeroRotator);
    void  CaptureOrientation();
    bool  PawnNear(const FVector& P, float Horizontal, float Vertical) const;
    void  PlaySfx(const TSoftObjectPtr<USoundBase>& S, float Volume = 0.6f) const;

    // ── Perro + foto ──
    float PerroLeft = 0.f;
    int32 CountdownN = 0;
    float CountdownT = 0.f;
    bool  bShotRequested = false;
    UPROPERTY() ACameraActor* PhotoCam = nullptr;
    UPROPERTY() class UTexture2D* PhotoTex = nullptr;
    FDelegateHandle ShotDelegate;
    void  StartPhoto();
    void  FramePhotoCamera();
    void  SetPhotoHidden(bool bHide);
    void  OnScreenshot(int32 W, int32 H, const TArray<FColor>& Pixels);
    void  SaveFramedPhoto();

    void  OnSkip();
    void  OnDonePerro();
    void  OnContinue();
    void  Finish();
    void  BindPCEvents();
    FDelegateHandle UndoH, ClearH, SaveH;
};
