#include "PTTutorialDirector.h"
#include "PTTutorialWidget.h"
#include "PTTutorialChoiceWidget.h"
#include "../Sculpt/PTSculptPlayerController.h"
#include "../Sculpt/PTSculptGameState.h"
#include "../Lobby/PTPlayerState.h"
#include "../Lobby/PTLobbyCharacter.h"
#include "../Lobby/PTLockerSubsystem.h"
#include "../PTGameInstance.h"
#include "../PTGameUserSettings.h"
#include "../PTTextTable.h"
#include "../PTInputBindings.h"
#include "../PTGamepad.h"
#include "../UI/PTGamepadUINavigator.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "Sound/SoundBase.h"
#include "TextureResource.h"
#include "UnrealClient.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Engine.h" // GEngine (mensajes en pantalla de los comandos de grabación)

#if PT_WITH_STEAM
#include "steam/steam_api.h"
#include "steam/isteamscreenshots.h"
#include "steam/isteamutils.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPTTutorial, Log, All);

// ── Grabación de la jirafa (dev): PT.TutRec <1-4> para grabar una parte, PT.TutRecStop para guardarla ──
static FString QTutRecPath(int32 Lesson)
{
    // 1-4 = partes de la jirafa; 5 = el ÁRBOL del intro "adiviná la palabra".
    if (Lesson == 5) return FPaths::ProjectContentDir() / TEXT("Tutorial") / TEXT("tree.rec");
    const TCHAR* Name = Lesson == 1 ? TEXT("legs") : Lesson == 2 ? TEXT("torso")
                      : Lesson == 3 ? TEXT("neck") : Lesson == 4 ? TEXT("head") : TEXT("x");
    return FPaths::ProjectContentDir() / TEXT("Tutorial") / (FString(TEXT("giraffe_")) + Name + TEXT(".rec"));
}
static void QTutRecSerialize(FArchive& Ar, TArray<FPTTutRecStamp>& S)
{
    int32 Ver = 1; Ar << Ver;
    int32 N = S.Num(); Ar << N;
    if (Ar.IsLoading()) { S.Reset(); S.SetNum(FMath::Max(0, N)); }
    for (int32 i = 0; i < N; ++i)
    {
        FPTTutRecStamp& E = S[i];
        Ar << E.T; Ar << E.Pos; Ar << E.Shape; Ar << E.Size; Ar << E.Mode; Ar << E.Color; Ar << E.Rot; Ar << E.Scale;
    }
}
// La palabra del intro ("árbol") en el orden de idiomas de GetCurrentLanguageIndex() (ES,EN,PT,DE,FR,IT).
// La máscara usa la del idioma actual; el guess se acepta si coincide con CUALQUIERA (como en el juego).
static const TCHAR* const GTreeWords[] = { TEXT("árbol"), TEXT("tree"), TEXT("árvore"), TEXT("Baum"), TEXT("arbre"), TEXT("albero") };
static FString QTutNorm(const FString& In)
{
    FString O;
    for (TCHAR c : In)
    {
        c = FChar::ToLower(c);
        if      (c==TEXT('á')||c==TEXT('à')||c==TEXT('ä')||c==TEXT('â')) c=TEXT('a');
        else if (c==TEXT('é')||c==TEXT('è')||c==TEXT('ë')||c==TEXT('ê')) c=TEXT('e');
        else if (c==TEXT('í')||c==TEXT('ì')||c==TEXT('ï')||c==TEXT('î')) c=TEXT('i');
        else if (c==TEXT('ó')||c==TEXT('ò')||c==TEXT('ö')||c==TEXT('ô')) c=TEXT('o');
        else if (c==TEXT('ú')||c==TEXT('ù')||c==TEXT('ü')||c==TEXT('û')) c=TEXT('u');
        if (FChar::IsAlpha(c)) O.AppendChar(c);
    }
    return O;
}

static int32 GTutRecLesson = 0;
static FAutoConsoleCommandWithWorldAndArgs GTutRecStartCmd(
    TEXT("PT.TutRec"),
    TEXT("Graba los sellos de una parte de la jirafa: PT.TutRec <1=patas,2=torso,3=cuello,4=cabeza>. Esculpí y luego PT.TutRecStop."),
    FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* W)
    {
        const int32 L = Args.Num() > 0 ? FCString::Atoi(*Args[0]) : 0;
        APTSculptVolume* V = Cast<APTSculptVolume>(UGameplayStatics::GetActorOfClass(W, APTSculptVolume::StaticClass()));
        auto Scr = [](const FString& Msg, const FColor& Col)
        { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 6.f, Col, Msg); };
        if (L < 1 || L > 5)      { Scr(TEXT("[TutRec] Uso: PT.TutRec <1=patas,2=torso,3=cuello,4=cabeza,5=arbol>"), FColor::Yellow); return; }
        if (!V)                  { Scr(TEXT("[TutRec] No hay cubo de esculpido. Entrá a esculpir primero."), FColor::Red); return; }
        if (!PTTutRec::Start(V)) { Scr(TEXT("[TutRec] No se pudo iniciar (el cubo todavía no está listo)."), FColor::Red); return; }
        GTutRecLesson = L;
        const TCHAR* Names[] = { TEXT("patas"), TEXT("torso"), TEXT("cuello"), TEXT("cabeza"), TEXT("arbol") };
        Scr(FString::Printf(TEXT("[TutRec] GRABANDO '%s'. Esculpí y después: PT.TutRecStop"), Names[L - 1]), FColor::Green);
        UE_LOG(LogPTTutorial, Log, TEXT("[TutRec] Grabando parte %d."), L);
    }));
static FAutoConsoleCommand GTutRecStopCmd(
    TEXT("PT.TutRecStop"),
    TEXT("Para la grabación y la guarda en Content/Tutorial/giraffe_<parte>.rec"),
    FConsoleCommandDelegate::CreateLambda([]()
    {
        auto Scr = [](const FString& Msg, const FColor& Col)
        { if (GEngine) GEngine->AddOnScreenDebugMessage(-1, 8.f, Col, Msg); };
        if (!PTTutRec::IsRecording()) { Scr(TEXT("[TutRec] No se estaba grabando (corré PT.TutRec <1-4> primero)."), FColor::Yellow); return; }
        PTTutRec::Stop();
        TArray<FPTTutRecStamp> S = PTTutRec::Get();
        TArray<uint8> Bytes; FMemoryWriter Ar(Bytes); QTutRecSerialize(Ar, S);
        const FString Path = QTutRecPath(GTutRecLesson);
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree=*/true);
        if (FFileHelper::SaveArrayToFile(Bytes, *Path))
        {
            Scr(FString::Printf(TEXT("[TutRec] Guardados %d sellos en %s"), S.Num(), *Path), FColor::Green);
            UE_LOG(LogPTTutorial, Log, TEXT("[TutRec] Guardados %d sellos en %s"), S.Num(), *Path);
        }
        else
        {
            Scr(FString::Printf(TEXT("[TutRec] NO se pudo guardar: %s"), *Path), FColor::Red);
            UE_LOG(LogPTTutorial, Warning, TEXT("[TutRec] No se pudo guardar %s"), *Path);
        }
    }));

namespace
{
    const FLinearColor TUTD_Blue(0.3f, 0.75f, 1.f, 1.f);
    const FLinearColor TUTD_Red(1.f, 0.3f, 0.3f, 1.f);
    const FLinearColor TUTD_Yellow(1.f, 0.85f, 0.25f, 1.f);
    const FLinearColor TUTD_Green(0.36f, 0.95f, 0.5f, 1.f);

    // Nombre corto y traducido de una tecla de teclado/mouse para los carteles de Sculpi.
    FText TutKeyName(const FKey& K)
    {
        if (K == EKeys::LeftMouseButton)   return PTText::Get(TEXT("TUT_MOUSE_L"));
        if (K == EKeys::RightMouseButton)  return PTText::Get(TEXT("TUT_MOUSE_R"));
        if (K == EKeys::MiddleMouseButton) return PTText::Get(TEXT("TUT_MOUSE_M"));
        if (K == EKeys::MouseWheelAxis || K == EKeys::MouseScrollUp || K == EKeys::MouseScrollDown) return PTText::Get(TEXT("TUT_MOUSE_WHEEL"));
        if (K == EKeys::Mouse2D || K == EKeys::MouseX || K == EKeys::MouseY) return PTText::Get(TEXT("TUT_MOUSE_MOVE"));
        if (K == EKeys::SpaceBar)  return PTText::Get(TEXT("TUT_KEY_SPACE"));
        if (K == EKeys::BackSpace) return PTText::Get(TEXT("TUT_KEY_BKSP"));
        if (K == EKeys::LeftControl || K == EKeys::RightControl) return FText::FromString(TEXT("Ctrl"));
        if (K == EKeys::LeftAlt || K == EKeys::RightAlt)         return FText::FromString(TEXT("Alt"));
        if (K == EKeys::LeftShift || K == EKeys::RightShift)     return FText::FromString(TEXT("Shift"));
        if (K == EKeys::Enter)  return FText::FromString(TEXT("Enter"));
        if (K == EKeys::Escape) return FText::FromString(TEXT("Esc"));
        return K.GetDisplayName();
    }

    // Acción del tutorial → tecla de teclado (id de PTInput o especial), botón del joystick (id de
    // PTGamepad o especial) y el texto "para qué sirve".
    struct FTutAction { const TCHAR* Id; const TCHAR* Kb; const TCHAR* KbFixed; const TCHAR* Pad; const TCHAR* PadFixed; const TCHAR* Label; };
    const FTutAction GTutActions[] = {
        { TEXT("Look"),          nullptr,                TEXT("TUT_MOUSE_MOVE"), nullptr,          TEXT("TUT_STICK_R"), TEXT("KEY_LOOK") },
        { TEXT("Move"),          nullptr,                TEXT("TUT_KEY_WASD"),   nullptr,          TEXT("TUT_STICK_L"), TEXT("KEY_MOVE") },
        { TEXT("FlyUp"),         TEXT("FlyUp"),          nullptr, TEXT("FlyUp"),       nullptr, TEXT("KEY_FLY_UP") },
        { TEXT("FlyDown"),       TEXT("FlyDown"),        nullptr, TEXT("FlyDown"),     nullptr, TEXT("KEY_FLY_DOWN") },
        { TEXT("Sculpt"),        TEXT("Sculpt"),         nullptr, TEXT("Sculpt"),      nullptr, TEXT("TUT_K_SCULPT") },
        { TEXT("BrushSize"),     nullptr,                TEXT("TUT_MOUSE_WHEEL"), nullptr,         TEXT("TUT_PAD_LBRB"), TEXT("KEY_BRUSH_SIZE") },
        { TEXT("ModeAdd"),       TEXT("ModeAdd"),        nullptr, TEXT("ToolAdd"),     nullptr, TEXT("KEY_MODE_ADD") },
        { TEXT("ModeErase"),     TEXT("ModeErase"),      nullptr, TEXT("ToolErase"),   nullptr, TEXT("KEY_MODE_ERASE") },
        { TEXT("ModePaint"),     TEXT("ModePaint"),      nullptr, TEXT("ToolPaint"),   nullptr, TEXT("KEY_MODE_PAINT") },
        { TEXT("ModeEyes"),      TEXT("ModeEyes"),       nullptr, TEXT("ToolEyes"),    nullptr, TEXT("KEY_MODE_EYES") },
        { TEXT("CycleShape"),    TEXT("CycleShape"),     nullptr, TEXT("ShapeRadial"), nullptr, TEXT("TUT_K_SHAPES") },
        { TEXT("AxisVertical"),  TEXT("AxisVertical"),   nullptr, TEXT("AxisVertical"),nullptr, TEXT("TUT_K_VERT") },
        { TEXT("AxisHorizontal"),TEXT("AxisHorizontal"), nullptr, nullptr,             nullptr, TEXT("TUT_K_HORIZ") },
        { TEXT("RotateShape"),   TEXT("RotateShape"),    nullptr, TEXT("RotateShape"), nullptr, TEXT("TUT_K_ROTATE") },
        { TEXT("SurfaceSnap"),   nullptr,                TEXT("TUT_KEY_ALT"),    TEXT("SurfaceSnap"), nullptr, TEXT("TUT_K_ALT") },
        { TEXT("Undo"),          TEXT("ClearAll"),       nullptr, TEXT("Undo"),        nullptr, TEXT("TUT_K_UNDO") },
        { TEXT("ClearAll"),      TEXT("ClearAll"),       nullptr, TEXT("Undo"),        nullptr, TEXT("TUT_K_CLEAR") },
        { TEXT("ColorPick"),     TEXT("ColorPick"),      nullptr, TEXT("ColorPick"),   nullptr, TEXT("TUT_K_COLOR") },
        { TEXT("SaveColor"),     TEXT("SaveColor"),      nullptr, nullptr,             nullptr, TEXT("KEY_SAVE_COLOR") },
        { TEXT("Done"),          nullptr,                TEXT("TUT_KEY_ENTER"),  nullptr,          TEXT("TUT_PAD_PAUSE"), TEXT("TUT_DONE_BTN") },
        { TEXT("Finish"),        nullptr,                TEXT("TUT_KEY_ENTER"),  nullptr,          TEXT("TUT_PAD_PAUSE"), TEXT("TUT_K_FINISH") },
        { TEXT("PhotoOrbit"),    nullptr,                TEXT("TUT_KEY_WASD"),   nullptr,          TEXT("TUT_STICK_L"), TEXT("TUT_K_ORBIT") },
        { TEXT("PhotoShoot"),    nullptr,                TEXT("TUT_KEY_ENTER"),  TEXT("FlyUp"),    nullptr, TEXT("TUT_K_SHOOT") },
    };

    const FTutAction* FindTutAction(FName Id)
    {
        for (const FTutAction& A : GTutActions) if (Id == FName(A.Id)) return &A;
        return nullptr;
    }

    // ¿Está P dentro de la forma de sello (misma cuenta que APTSculptVolume::ApplyStamp)?
    bool InsidePart(const FPTGhostPart& G, const FVector& P)
    {
        FVector LP = G.Rot.Quaternion().UnrotateVector(P - G.Center);
        const FVector S(FMath::Max(0.05f, (float)G.Scale.X), FMath::Max(0.05f, (float)G.Scale.Y), FMath::Max(0.05f, (float)G.Scale.Z));
        LP /= S;
        return APTSculptVolume::StampSDF(G.Shape, LP, G.Size * 0.5f) > 0.f;
    }

    // ── Íconos de teclas / botones (Content/UMG/Texture/NewUI/Gameplay_UI/Keyboards) ──
    UTexture2D* TutIcon(const FString& Name, bool bPad)
    {
        if (Name.IsEmpty()) return nullptr;
        const FString Path = FString(bPad ? TEXT("/Game/UMG/Texture/NewUI/Gameplay_UI/Keyboards/Joystick/")
                                          : TEXT("/Game/UMG/Texture/NewUI/Gameplay_UI/Keyboards/Keyboard/")) + Name + TEXT(".") + Name;
        return Cast<UTexture2D>(StaticLoadObject(UTexture2D::StaticClass(), nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet));
    }

    FString KbIconName(const FKey& K)
    {
        static const TMap<FName, const TCHAR*> Map = {
            { EKeys::SpaceBar.GetFName(), TEXT("Space") }, { EKeys::Tab.GetFName(), TEXT("Tab") },
            { EKeys::LeftAlt.GetFName(), TEXT("Alt") }, { EKeys::RightAlt.GetFName(), TEXT("Alt") },
            { EKeys::LeftControl.GetFName(), TEXT("Ctrl") }, { EKeys::RightControl.GetFName(), TEXT("Ctrl") },
            { EKeys::LeftShift.GetFName(), TEXT("Shift") }, { EKeys::RightShift.GetFName(), TEXT("Shift") },
            { EKeys::BackSpace.GetFName(), TEXT("Backspace") }, { EKeys::Enter.GetFName(), TEXT("Enter") },
            { EKeys::Escape.GetFName(), TEXT("Esc") },
            { EKeys::LeftMouseButton.GetFName(), TEXT("Mouse_Left") }, { EKeys::RightMouseButton.GetFName(), TEXT("Mouse_Right") },
            { EKeys::MiddleMouseButton.GetFName(), TEXT("Mouse_Middle") }, { EKeys::MouseWheelAxis.GetFName(), TEXT("Mouse_Middle") },
            { EKeys::Mouse2D.GetFName(), TEXT("Mouse_Simple") },
            { EKeys::Zero.GetFName(), TEXT("0") }, { EKeys::One.GetFName(), TEXT("1") }, { EKeys::Two.GetFName(), TEXT("2") },
            { EKeys::Three.GetFName(), TEXT("3") }, { EKeys::Four.GetFName(), TEXT("4") }, { EKeys::Five.GetFName(), TEXT("5") },
            { EKeys::Six.GetFName(), TEXT("6") }, { EKeys::Seven.GetFName(), TEXT("7") }, { EKeys::Eight.GetFName(), TEXT("8") },
            { EKeys::Nine.GetFName(), TEXT("9") },
        };
        if (const TCHAR* const* N = Map.Find(K.GetFName())) return FString(*N) + TEXT("_Key_Dark");
        const FString S = K.GetFName().ToString();
        if (S.Len() == 1 && FChar::IsAlpha(S[0])) return S.ToUpper() + TEXT("_Key_Dark");
        return FString();
    }

    FString PadIconName(const FKey& K)
    {
        static const TMap<FName, const TCHAR*> Map = {
            { EKeys::Gamepad_FaceButton_Bottom.GetFName(), TEXT("A") }, { EKeys::Gamepad_FaceButton_Right.GetFName(), TEXT("B") },
            { EKeys::Gamepad_FaceButton_Left.GetFName(), TEXT("X") },   { EKeys::Gamepad_FaceButton_Top.GetFName(), TEXT("Y") },
            { EKeys::Gamepad_LeftShoulder.GetFName(), TEXT("LB") },     { EKeys::Gamepad_RightShoulder.GetFName(), TEXT("RB") },
            { EKeys::Gamepad_LeftTrigger.GetFName(), TEXT("LT") },      { EKeys::Gamepad_RightTrigger.GetFName(), TEXT("RT") },
            { EKeys::Gamepad_DPad_Up.GetFName(), TEXT("Pad_Up") },      { EKeys::Gamepad_DPad_Down.GetFName(), TEXT("Pad_Down") },
            { EKeys::Gamepad_DPad_Left.GetFName(), TEXT("Pad_Left") },  { EKeys::Gamepad_DPad_Right.GetFName(), TEXT("Pad_Right") },
            { EKeys::Gamepad_LeftThumbstick.GetFName(), TEXT("L_Click") }, { EKeys::Gamepad_RightThumbstick.GetFName(), TEXT("R_Click") },
            { EKeys::Gamepad_Special_Left.GetFName(), TEXT("View") },   { EKeys::Gamepad_Special_Right.GetFName(), TEXT("Menu") },
        };
        if (const TCHAR* const* N = Map.Find(K.GetFName())) return FString(TEXT("UI_Joystick_")) + *N;
        return FString();
    }

    const TCHAR* GPraise[] = { TEXT("TUT_GOOD_1"), TEXT("TUT_GOOD_2"), TEXT("TUT_GOOD_3"), TEXT("TUT_GOOD_4") };
}

APTTutorialDirector::APTTutorialDirector()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bTickEvenWhenPaused = true;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    RootComponent = Root;

    VoiceSounds = {
        TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Template/SFX/SFX_Bop.SFX_Bop"))),
        TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Template/SFX/SFX_Bop-2.SFX_Bop-2"))),
        TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Template/SFX/SFX_Bop-3.SFX_Bop-3"))),
        TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Template/SFX/SFX_Bop-4.SFX_Bop-4"))),
    };
    SuccessSound   = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Template/SFX/SFX_TiiinnCorrect.SFX_TiiinnCorrect")));
    CountdownSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Template/SFX/SFX_PerSecondGame.SFX_PerSecondGame")));
    ShutterSound   = TSoftObjectPtr<USoundBase>(FSoftObjectPath(TEXT("/Game/Template/SFX/SFX_Unlock-03.SFX_Unlock-03")));
    LogoTexture    = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(TEXT("/Game/Template/UI/Titulo-LogoOrange.Titulo-LogoOrange")));
    // Material de las guías: el que armes en Content/Tutorial (instancia o material, con parámetro "Color");
    // si no existe, el brillo de bordes del modo Suavizar (sin color).
    GhostMaterial  = TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Tutorial/M_TutorialGhost_Inst.M_TutorialGhost_Inst")));
    GhostMaterialFallbacks = {
        TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Tutorial/MI_TutorialGhost.MI_TutorialGhost"))),
        TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Tutorial/M_TutorialGhost.M_TutorialGhost"))),
        TSoftObjectPtr<UMaterialInterface>(FSoftObjectPath(TEXT("/Game/Template/Materials/M_SmoothPreview.M_SmoothPreview"))),
    };
    RingMesh       = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Game/Template/Meshes/SM_Tourus.SM_Tourus")));
}

void APTTutorialDirector::BeginPlay()
{
    Super::BeginPlay();
    UE_LOG(LogPTTutorial, Log, TEXT("Tutorial de Sculpi: arrancando."));
}

void APTTutorialDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    if (ShotDelegate.IsValid()) UGameViewportClient::OnScreenshotCaptured().Remove(ShotDelegate);
    if (APTSculptPlayerController* P = PC.Get())
    {
        P->OnLocalUndo.Remove(UndoH);
        P->OnLocalClearAll.Remove(ClearH);
        P->OnLocalColorSaved.Remove(SaveH);
        P->OnLocalChat.Remove(ChatH);
    }
    // Se fue sin terminar (menú de pausa → Salir): cuenta como saltado.
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
    {
        GI->bTutorialWantsCursor = false;
        GI->bTutorialGuessPhase = false; // por si se sale durante el intro de adivinar
    }
    if (APTSculptVolume* V = Volume.Get()) V->SetFrameMeshesHidden(false); // restaurar el marco al salir
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
    {
        if (!bExiting && GI->bTutorialMode)
        {
            GI->bTutorialMode = false;
            if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
            {
                if (bQuickMode) S->SetQuickTutorialDone(true); // el rápido marca su propio flag
                else            S->SetTutorialDone(true);
            }
        }
    }
    if (Widget) Widget->RemoveFromParent();
    Super::EndPlay(Reason);
}

// ── Preparación ─────────────────────────────────────────────────────────────

bool APTTutorialDirector::SetupWorld()
{
    UWorld* W = GetWorld();
    APTSculptPlayerController* P = Cast<APTSculptPlayerController>(W ? W->GetFirstPlayerController() : nullptr);
    if (!P || !P->GetPawn() || !P->PlayerCameraManager) return false;
    APTSculptVolume* V = Cast<APTSculptVolume>(UGameplayStatics::GetActorOfClass(W, APTSculptVolume::StaticClass()));
    if (!V) return false;
    FTransform Xf; FVector Ext;
    if (!V->GetCanvasBox(Xf, Ext)) return false;
    PC = P;
    Volume = V;
    CanvasCenter = Xf.GetLocation();
    CanvasExt = Ext; // tamaño final (escala 1): el cubo puede estar creciendo todavía
    FloorZ = CanvasCenter.Z - Ext.Z;

    Widget = CreateWidget<UPTTutorialWidget>(P, UPTTutorialWidget::StaticClass());
    if (Widget)
    {
        Widget->VoiceSounds = VoiceSounds;
        Widget->LogoTexture = LogoTexture;
        Widget->AddToViewport(30); // encima del HUD y del menú de pausa (Z 10)
        Widget->OnSkipClicked.AddUObject(this, &APTTutorialDirector::OnSkip);
        Widget->OnDoneClicked.AddUObject(this, &APTTutorialDirector::OnDonePerro);
        Widget->OnContinueClicked.AddUObject(this, &APTTutorialDirector::OnContinue);
        Widget->OnSavePhotoClicked.AddUObject(this, &APTTutorialDirector::OnSavePhoto);
        Widget->OnShareClicked.AddUObject(this, &APTTutorialDirector::OnSharePhoto);
    }
    BindPCEvents();
    SpawnSculpi();
    ClearClay();
    if (const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>()) bQuickMode = GI->bQuickTutorial;
    return true;
}

void APTTutorialDirector::BindPCEvents()
{
    APTSculptPlayerController* P = PC.Get();
    if (!P) return;
    UndoH  = P->OnLocalUndo.AddLambda([this]() { bUndoDone = true; ++UndoCount; });
    ClearH = P->OnLocalClearAll.AddLambda([this]() { bClearDone = true; });
    SaveH  = P->OnLocalColorSaved.AddLambda([this]() { bSawSave = true; ++SaveCount; });
    ChatH  = P->OnLocalChat.AddUObject(this, &APTTutorialDirector::OnTutorialChat);
}

void APTTutorialDirector::SpawnSculpi()
{
    UWorld* W = GetWorld();
    APTSculptPlayerController* P = PC.Get();
    if (!W || !P) return;
    const AGameModeBase* GM = W->GetAuthGameMode();
    UClass* Cls = (GM && GM->DefaultPawnClass) ? GM->DefaultPawnClass.Get() : APTLobbyCharacter::StaticClass();
    if (!Cls->IsChildOf(APTLobbyCharacter::StaticClass())) Cls = APTLobbyCharacter::StaticClass();
    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    const FVector Loc = P->PlayerCameraManager->GetCameraLocation() + P->PlayerCameraManager->GetCameraRotation().Vector() * 300.f;
    Sculpi = W->SpawnActor<APTLobbyCharacter>(Cls, Loc, FRotator::ZeroRotator, SP);
    if (!Sculpi) return;
    Sculpi->SetActorEnableCollision(false); // que no frene al jugador ni al rayo de esculpir
    Sculpi->ApplyGameplayMovementMode();
    Sculpi->SetNameOverride(TEXT("Sculpi")); // cartel con su nombre (no tiene PlayerState)

    // Skin "Frank Suit" (Workshop), empaquetada en Content/Tutorial.
    TArray<uint8> Bytes, Head, Body;
    const FString Path = FPaths::ProjectContentDir() / SculpiSkinFile;
    if (FFileHelper::LoadFileToArray(Bytes, *Path) && UPTLockerSubsystem::ParseSkinBundle(Bytes, Head, Body))
        Sculpi->ApplySkinLocal(Head, Body);
    else
        UE_LOG(LogPTTutorial, Warning, TEXT("No se pudo cargar la skin de Sculpi (%s)."), *Path);
}

void APTTutorialDirector::TickSculpi(float Dt)
{
    APTSculptPlayerController* P = PC.Get();
    if (!Sculpi || !P || !P->PlayerCameraManager || Sculpi->IsHidden()) return;
    const FVector CamLoc = P->PlayerCameraManager->GetCameraLocation();
    const FRotator CamRot = P->PlayerCameraManager->GetCameraRotation();
    const FRotationMatrix M(FRotator(0.f, CamRot.Yaw, 0.f));

    // Durante el DEMO del tutorial rápido: Sculpi se pone frente al trazo que "esculpe", mirándolo (de su
    // cara sale la línea punteada hasta la brocha). Así se ve que él es quien esculpe, como un jugador.
    if (bQSculpiAtWork && QBrush && QBrush->IsVisible())
    {
        const FVector BrushLoc = QBrush->GetComponentLocation();
        const FVector Target = BrushLoc - M.GetUnitAxis(EAxis::X) * 130.f + M.GetUnitAxis(EAxis::Y) * 260.f + FVector(0, 0, 130.f);
        const FVector NewLoc = FMath::VInterpTo(Sculpi->GetActorLocation(), Target, Dt, 5.f);
        const FRotator Face = (BrushLoc - NewLoc).Rotation();
        Sculpi->SetActorLocationAndRotation(NewLoc, FMath::RInterpTo(Sculpi->GetActorRotation(), FRotator(0.f, Face.Yaw, 0.f), Dt, 7.f));
        return;
    }

    const float Bob = FMath::Sin(GetWorld()->GetTimeSeconds() * (Widget && Widget->IsTyping() ? 9.f : 2.2f)) *
                      (Widget && Widget->IsTyping() ? 6.f : 10.f);
    // Adelante a la derecha de la cámara, un poco abajo: siempre a la vista, sin tapar el centro.
    const FVector Target = CamLoc + M.GetUnitAxis(EAxis::X) * 520.f + M.GetUnitAxis(EAxis::Y) * 330.f + FVector(0.f, 0.f, -95.f + Bob);
    const FVector NewLoc = FMath::VInterpTo(Sculpi->GetActorLocation(), Target, Dt, 4.f);
    const FRotator Face = (CamLoc - NewLoc).Rotation();
    Sculpi->SetActorLocationAndRotation(NewLoc, FMath::RInterpTo(Sculpi->GetActorRotation(), FRotator(0.f, Face.Yaw, 0.f), Dt, 6.f));
}

void APTTutorialDirector::CaptureOrientation()
{
    // "Adelante" = hacia donde mira la cámara (horizontal); "hacia el jugador" = al revés.
    if (APTSculptPlayerController* P = PC.Get())
        if (P->PlayerCameraManager)
        {
            const FRotationMatrix M(FRotator(0.f, P->PlayerCameraManager->GetCameraRotation().Yaw, 0.f));
            Fwd = M.GetUnitAxis(EAxis::X);
            Right = M.GetUnitAxis(EAxis::Y);
        }
}

// ── Guías fantasma ──────────────────────────────────────────────────────────

void APTTutorialDirector::ClearGhosts()
{
    for (FPTGhost& G : Ghosts)
        for (UPrimitiveComponent* M : G.Meshes)
            if (M) M->DestroyComponent();
    Ghosts.Reset();
    KeepAlive.Reset();
}

int32 APTTutorialDirector::AddGhost(const TArray<FPTGhostPart>& Parts, const FLinearColor& Color, bool bVisible)
{
    FPTGhost G;
    G.Parts = Parts;
    G.Color = Color;
    G.bVisible = bVisible;

    if (bVisible)
    {
        // Sin avisos si todavía no existe el material propio: se prueba el siguiente.
        UMaterialInterface* Base = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
            *GhostMaterial.ToSoftObjectPath().ToString(), nullptr, LOAD_NoWarn | LOAD_Quiet));
        for (int32 i = 0; !Base && i < GhostMaterialFallbacks.Num(); ++i)
            Base = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr,
                *GhostMaterialFallbacks[i].ToSoftObjectPath().ToString(), nullptr, LOAD_NoWarn | LOAD_Quiet));
        if (!Base && PC.IsValid()) Base = PC->GetGhostMaterial();
        G.MID = Base ? UMaterialInstanceDynamic::Create(Base, this) : nullptr;
        if (G.MID)
        {
            G.MID->SetVectorParameterValue(TEXT("Color"), Color * GhostIntensity);
            G.MID->SetScalarParameterValue(TEXT("GlowEnable"), 0.f);
            KeepAlive.Add(G.MID);
        }
        if (UMaterialInterface* Ov = GhostOverlayMaterial.LoadSynchronous())
        {
            G.OverlayMID = UMaterialInstanceDynamic::Create(Ov, this);
            G.OverlayMID->SetVectorParameterValue(TEXT("Color"), Color);
            KeepAlive.Add(G.OverlayMID);
        }
        for (const FPTGhostPart& Part : Parts)
        {
            // Aros: la malla de toro del juego (el toro del preview sale muy grueso).
            if (Part.Shape == EPTStampShape::Torus)
                if (UStaticMesh* Ring = RingMesh.LoadSynchronous())
                {
                    UStaticMeshComponent* SM = NewObject<UStaticMeshComponent>(this);
                    SM->SetupAttachment(Root);
                    SM->RegisterComponent();
                    SM->SetStaticMesh(Ring);
                    SM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
                    SM->SetCastShadow(false);
                    const FVector B = Ring->GetBounds().BoxExtent * 2.f;
                    const float S = Part.Size / FMath::Max(1.f, (float)FMath::Max(B.X, B.Y));
                    SM->SetWorldLocationAndRotation(Part.Center, Part.Rot);
                    SM->SetWorldScale3D(FVector(S));
                    if (G.MID) SM->SetMaterial(0, G.MID);
                    if (G.OverlayMID) SM->SetOverlayMaterial(G.OverlayMID);
                    G.Meshes.Add(SM);
                    KeepAlive.Add(SM);
                    continue;
                }
            TArray<FVector> V, N; TArray<int32> T;
            // VoxSz grande a propósito: la grilla del preview tiene tope de celdas.
            APTSculptVolume::BuildStampPreview(Part.Shape, Part.Size, FMath::Max(6.f, Part.Size * (float)Part.Scale.GetMax() / 40.f), V, T, N, Part.Scale);
            if (V.Num() == 0) continue;
            UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this);
            M->SetupAttachment(Root);
            M->RegisterComponent();
            M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            M->SetCastShadow(false);
            M->SetTranslucentSortPriority(9);
            M->CreateMeshSection(0, V, T, N, TArray<FVector2D>(), TArray<FColor>(), TArray<FProcMeshTangent>(), false);
            if (G.MID) M->SetMaterial(0, G.MID);
            if (G.OverlayMID) M->SetOverlayMaterial(G.OverlayMID);
            M->SetWorldLocationAndRotation(Part.Center, Part.Rot);
            G.Meshes.Add(M);
            KeepAlive.Add(M);
        }
    }

    // Puntos dentro de la guía (unión de las partes), en una grilla de ~14 por lado.
    FBox Box(ForceInit);
    for (const FPTGhostPart& Part : Parts)
    {
        const float R = Part.Size * 0.5f * (float)Part.Scale.GetMax() * 1.75f;
        Box += Part.Center + FVector(R);
        Box += Part.Center - FVector(R);
    }
    const FVector Ext = Box.GetSize();
    const float StepLen = FMath::Clamp((float)Ext.GetMax() / 14.f, 10.f, 60.f);
    const APTSculptVolume* V = Volume.Get();
    for (float X = Box.Min.X; X <= Box.Max.X; X += StepLen)
        for (float Y = Box.Min.Y; Y <= Box.Max.Y; Y += StepLen)
            for (float Z = Box.Min.Z; Z <= Box.Max.Z; Z += StepLen)
            {
                const FVector P(X, Y, Z);
                if (V && !V->IsInsideCanvas(P)) continue; // ej: la mitad enterrada del iglú
                for (const FPTGhostPart& Part : Parts)
                    if (InsidePart(Part, P)) { G.Samples.Add(P); break; }
            }
    return Ghosts.Add(MoveTemp(G));
}

float APTTutorialDirector::MeasureFill(int32 Idx)
{
    if (!Ghosts.IsValidIndex(Idx) || !Volume.IsValid()) return 0.f;
    FPTGhost& G = Ghosts[Idx];
    if (G.Samples.Num() == 0) return 0.f;
    int32 Solid = 0;
    for (const FVector& P : G.Samples) if (Volume->SampleWorldDensity(P) > 0.f) ++Solid;
    G.Fill = Solid / (float)G.Samples.Num();
    // La guía se pone verde a medida que se llena.
    UpdateGhostColor(Idx);
    return G.Fill;
}

void APTTutorialDirector::UpdateGhostColor(int32 Idx)
{
    if (!Ghosts.IsValidIndex(Idx)) return;
    FPTGhost& G = Ghosts[Idx];
    // Se pone verde a medida que se llena; con el pincel adentro, más clara (se "enciende").
    FLinearColor Now = FMath::Lerp(G.Color, TUTD_Green, FMath::Clamp(G.Fill, 0.f, 1.f));
    if (G.bAimed) Now = FMath::Lerp(Now, FLinearColor::White, 0.45f);
    if (G.MID) G.MID->SetVectorParameterValue(TEXT("Color"), Now * GhostIntensity);
    if (G.OverlayMID) G.OverlayMID->SetVectorParameterValue(TEXT("Color"), Now);
}

float APTTutorialDirector::QuickLegsMinFill() const
{
    // Divide los samples de la guía en 4 cuadrantes XY (las 4 patas, en las 4 esquinas) y devuelve el
    // relleno MÍNIMO entre ellos. Así, si falta una pata, su cuadrante queda vacío → el mínimo es bajo
    // y la lección NO se completa con 3 patas.
    if (!Ghosts.IsValidIndex(QGhost) || !Volume.IsValid()) return 0.f;
    const FPTGhost& G = Ghosts[QGhost];
    const int32 NS = G.Samples.Num();
    if (NS == 0) return 0.f;
    FVector2D C(0.f, 0.f);
    for (const FVector& P : G.Samples) C += FVector2D(P.X, P.Y);
    C /= NS;
    int32 Solid[4] = { 0,0,0,0 }, Tot[4] = { 0,0,0,0 };
    for (const FVector& P : G.Samples)
    {
        const int32 q = (P.X >= C.X ? 1 : 0) + (P.Y >= C.Y ? 2 : 0);
        ++Tot[q];
        if (Volume->SampleWorldDensity(P) > 0.f) ++Solid[q];
    }
    float MinF = 1.f; int32 Used = 0;
    const int32 MinSamples = FMath::Max(3, NS / 20); // ignorar cuadrantes casi vacíos (ruido)
    for (int32 q = 0; q < 4; ++q)
        if (Tot[q] >= MinSamples) { ++Used; MinF = FMath::Min(MinF, Solid[q] / (float)Tot[q]); }
    return (Used >= 3) ? MinF : 0.f; // hacen falta ~4 grupos de patas
}

void APTTutorialDirector::QuickTickGhostHighlight()
{
    if (!Ghosts.IsValidIndex(QGhost)) return;
    FPTGhost& G = Ghosts[QGhost];
    APTSculptPlayerController* P = PC.Get();
    const AActor* Brush = P ? P->GetBrushPreviewActor() : nullptr;
    const bool bShow = Brush && !Brush->IsHidden();
    const FVector B = bShow ? Brush->GetActorLocation() : FVector::ZeroVector;
    const float BrushR = P ? FMath::Max(40.f, P->StampSize * 0.5f) : 80.f;

    // Material de resaltado (cian brillante), creado una sola vez a partir del mismo base de la guía.
    if (!QHiliteMID && G.MID && G.MID->Parent)
    {
        QHiliteMID = UMaterialInstanceDynamic::Create(G.MID->Parent, this);
        if (QHiliteMID)
        {
            QHiliteMID->SetVectorParameterValue(TEXT("Color"), FLinearColor(0.15f, 1.f, 1.f) * GhostIntensity);
            QHiliteMID->SetScalarParameterValue(TEXT("GlowEnable"), 0.f);
            KeepAlive.Add(QHiliteMID);
        }
    }

    for (UPrimitiveComponent* M : G.Meshes)
    {
        if (!M) continue;
        // Radio de la parte más cercana a esta malla (la malla está ubicada en el centro de su parte).
        const FVector MC = M->GetComponentLocation();
        float PartR = 80.f; float Best = TNumericLimits<float>::Max();
        for (const FPTGhostPart& Part : G.Parts)
        {
            const float D = (float)FVector::DistSquared(MC, Part.Center);
            if (D < Best) { Best = D; PartR = Part.Size * 0.5f * (float)Part.Scale.GetMax(); }
        }
        const bool bTouch = bShow && FVector::Dist(B, MC) < (PartR + BrushR);
        M->SetWorldScale3D(FVector(bTouch ? 1.3f : 1.f));
        if (bTouch && QHiliteMID) M->SetMaterial(0, QHiliteMID);
        else if (G.MID)          M->SetMaterial(0, G.MID);
    }
}

void APTTutorialDirector::TickAim()
{
    APTSculptPlayerController* P = PC.Get();
    const AActor* Brush = P ? P->GetBrushPreviewActor() : nullptr;
    const bool bUsesAim = Step == EPTTutStep::Add || Step == EPTTutStep::Shapes || Step == EPTTutStep::Rotate ||
                          Step == EPTTutStep::Squash || Step == EPTTutStep::Lines || Step == EPTTutStep::Alt ||
                          Step == EPTTutStep::Erase || Step == EPTTutStep::Iglu ||
                          (Step == EPTTutStep::Hongo && LineIndex == 0); // el sombrero no tiene guía visible
    int32 Best = INDEX_NONE;
    float BestDist = TNumericLimits<float>::Max();
    const FPTGhostPart* BestPart = nullptr;
    FVector S = FVector::ZeroVector;
    if (bUsesAim && !bStepDone && Brush && !Brush->IsHidden() && P->PlayerCameraManager)
    {
        S = Brush->GetActorLocation();
        for (int32 i = 0; i < Ghosts.Num(); ++i)
        {
            if (!Ghosts[i].bVisible) continue;
            for (const FPTGhostPart& Part : Ghosts[i].Parts)
            {
                const float D = (float)FVector::Dist(S, Part.Center);
                if (D < BestDist) { BestDist = D; Best = i; BestPart = &Part; }
            }
        }
    }
    bool bInside = false;
    if (Best != INDEX_NONE)
        for (const FPTGhostPart& Part : Ghosts[Best].Parts)
            if (InsidePart(Part, S)) { bInside = true; break; }
    for (int32 i = 0; i < Ghosts.Num(); ++i)
    {
        const bool bAim = (i == Best) && bInside;
        if (Ghosts[i].bAimed != bAim) { Ghosts[i].bAimed = bAim; UpdateGhostColor(i); }
    }
    if (!Widget) return;
    if (Best == INDEX_NONE || !BestPart) { Widget->SetAim(FText::GetEmpty(), FLinearColor::White); return; }
    if (bInside) { Widget->SetAim(PTText::Get(TEXT("TUT_AIM_IN")), TUTD_Green); return; }

    // Afuera: ¿más adelante / más atrás de la guía (según la cámara) o corrido al costado?
    // El pincel va a una distancia fija de la cámara: para corregir la profundidad hay que moverse.
    const FVector CamFwd = P->PlayerCameraManager->GetCameraRotation().Vector();
    const FVector Off = S - BestPart->Center;
    const float Along = (float)FVector::DotProduct(Off, CamFwd);
    const float Lateral = (float)(Off - CamFwd * Along).Size();
    const float R = BestPart->Size * 0.5f * (float)BestPart->Scale.GetMax();
    const FLinearColor Warn(1.f, 0.78f, 0.35f, 1.f);
    if (Lateral > R * 1.1f)    Widget->SetAim(PTText::Get(TEXT("TUT_AIM_SIDE")), Warn);
    else if (Along > R * 0.3f) Widget->SetAim(PTText::Get(TEXT("TUT_AIM_BACK")), Warn);
    else if (Along < -R * 0.3f) Widget->SetAim(PTText::Get(TEXT("TUT_AIM_CLOSER")), Warn);
    else                       Widget->SetAim(PTText::Get(TEXT("TUT_AIM_IN")), TUTD_Green);
}

void APTTutorialDirector::TickTips()
{
    // Si se traba (sin avanzar un rato), Sculpi le dice qué le falta en su globo de chat.
    const bool bLesson = Step >= EPTTutStep::Look && Step <= EPTTutStep::Hongo;
    if (!Sculpi || !bLesson || bStepDone || (Widget && Widget->IsTyping())) return;
    const float Idle = StepTime - FMath::Max(LastAdvanceTime, LastTipTime);
    if (Idle < (LastTipTime > 0.f ? TipRepeatSeconds : TipFirstSeconds)) return;
    LastTipTime = StepTime;

    FText Tip;
    if (bChipsSequential && ChipActions.IsValidIndex(ActiveChip))
    {
        const FName A = ChipActions[ActiveChip];
        if (const FTutAction* Act = FindTutAction(A))
        {
            // Corto (el globo corta textos largos): sin el "(mantener)"; la tecla ya está en el cartel de abajo.
            FString Label = PTText::GetStr(FName(Act->Label));
            int32 Paren;
            if (Label.FindChar(TEXT('('), Paren)) Label = Label.Left(Paren).TrimEnd();
            Tip = FText::Format(PTText::Get(TEXT("TUT_TIP_CHIP")), FText::FromString(Label), KeyText(A));
        }
    }
    else if (Step == EPTTutStep::Move || Step == EPTTutStep::FlyUp || Step == EPTTutStep::FlyDown)
        Tip = PTText::Get(TEXT("TUT_TIP_RING"));
    else if (Step == EPTTutStep::Hongo && LineIndex == 1) // el tallo ya está: falta el sombrero (sin guía)
        Tip = PTText::Get(TEXT("TUT_TIP_CAP"));
    else if (Ghosts.Num() > 0)
    {
        bool bAimed = false;
        for (const FPTGhost& G : Ghosts) bAimed |= G.bAimed;
        if (!bAimed)                 Tip = PTText::Get(TEXT("TUT_TIP_AIM"));
        else if (LastProgress < 0.05f) Tip = PTText::Get(TEXT("TUT_TIP_CLICK"));
        else Tip = FText::Format(PTText::Get(TEXT("TUT_TIP_FILL")), FText::AsNumber(FMath::RoundToInt(LastProgress * 100.f)));
    }
    if (Tip.IsEmpty()) return;
    UE_LOG(LogPTTutorial, Log, TEXT("Consejo de Sculpi (lección %d): %s"), (int32)Step, *Tip.ToString());
    Sculpi->Multicast_ShowChatBubble(Tip.ToString(), false);
    PlaySfx(VoiceSounds.Num() > 0 ? VoiceSounds[FMath::RandRange(0, VoiceSounds.Num() - 1)] : TSoftObjectPtr<USoundBase>(), 0.4f);
}

float APTTutorialDirector::ReadTime() const
{
    // ~16 letras por segundo para leer tranquilo, entre 1,8 y 6 s.
    return FMath::Clamp((Widget ? Widget->GetTextLength() : 40) / 16.f, 1.8f, 6.f);
}

float APTTutorialDirector::MeasurePainted(const FVector& Center, float Radius) const
{
    if (!Volume.IsValid()) return 0.f;
    // Puntos repartidos sobre la esfera (Fibonacci), justo por dentro de la superficie.
    const int32 N = 160;
    int32 Painted = 0, Valid = 0;
    for (int32 i = 0; i < N; ++i)
    {
        const float Yv = 1.f - (i + 0.5f) * 2.f / N;
        const float R = FMath::Sqrt(FMath::Max(0.f, 1.f - Yv * Yv));
        const float Th = i * 2.39996323f;
        const FVector Dir(FMath::Cos(Th) * R, FMath::Sin(Th) * R, Yv);
        const FVector P = Center + Dir * Radius * 0.9f;
        if (!Volume->IsInsideCanvas(P)) continue;
        ++Valid;
        bool bPainted = false;
        Volume->SampleWorldPaintColor(P, bPainted);
        if (bPainted) ++Painted;
    }
    return Valid > 0 ? Painted / (float)Valid : 0.f;
}

void APTTutorialDirector::ClearClay()
{
    if (APTSculptVolume* V = Volume.Get()) V->Multicast_ClearAll();
}

void APTTutorialDirector::PlaceClay(EPTStampShape Shape, const FVector& Center, float Size, FVector Scale, FRotator Rot)
{
    if (APTSculptVolume* V = Volume.Get())
        V->ApplyStamp(Center, Shape, Size, EPTEditMode::Add, FLinearColor::White, Rot, Scale);
}

bool APTTutorialDirector::PawnNear(const FVector& P, float Horizontal, float Vertical) const
{
    const APawn* Pawn = PC.IsValid() ? PC->GetPawn() : nullptr;
    if (!Pawn) return false;
    const FVector D = Pawn->GetActorLocation() - P;
    return FVector2D(D.X, D.Y).Size() < Horizontal && FMath::Abs((float)D.Z) < Vertical;
}

void APTTutorialDirector::PlaySfx(const TSoftObjectPtr<USoundBase>& S, float Vol) const
{
    if (USoundBase* Snd = S.LoadSynchronous()) UGameplayStatics::PlaySound2D(this, Snd, Vol);
}

// ── Textos y teclas ─────────────────────────────────────────────────────────

void APTTutorialDirector::Say(const TCHAR* Key)
{
    UE_LOG(LogPTTutorial, Log, TEXT("Sculpi dice: %s"), Key);
    if (Widget) Widget->Say(PTText::Get(FName(Key)));
}

FText APTTutorialDirector::KeyText(FName Action) const
{
    const FTutAction* A = FindTutAction(Action);
    if (!A) return FText::GetEmpty();
    bool bPad = false;
    if (const UGameInstance* GI = GetGameInstance())
        if (const UPTGamepadUINavigator* Nav = GI->GetSubsystem<UPTGamepadUINavigator>()) bPad = Nav->IsUsingGamepad();
    if (bPad)
    {
        if (A->PadFixed) return PTText::Get(FName(A->PadFixed));
        if (A->Pad)      return FText::FromString(PTGamepad::KeyLabel(PTGamepad::GetKey(FName(A->Pad))));
        return FText::GetEmpty(); // no existe en el joystick
    }
    if (A->KbFixed) return PTText::Get(FName(A->KbFixed));
    if (A->Kb)      return TutKeyName(PTInput::GetKey(FName(A->Kb)));
    return FText::GetEmpty();
}

void APTTutorialDirector::SetHintsFor(std::initializer_list<FName> Actions, bool bSequential, bool bGate)
{
    bChipsGate = bGate;
    ChipActions.Reset();
    for (FName A : Actions) if (FindTutAction(A) && !KeyText(A).IsEmpty()) ChipActions.Add(A);
    ChipDone.Init(false, ChipActions.Num());
    bChipsSequential = bSequential && ChipActions.Num() > 0;
    ActiveChip = -1;
    if (bChipsSequential) ActivateNextChip();
    RefreshHints();
}

void APTTutorialDirector::ActivateNextChip()
{
    ActiveChip = ChipDone.IndexOfByKey(false);
    APTSculptPlayerController* P = PC.Get();
    // Punto de partida para lo que se mide "desde que te toca".
    ChipSizeBase = P ? P->StampSize : 0.f;
    ChipYawBase = TotalYaw;
    ChipUndoBase = UndoCount;
    ChipSaveBase = SaveCount;
    ChipEyesBase = Volume.IsValid() ? Volume->GetEyeCount() : 0;
}

bool APTTutorialDirector::ChipSatisfied(FName A) const
{
    const APTSculptPlayerController* P = PC.Get();
    if (!P) return false;
    const APawn* Pawn = P->GetPawn();
    const FVector Vel = Pawn ? Pawn->GetVelocity() : FVector::ZeroVector;
    if (A == TEXT("Look"))           return TotalYaw - ChipYawBase >= 45.f;
    if (A == TEXT("Move"))           return FVector2D(Vel.X, Vel.Y).Size() > 60.f;
    if (A == TEXT("FlyUp"))          return Vel.Z > 60.f;
    if (A == TEXT("FlyDown"))        return Vel.Z < -60.f;
    if (A == TEXT("BrushSize"))      return FMath::Abs(P->StampSize - ChipSizeBase) > 1.f || !P->StampScale.Equals(FVector::OneVector, 0.01f);
    if (A == TEXT("ModeAdd"))        return P->EditMode == EPTEditMode::Add && !P->IsEyesToolActive();
    if (A == TEXT("ModeErase"))      return P->EditMode == EPTEditMode::Erase && !P->IsEyesToolActive();
    if (A == TEXT("ModePaint"))      return P->EditMode == EPTEditMode::Paint && !P->IsEyesToolActive();
    if (A == TEXT("ModeEyes"))       return P->IsEyesToolActive();
    if (A == TEXT("CycleShape"))     return P->IsShapeRadialOpen();
    if (A == TEXT("RotateShape"))    return P->IsRotatingShape();
    if (A == TEXT("AxisVertical"))   return P->IsAxisLockActive() && !P->IsAxisHorizontal();
    if (A == TEXT("AxisHorizontal")) return P->IsAxisLockActive() && P->IsAxisHorizontal();
    if (A == TEXT("SurfaceSnap"))    return P->IsSurfaceSnapActive();
    if (A == TEXT("Undo"))           return UndoCount > ChipUndoBase;
    if (A == TEXT("ClearAll"))       return P->IsClearHeld() || bClearDone;
    if (A == TEXT("ColorPick"))      return P->IsColorPickerOpen();
    if (A == TEXT("SaveColor"))      return SaveCount > ChipSaveBase;
    if (A == TEXT("PhotoOrbit"))     return bPhotoAiming && FMath::Abs(FMath::FindDeltaAngleDegrees(OrbitYaw0, OrbitYaw)) > 10.f;
    if (A == TEXT("Sculpt"))
        return Step == EPTTutStep::Eyes ? (Volume.IsValid() && Volume->GetEyeCount() > ChipEyesBase) : P->IsStamping();
    return false;
}

void APTTutorialDirector::TickChips()
{
    if (!bChipsSequential || !ChipDone.IsValidIndex(ActiveChip)) return;
    if (!ChipSatisfied(ChipActions[ActiveChip])) return;
    ChipDone[ActiveChip] = true;
    LastAdvanceTime = StepTime; // avanzó: no hace falta consejo
    if (USoundBase* Snd = VoiceSounds.Num() > 0 ? VoiceSounds[0].LoadSynchronous() : nullptr)
        UGameplayStatics::PlaySound2D(this, Snd, 0.35f, 1.6f); // "tic" de tecla marcada
    if (ChipDone.Contains(false)) ActivateNextChip(); else ActiveChip = -1;
    RefreshHints();
}

void APTTutorialDirector::RefreshHints()
{
    if (!Widget) return;
    bool bPad = false;
    if (const UGameInstance* GI = GetGameInstance())
        if (const UPTGamepadUINavigator* Nav = GI->GetSubsystem<UPTGamepadUINavigator>()) bPad = Nav->IsUsingGamepad();
    TArray<FPTTutHint> Hints;
    for (int32 Ci = 0; Ci < ChipActions.Num(); ++Ci)
    {
        const FName Id = ChipActions[Ci];
        const FTutAction* A = FindTutAction(Id);
        const FText K = KeyText(Id);
        if (!A || K.IsEmpty()) continue;
        FPTTutHint H;
        H.Key = K;
        H.Label = PTText::Get(FName(A->Label));
        H.State = !bChipsSequential ? 0 : ChipDone[Ci] ? 3 : (Ci == ActiveChip ? 2 : 1);
        // Íconos: casos fijos (WASD, sticks, rueda, LB/RB) o la tecla/botón asignado ahora.
        TArray<FString> Names;
        if (bPad)
        {
            if (Id == TEXT("Look")) Names = { TEXT("UI_Joystick_R") };
            else if (Id == TEXT("Move")) Names = { TEXT("UI_Joystick_L") };
            else if (Id == TEXT("BrushSize")) Names = { PadIconName(PTGamepad::GetKey(TEXT("BrushSmaller"))), PadIconName(PTGamepad::GetKey(TEXT("BrushBigger"))) };
            else if (Id == TEXT("Done") || Id == TEXT("Finish")) Names = { TEXT("UI_Joystick_Menu") };
            else if (Id == TEXT("PhotoOrbit")) Names = { TEXT("UI_Joystick_L") };
            else if (A->Pad) Names = { PadIconName(PTGamepad::GetKey(FName(A->Pad))) };
        }
        else
        {
            if (Id == TEXT("Look")) Names = { TEXT("Mouse_Simple_Key_Dark") };
            else if (Id == TEXT("Move")) Names = { TEXT("W_Key_Dark"), TEXT("A_Key_Dark"), TEXT("S_Key_Dark"), TEXT("D_Key_Dark") };
            else if (Id == TEXT("BrushSize")) Names = { TEXT("Mouse_Middle_Key_Dark") };
            else if (Id == TEXT("SurfaceSnap")) Names = { TEXT("Alt_Key_Dark") };
            else if (Id == TEXT("Done") || Id == TEXT("PhotoShoot") || Id == TEXT("Finish")) Names = { TEXT("Enter_Key_Dark") };
            else if (Id == TEXT("PhotoOrbit")) Names = { TEXT("W_Key_Dark"), TEXT("A_Key_Dark"), TEXT("S_Key_Dark"), TEXT("D_Key_Dark") };
            else if (A->Kb) Names = { KbIconName(PTInput::GetKey(FName(A->Kb))) };
        }
        bool bAll = Names.Num() > 0;
        for (const FString& N : Names)
        {
            UTexture2D* T = TutIcon(N, bPad);
            if (!T) { bAll = false; break; }
            H.Icons.Add(T);
        }
        if (!bAll) H.Icons.Reset(); // falta alguno → texto
        Hints.Add(MoveTemp(H));
    }
    Widget->SetHints(Hints);
}

// ── Flujo de lecciones ──────────────────────────────────────────────────────

void APTTutorialDirector::NextStep()
{
    EnterStep((EPTTutStep)((uint8)Step + 1));
}

void APTTutorialDirector::CompleteStep()
{
    if (bStepDone || ChipsPending()) return; // primero las teclas de la secuencia
    bStepDone = true;
    UE_LOG(LogPTTutorial, Log, TEXT("Lección %d completada en %.1f s"), (int32)Step, StepTime);
    DoneTimer = 0.f;
    PlaySfx(SuccessSound, 0.5f);
    const TCHAR* Line = GPraise[FMath::RandRange(0, UE_ARRAY_COUNT(GPraise) - 1)];
    if (Step == EPTTutStep::Rotate) Line = TEXT("TUT_ROTATE_TIP");
    if (Step == EPTTutStep::Iglu)   Line = TEXT("TUT_IGLU_OK");
    if (Step == EPTTutStep::Hongo)  Line = TEXT("TUT_HONGO_OK");
    Say(Line);
    if (Widget) Widget->SetProgress(1.f, PTText::Get(TEXT("TUT_DONE_MARK")));
}

void APTTutorialDirector::EnterStep(EPTTutStep S)
{
    Step = S;
    StepTime = 0.f;
    LastAdvanceTime = 0.f;
    LastTipTime = 0.f;
    LastProgress = -1.f;
    bStepDone = false;
    DoneTimer = 0.f;
    LineIndex = 0;
    LineWait = 0.f;
    if (Widget) { Widget->SetProgress(-1.f, FText::GetEmpty()); Widget->SetHints(TArray<FPTTutHint>()); }
    APTSculptPlayerController* P = PC.Get();
    const APawn* Pawn = P ? P->GetPawn() : nullptr;
    const FVector C(CanvasCenter.X, CanvasCenter.Y, FloorZ);
    CaptureOrientation();
    const float ToPlayerYaw = (-Fwd).Rotation().Yaw;
    const float RightYaw = Right.Rotation().Yaw;

    switch (S)
    {
    case EPTTutStep::Intro:
        Say(TEXT("TUT_INTRO_1"));
        break;
    case EPTTutStep::Look:
        Say(TEXT("TUT_LOOK"));
        AccYaw = 0.f;
        LastYaw = P ? P->GetControlRotation().Yaw : 0.f;
        break;
    case EPTTutStep::Move:
    {
        Say(TEXT("TUT_MOVE"));
        ClearGhosts();
        const FVector Start = Pawn ? Pawn->GetActorLocation() : C;
        // Aro adelante; si cae dentro del cubo de esculpido, al costado o atrás.
        RingA = Start + Fwd * 420.f;
        if (Volume.IsValid() && Volume->IsInsideCanvas(RingA)) RingA = Start + Right * 420.f;
        if (Volume.IsValid() && Volume->IsInsideCanvas(RingA)) RingA = Start - Fwd * 420.f;
        AddGhost({ FPTGhostPart{ EPTStampShape::Torus, RingA, 300.f } }, TUTD_Yellow);
        MoveStartDist = FMath::Max(1.f, (float)FVector::Dist2D(Start, RingA));
        UE_LOG(LogPTTutorial, Log, TEXT("Aro: jugador %s -> aro %s (dist %.0f, fwd %s)"), *Start.ToString(), *RingA.ToString(), MoveStartDist, *Fwd.ToString());
        break;
    }
    case EPTTutStep::FlyUp:
        Say(TEXT("TUT_FLY_UP"));
        ClearGhosts();
        RingB = RingA + FVector(0.f, 0.f, 380.f);
        AddGhost({ FPTGhostPart{ EPTStampShape::Torus, RingB, 300.f } }, TUTD_Yellow);
        break;
    case EPTTutStep::FlyDown:
        Say(TEXT("TUT_FLY_DOWN"));
        ClearGhosts();
        RingC = RingA;
        AddGhost({ FPTGhostPart{ EPTStampShape::Torus, RingC, 300.f } }, TUTD_Yellow);
        break;
    case EPTTutStep::Add:
        Say(TEXT("TUT_ADD"));
        ClearGhosts(); ClearClay();
        AddGhost({ FPTGhostPart{ EPTStampShape::Sphere, C + FVector(0, 0, 190.f), 300.f } }, TUTD_Blue);
        break;
    case EPTTutStep::Size:
        Say(TEXT("TUT_SIZE"));
        ClearGhosts();
        SizeStart = P ? P->StampSize : 200.f;
        bSizeBigger = bSizeSmaller = false;
        break;
    case EPTTutStep::Shapes:
        Say(TEXT("TUT_SHAPES"));
        ClearGhosts(); ClearClay();
        AddGhost({ FPTGhostPart{ EPTStampShape::Cube, C + FVector(0, 0, 140.f), 280.f } }, TUTD_Blue);
        break;
    case EPTTutStep::Rotate:
        Say(TEXT("TUT_ROTATE"));
        ClearGhosts(); ClearClay();
        bSawRotate = false;
        AddGhost({ FPTGhostPart{ EPTStampShape::Cylinder, C + FVector(0, 0, 105.f), 210.f, FVector(1.f, 1.f, 1.9f), FRotator(90.f, RightYaw, 0.f) } }, TUTD_Blue);
        break;
    case EPTTutStep::Squash:
        Say(TEXT("TUT_SQUASH"));
        ClearGhosts(); ClearClay();
        bSawScale = false;
        AddGhost({ FPTGhostPart{ EPTStampShape::Sphere, C + FVector(0, 0, 75.f), 320.f, FVector(1.35f, 1.35f, 0.45f) } }, TUTD_Blue);
        break;
    case EPTTutStep::Lines:
        Say(TEXT("TUT_LINES"));
        ClearGhosts(); ClearClay();
        bSawAxisStroke = false;
        // Poste (vertical) a la derecha y viga (horizontal, acostada) a la izquierda.
        AddGhost({ FPTGhostPart{ EPTStampShape::Cylinder, C + Right * 210.f + FVector(0, 0, 230.f), 140.f, FVector(1.f, 1.f, 3.2f) } }, TUTD_Blue);
        AddGhost({ FPTGhostPart{ EPTStampShape::Cylinder, C - Right * 90.f + FVector(0, 0, 75.f), 140.f, FVector(1.f, 1.f, 3.0f), FRotator(90.f, RightYaw, 0.f) } }, TUTD_Blue);
        break;
    case EPTTutStep::Alt:
        Say(TEXT("TUT_ALT"));
        ClearGhosts(); ClearClay();
        PlaceClay(EPTStampShape::Cube, C + FVector(0, 0, 150.f), 300.f);
        DetailStart = Volume.IsValid() ? Volume->GetDetailMeshes().Num() : 0;
        AddGhost({ FPTGhostPart{ EPTStampShape::Sphere, C + FVector(0, 0, 360.f), 140.f } }, TUTD_Blue);
        break;
    case EPTTutStep::Erase:
        Say(TEXT("TUT_ERASE"));
        ClearGhosts(); ClearClay();
        PlaceClay(EPTStampShape::Cube, C + FVector(0, 0, 150.f), 300.f);
        // Zona roja: la mitad de arriba del cubo.
        AddGhost({ FPTGhostPart{ EPTStampShape::Cube, C + FVector(0, 0, 225.f), 290.f, FVector(1.f, 1.f, 0.5f) } }, TUTD_Red);
        break;
    case EPTTutStep::Undo:
        Say(TEXT("TUT_UNDO"));
        ClearGhosts();
        bUndoDone = false;
        break;
    case EPTTutStep::Clear:
        Say(TEXT("TUT_CLEAR"));
        bClearDone = false;
        break;
    case EPTTutStep::Paint:
        Say(TEXT("TUT_PAINT"));
        ClearGhosts(); ClearClay();
        PlaceClay(EPTStampShape::Sphere, C + FVector(0, 0, 170.f), 320.f);
        bSawPicker = bSawSave = false;
        break;
    case EPTTutStep::Eyes:
        Say(TEXT("TUT_EYES"));
        EyesStart = Volume.IsValid() ? Volume->GetEyeCount() : 0;
        break;
    case EPTTutStep::Iglu:
        Say(TEXT("TUT_WORDS_INTRO"));
        ClearGhosts(); ClearClay();
        // Media esfera apoyada en el piso (la mitad de abajo queda fuera del cubo) + entrada acostada.
        AddGhost({
            FPTGhostPart{ EPTStampShape::Sphere, C, 600.f },
            FPTGhostPart{ EPTStampShape::Cylinder, C - Fwd * 300.f + FVector(0, 0, 100.f), 220.f, FVector(1.f, 1.f, 1.0f), FRotator(90.f, ToPlayerYaw, 0.f) },
        }, TUTD_Blue);
        break;
    case EPTTutStep::Hongo:
        Say(TEXT("TUT_HONGO"));
        ClearGhosts(); ClearClay();
        AddGhost({ FPTGhostPart{ EPTStampShape::Cylinder, C + FVector(0, 0, 120.f), 160.f, FVector(1.f, 1.f, 1.5f) } }, TUTD_Blue);
        // El sombrero NO se dibuja (lo decide él), pero se mide: una esfera aplastada sobre el tallo.
        AddGhost({ FPTGhostPart{ EPTStampShape::Sphere, C + FVector(0, 0, 300.f), 440.f, FVector(1.f, 1.f, 0.42f) } }, TUTD_Blue, /*bVisible=*/false);
        bSawScale = false;
        break;
    case EPTTutStep::Perro:
        Say(TEXT("TUT_PERRO"));
        ClearGhosts(); ClearClay();
        PerroLeft = PerroSeconds;
        bPerroTimeUp = false;
        break;
    case EPTTutStep::Photo:
        StartPhoto();
        break;
    case EPTTutStep::End:
        break;
    }

    // Teclas de cada lección.
    switch (S)
    {
    case EPTTutStep::Look:    SetHintsFor({ TEXT("Look") }, /*bSequential=*/true); break;
    case EPTTutStep::Move:    SetHintsFor({ TEXT("Move"), TEXT("Look") }, /*bSequential=*/true); break;
    case EPTTutStep::FlyUp:   SetHintsFor({ TEXT("FlyUp") }, /*bSequential=*/true); break;
    case EPTTutStep::FlyDown: SetHintsFor({ TEXT("FlyDown") }, /*bSequential=*/true); break;
    case EPTTutStep::Add:     SetHintsFor({ TEXT("ModeAdd"), TEXT("Sculpt") }, /*bSequential=*/true); break;
    case EPTTutStep::Size:    SetHintsFor({ TEXT("BrushSize") }, /*bSequential=*/true); break;
    case EPTTutStep::Shapes:  SetHintsFor({ TEXT("CycleShape"), TEXT("Sculpt") }, /*bSequential=*/true); break;
    case EPTTutStep::Rotate:  SetHintsFor({ TEXT("CycleShape"), TEXT("RotateShape"), TEXT("Sculpt") }, /*bSequential=*/true); break;
    case EPTTutStep::Squash:  SetHintsFor({ TEXT("AxisVertical"), TEXT("AxisHorizontal"), TEXT("BrushSize"), TEXT("Sculpt") }, /*bSequential=*/true); break;
    case EPTTutStep::Lines:   SetHintsFor({ TEXT("AxisVertical"), TEXT("AxisHorizontal"), TEXT("Sculpt") }, /*bSequential=*/true); break;
    case EPTTutStep::Alt:     SetHintsFor({ TEXT("SurfaceSnap"), TEXT("Sculpt") }, /*bSequential=*/true); break;
    case EPTTutStep::Erase:   SetHintsFor({ TEXT("ModeErase"), TEXT("Sculpt") }, /*bSequential=*/true); break;
    case EPTTutStep::Undo:    SetHintsFor({ TEXT("Undo") }, /*bSequential=*/true); break;
    case EPTTutStep::Clear:   SetHintsFor({ TEXT("ClearAll") }, /*bSequential=*/true); break;
    case EPTTutStep::Paint:   SetHintsFor({ TEXT("ModePaint"), TEXT("ColorPick"), TEXT("Sculpt") }, /*bSequential=*/true); break;
    case EPTTutStep::Eyes:    SetHintsFor({ TEXT("ModeEyes"), TEXT("Sculpt") }, /*bSequential=*/true); break;
    case EPTTutStep::Iglu:    SetHintsFor({ TEXT("ModeAdd"), TEXT("CycleShape"), TEXT("RotateShape"), TEXT("BrushSize") }, /*bSequential=*/true, /*bGate=*/false); break;
    case EPTTutStep::Hongo:   SetHintsFor({ TEXT("CycleShape"), TEXT("AxisVertical"), TEXT("BrushSize"), TEXT("ModePaint") }, /*bSequential=*/true, /*bGate=*/false); break;
    case EPTTutStep::Perro:   SetHintsFor({}); break; // "Enter — sacar la foto" aparece al terminar el tiempo
    case EPTTutStep::Photo:   SetHintsFor({ TEXT("PhotoOrbit"), TEXT("PhotoShoot") }, /*bSequential=*/true, /*bGate=*/false); break;
    default: break;
    }
    UE_LOG(LogPTTutorial, Log, TEXT("Lección %d"), (int32)S);
}

void APTTutorialDirector::Tick(float Dt)
{
    Super::Tick(Dt);
    if (bExiting) return;
    if (!bStarted)
    {
        // Esperar a que estén el jugador, su cámara y el cubo (y un respiro para que crezca).
        StepTime += Dt;
        if (StepTime < 1.2f || !SetupWorld()) return;
        bStarted = true;
        if (bQuickMode) QuickBeginGuess(); // intro "adiviná la palabra" (árbol) → después la jirafa
        else            EnterStep(EPTTutStep::Intro);
        return;
    }
    TickSculpi(Dt);
    // Flujo rápido aparte (no toca la máquina del avanzado). EXCEPTO la foto final: ahí hacemos handoff
    // y dejamos correr TickStep (maneja Photo y End).
    if (bQuickMode && QState != EPTQuick::Photo) { TickChips(); QuickTick(Dt); return; }
    if (APTSculptPlayerController* P = PC.Get())
    {
        const float Yaw = P->GetControlRotation().Yaw;
        TotalYaw += FMath::Abs(FMath::FindDeltaAngleDegrees(LastTotalYaw, Yaw));
        LastTotalYaw = Yaw;
    }
    TickChips();
    TickAim();
    TickTips();
    if (Widget && PC.IsValid()) Widget->SetDockLeft(PC->IsShapeRadialOpen() || PC->IsColorPickerOpen());
    if (APTSculptPlayerController* P = PC.Get())
        if (Widget) Widget->SetPauseOptions(P->IsEscapeMenuOpen() && Step < EPTTutStep::Photo, Step == EPTTutStep::Perro && bPerroTimeUp);
    TickStep(Dt);
}

void APTTutorialDirector::TickStep(float Dt)
{
    StepTime += Dt;
    APTSculptPlayerController* P = PC.Get();
    if (!P) return;

    if (bStepDone)
    {
        // Esperar a que termine de escribir y dar tiempo de leer (según el largo de la frase).
        if (Widget && Widget->IsTyping()) DoneTimer = 0.f; else DoneTimer += Dt;
        if (DoneTimer > ReadTime()) NextStep();
        return;
    }

    // Medir 5 veces por segundo (las guías tienen cientos de puntos).
    static float MeasureAccum = 0.f;
    MeasureAccum += Dt;
    const bool bMeasure = MeasureAccum >= 0.2f;
    if (bMeasure) MeasureAccum = 0.f;
    auto Pct = [](float F) { return FText::Format(PTText::Get(TEXT("TUT_PCT")), FText::AsNumber(FMath::RoundToInt(FMath::Clamp(F, 0.f, 1.f) * 100.f))); };
    // Con teclas pendientes, la barra no llega al 100% (y la lección no termina): falta usar los controles.
    auto Show = [this](float F, const FText& L)
    {
        if (FMath::Abs(F - LastProgress) > 0.02f) { LastProgress = F; LastAdvanceTime = StepTime; }
        if (!Widget) return;
        if (ChipsPending() && F >= 0.95f)
            Widget->SetProgress(0.95f, FText::Format(PTText::Get(TEXT("TUT_PCT")), FText::AsNumber(95)));
        else
            Widget->SetProgress(F, L);
    };

    switch (Step)
    {
    case EPTTutStep::Intro:
        // Dos frases seguidas.
        if (Widget && !Widget->IsTyping())
        {
            LineWait += Dt;
            if (LineWait > ReadTime())
            {
                LineWait = 0.f;
                if (++LineIndex == 1) Say(TEXT("TUT_INTRO_2"));
                else NextStep();
            }
        }
        break;

    case EPTTutStep::Look:
    {
        const float Yaw = P->GetControlRotation().Yaw;
        AccYaw += FMath::Abs(FMath::FindDeltaAngleDegrees(LastYaw, Yaw));
        LastYaw = Yaw;
        Show(AccYaw / 200.f, Pct(AccYaw / 200.f));
        if (AccYaw >= 200.f) CompleteStep();
        break;
    }
    case EPTTutStep::Move:
    {
        const float D = P->GetPawn() ? (float)FVector::Dist2D(P->GetPawn()->GetActorLocation(), RingA) : MoveStartDist;
        const float F = 1.f - FMath::Clamp((D - 150.f) / FMath::Max(1.f, MoveStartDist - 150.f), 0.f, 1.f);
        Show(F, Pct(F));
        if (PawnNear(RingA, 170.f, 220.f)) CompleteStep();
        break;
    }
    case EPTTutStep::FlyUp:
    {
        const float Z0 = (float)RingA.Z, Z1 = (float)RingB.Z;
        const float F = P->GetPawn() ? FMath::Clamp(((float)P->GetPawn()->GetActorLocation().Z - Z0) / (Z1 - Z0), 0.f, 1.f) : 0.f;
        Show(F, Pct(F));
        if (PawnNear(RingB, 260.f, 110.f)) CompleteStep();
        break;
    }
    case EPTTutStep::FlyDown:
    {
        const float Z0 = (float)RingB.Z, Z1 = (float)RingC.Z;
        const float F = P->GetPawn() ? FMath::Clamp(((float)P->GetPawn()->GetActorLocation().Z - Z0) / (Z1 - Z0), 0.f, 1.f) : 0.f;
        Show(F, Pct(F));
        if (PawnNear(RingC, 260.f, 110.f)) CompleteStep();
        break;
    }
    case EPTTutStep::Add:
        if (bMeasure)
        {
            const float F = MeasureFill(0) / LessonFill;
            Show(F, Pct(F));
            if (F >= 1.f) CompleteStep();
        }
        break;
    case EPTTutStep::Size:
    {
        if (P->StampSize >= SizeStart + 60.f || P->StampSize >= P->MaxSize - 1.f) bSizeBigger = true;
        if (bSizeBigger && P->StampSize <= SizeStart - 40.f) bSizeSmaller = true;
        if (bSizeBigger && P->StampSize <= P->MinSize + 1.f) bSizeSmaller = true;
        const float F = (bSizeBigger ? 0.5f : 0.f) + (bSizeSmaller ? 0.5f : 0.f);
        Show(F, Pct(F));
        if (bSizeBigger && bSizeSmaller) CompleteStep();
        break;
    }
    case EPTTutStep::Shapes:
        if (bMeasure)
        {
            float F = MeasureFill(0) / LessonFill;
            if (P->StampShape != EPTStampShape::Cube) F = FMath::Min(F, 0.5f); // con el cubo
            Show(F, Pct(F));
            if (F >= 1.f) CompleteStep();
        }
        break;
    case EPTTutStep::Rotate:
        if (P->IsRotatingShape() || !P->StampRotation.IsNearlyZero(1.f)) bSawRotate = true;
        if (bMeasure)
        {
            float F = MeasureFill(0) / 0.55f;
            if (!bSawRotate) F = FMath::Min(F, 0.5f);
            Show(F, Pct(F));
            if (F >= 1.f) CompleteStep();
        }
        break;
    case EPTTutStep::Squash:
        if (!P->StampScale.Equals(FVector::OneVector, 0.01f)) bSawScale = true;
        if (bMeasure)
        {
            float F = MeasureFill(0) / 0.55f;
            if (!bSawScale) F = FMath::Min(F, 0.5f);
            Show(F, Pct(F));
            if (F >= 1.f) CompleteStep();
        }
        break;
    case EPTTutStep::Lines:
        if (P->IsStamping() && P->IsAxisLockActive()) bSawAxisStroke = true;
        if (bMeasure)
        {
            const float A = MeasureFill(0) / 0.5f, B = MeasureFill(1) / 0.5f;
            float F = (FMath::Min(A, 1.f) + FMath::Min(B, 1.f)) * 0.5f;
            if (!bSawAxisStroke) F = FMath::Min(F, 0.5f);
            Show(F, Pct(F));
            if (F >= 1.f) CompleteStep();
        }
        break;
    case EPTTutStep::Alt:
        if (bMeasure)
        {
            const bool bDetail = Volume.IsValid() && Volume->GetDetailMeshes().Num() > DetailStart;
            const float F = (bDetail ? 0.5f : 0.f) + 0.5f * FMath::Min(1.f, MeasureFill(0) / 0.25f);
            Show(F, Pct(F));
            if (bDetail && F >= 1.f) CompleteStep();
        }
        break;
    case EPTTutStep::Erase:
        if (bMeasure && StepTime > 0.5f)
        {
            const float F = (1.f - MeasureFill(0)) / 0.65f;
            Show(F, Pct(F));
            if (F >= 1.f) CompleteStep();
        }
        break;
    case EPTTutStep::Undo:
        if (bUndoDone) CompleteStep();
        break;
    case EPTTutStep::Clear:
        if (P->IsClearHeld()) Show(P->GetClearHoldProgress(), Pct(P->GetClearHoldProgress()));
        if (bClearDone) CompleteStep();
        break;
    case EPTTutStep::Paint:
        if (P->IsColorPickerOpen()) bSawPicker = true;
        if (bMeasure)
        {
            const float Painted = MeasurePainted(FVector(CanvasCenter.X, CanvasCenter.Y, FloorZ + 170.f), 160.f);
            // El color se guarda solo al elegirlo: alcanza con elegir uno y pintar.
            const float F = (bSawPicker ? 0.3f : 0.f) + 0.7f * FMath::Min(1.f, Painted / 0.35f);
            Show(F, Pct(F));
            if (bSawPicker && Painted >= 0.35f) CompleteStep();
            // Ya pintó pero no eligió color: Sculpi se lo recuerda una vez.
            if (!bPaintHintSaid && Painted >= 0.35f && !bSawPicker && !(Widget && Widget->IsTyping()))
            {
                bPaintHintSaid = true;
                Say(TEXT("TUT_PAINT_SAVE_HINT"));
            }
        }
        break;
    case EPTTutStep::Eyes:
    {
        const int32 Have = Volume.IsValid() ? Volume->GetEyeCount() - EyesStart : 0;
        const float F = FMath::Clamp(Have / 2.f, 0.f, 1.f);
        Show(F, Pct(F));
        if (Have >= 2) CompleteStep();
        break;
    }
    case EPTTutStep::Iglu:
        if (LineIndex == 0 && Widget && !Widget->IsTyping())
        {
            LineWait += Dt;
            if (LineWait > ReadTime()) { LineIndex = 1; Say(TEXT("TUT_IGLU")); }
        }
        if (bMeasure)
        {
            const float F = MeasureFill(0) / WordFill;
            Show(F, FText::Format(PTText::Get(TEXT("TUT_WORD_PCT")), PTText::Get(TEXT("TUT_W_IGLU")), FText::AsNumber(FMath::RoundToInt(FMath::Min(F, 1.f) * 100.f))));
            if (F >= 1.f && LineIndex == 1) CompleteStep();
        }
        break;
    case EPTTutStep::Hongo:
        if (bMeasure)
        {
            const float Stem = FMath::Min(1.f, MeasureFill(0) / 0.55f);
            const float Cap = FMath::Min(1.f, MeasureFill(1) / 0.25f); // cualquier sombrero ancho encima del tallo
            if (Stem >= 1.f && LineIndex == 0) { LineIndex = 1; Say(TEXT("TUT_HONGO_CAP")); }
            const float F = 0.5f * Stem + 0.5f * Cap;
            Show(F, FText::Format(PTText::Get(TEXT("TUT_WORD_PCT")), PTText::Get(TEXT("TUT_W_HONGO")), FText::AsNumber(FMath::RoundToInt(F * 100.f))));
            if (Stem >= 1.f && Cap >= 1.f) CompleteStep();
        }
        break;
    case EPTTutStep::Perro:
    {
        PerroLeft -= Dt;
        Show(FMath::Clamp(PerroLeft / PerroSeconds, 0.f, 1.f),
             FText::Format(PTText::Get(TEXT("TUT_SECONDS")), FText::AsNumber(FMath::Max(0, FMath::CeilToInt(PerroLeft)))));
        // Como en una partida: se esculpe hasta que termina el tiempo. Recién ahí "Enter — sacar la foto".
        if (!bPerroTimeUp && PerroLeft <= 0.f)
        {
            if (!HasAnyClay())
            {
                // Sin nada esculpido: 30 s más (no tiene sentido sacarle foto a un cubo vacío).
                PerroLeft = 30.f;
                if (Sculpi) Sculpi->Multicast_ShowChatBubble(PTText::GetStr(TEXT("TUT_TIP_SCULPT_FIRST")), false);
            }
            else
            {
                bPerroTimeUp = true;
                if (APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr)
                    G->TurnPhase = EPTTurnPhase::TurnEnd; // ¡tiempo! ya no se esculpe
                PlaySfx(SuccessSound, 0.5f);
                Say(TEXT("TUT_PERRO_TIME"));
                SetHintsFor({ TEXT("Finish") }, /*bSequential=*/true, /*bGate=*/false);
            }
        }
        if (bPerroTimeUp && P->WasInputKeyJustPressed(EKeys::Enter)) OnDonePerro();
        break;
    }
    case EPTTutStep::Photo:
    {
        if (bPhotoAiming)
        {
            // Encuadre: girar alrededor del perro; Enter / clic / A para disparar.
            TickPhotoOrbit(Dt);
            const bool bShoot = StepTime > 1.2f && (P->WasInputKeyJustPressed(EKeys::Enter) || P->WasInputKeyJustPressed(EKeys::LeftMouseButton) ||
                                                    P->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Bottom) || P->WasInputKeyJustPressed(EKeys::Gamepad_RightTrigger));
            if (bShoot)
            {
                bPhotoAiming = false;
                CountdownN = 3; CountdownT = 0.f;
                if (Widget) { Widget->ShowCountdown(3); Widget->SetHints(TArray<FPTTutHint>()); }
                PlaySfx(CountdownSound);
            }
        }
        else if (CountdownN > 0)
        {
            CountdownT += Dt;
            if (CountdownT >= 1.f)
            {
                CountdownT = 0.f;
                --CountdownN;
                if (Widget) Widget->ShowCountdown(CountdownN);
                if (CountdownN > 0) PlaySfx(CountdownSound);
                else
                {
                    // ¡Foto! Sin Sculpi ni interfaz; la captura llega en OnScreenshot.
                    SetPhotoHidden(true);
                    bShotRequested = true;
                    LineWait = 0.f;
                }
            }
        }
        else if (bShotRequested && !ShotDelegate.IsValid())
        {
            LineWait += Dt;
            if (LineWait > 0.35f) // un par de frames para que lo oculto ya no esté en pantalla
            {
                ShotDelegate = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &APTTutorialDirector::OnScreenshot);
                FScreenshotRequest::RequestScreenshot(/*bShowUI=*/false);
            }
        }
        break;
    }
    case EPTTutStep::End:
        // Con joystick, teclado en pantalla para el nombre (hasta guardar).
        if (Widget && !bPhotoSaved)
            if (const UPTGamepadUINavigator* Nav = GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTGamepadUINavigator>() : nullptr)
                if (Nav->IsUsingGamepad()) Widget->SetOnScreenKeyboard(true);
        // Ya guardada: "Continuar" (botón) o Enter. Antes de guardar, Enter confirma el nombre (lo maneja el cuadro).
        if (bPhotoSaved && StepTime > 1.5f && P->WasInputKeyJustPressed(EKeys::Enter)) OnContinue();
        break;
    }
}

void APTTutorialDirector::DevJumpTo(int32 StepIndex)
{
    if (!bStarted) return;
    const int32 Max = (int32)EPTTutStep::Photo;
    const EPTTutStep S = (EPTTutStep)FMath::Clamp(StepIndex, 0, Max);
    // Para probar la foto sin esculpir: un "perro" de prueba.
    if (S == EPTTutStep::Photo)
    {
        const FVector C(CanvasCenter.X, CanvasCenter.Y, FloorZ);
        ClearClay();
        PlaceClay(EPTStampShape::Capsule, C + FVector(0, 0, 160.f), 200.f, FVector(1.f, 1.f, 2.f), FRotator(90.f, Right.Rotation().Yaw, 0.f));
        PlaceClay(EPTStampShape::Sphere, C + Right * 200.f + FVector(0, 0, 260.f), 170.f);
        for (float Off : { -110.f, 110.f })
            PlaceClay(EPTStampShape::Cylinder, C + Right * Off + FVector(0, 0, 60.f), 70.f, FVector(1.f, 1.f, 1.6f));
    }
    EnterStep(S);
}

void APTTutorialDirector::DevShoot()
{
    if (Step != EPTTutStep::Photo || !bPhotoAiming) return;
    bPhotoAiming = false;
    CountdownN = 3; CountdownT = 0.f;
    if (Widget) { Widget->ShowCountdown(3); Widget->SetHints(TArray<FPTTutHint>()); }
}

void APTTutorialDirector::DevFillGhosts()
{
    // Rellena por código las guías visibles (y la invisible del hongo) para probar la medición.
    for (const FPTGhost& G : Ghosts)
        for (const FPTGhostPart& Part : G.Parts)
            if (Part.Shape != EPTStampShape::Torus) PlaceClay(Part.Shape, Part.Center, Part.Size, Part.Scale, Part.Rot);
    if (Step == EPTTutStep::Erase) ClearClay();
    if (Step == EPTTutStep::Undo) bUndoDone = true;
    if (Step == EPTTutStep::Clear) bClearDone = true;
    if (Step == EPTTutStep::Eyes && Volume.IsValid())
        for (int32 i = 0; i < 2; ++i) Volume->AddEye(FVector(CanvasCenter.X + i * 60.f, CanvasCenter.Y, FloorZ + 250.f), 30.f);
}

// ── Perro + foto ────────────────────────────────────────────────────────────

bool APTTutorialDirector::HasAnyClay() const
{
    if (!Volume.IsValid()) return false;
    const int32 N = 12;
    for (int32 i = 0; i < N; ++i)
        for (int32 j = 0; j < N; ++j)
            for (int32 k = 0; k < N; ++k)
            {
                const FVector Pt = CanvasCenter + FVector((i + 0.5f) / N * 2.f - 1.f, (j + 0.5f) / N * 2.f - 1.f, (k + 0.5f) / N * 2.f - 1.f) * CanvasExt;
                if (Volume->SampleWorldDensity(Pt) > 0.f) return true;
            }
    return false;
}

void APTTutorialDirector::OnDonePerro()
{
    if (Step != EPTTutStep::Perro) return;
    if (!bPerroTimeUp) return; // la foto es al terminar el tiempo
    // Cerrar el menú de pausa si se terminó desde ahí.
    if (APTSculptPlayerController* P = PC.Get())
        if (P->IsEscapeMenuOpen()) P->ConsoleCommand(TEXT(""), false);
    PlaySfx(SuccessSound, 0.5f);
    EnterStep(EPTTutStep::Photo);
}

void APTTutorialDirector::StartPhoto()
{
    ClearGhosts(); // que en la foto solo salga la escultura
    CountdownN = 0;
    bShotRequested = false;
    bPhotoSaved = false;
    LineWait = 0.f;
    // Ya no se esculpe (el clic es para disparar) ni se mueve el personaje: la cámara es de la foto.
    if (APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr)
        G->TurnPhase = EPTTurnPhase::TurnEnd;
    if (APTSculptPlayerController* P = PC.Get())
    {
        P->SetIgnoreLookInput(true);
        P->SetIgnoreMoveInput(true);
        if (AActor* Prev = P->GetBrushPreviewActor()) Prev->SetActorHiddenInGame(true);
        if (APawn* Pawn = P->GetPawn()) Pawn->SetActorHiddenInGame(true); // tu personaje no sale en la foto
    }
    if (Sculpi) Sculpi->SetActorHiddenInGame(true); // Sculpi tampoco (sigue hablando en el cuadro de diálogo)
    if (APTSculptVolume* V = Volume.Get()) V->SetFrameMeshesHidden(true); // la foto ignora el marco de la zona
    FramePhotoCamera();
    bPhotoAiming = true;
    Say(bQuickMode ? TEXT("TUT_PHOTO_GIRAFFE") : TEXT("TUT_PHOTO"));
}

void APTTutorialDirector::FramePhotoCamera()
{
    UWorld* W = GetWorld();
    APTSculptPlayerController* P = PC.Get();
    if (!W || !P || !Volume.IsValid()) return;

    // Dónde está la escultura: muestrear el cubo y quedarse con los puntos sólidos.
    FBox Clay(ForceInit);
    const int32 N = 22;
    for (int32 i = 0; i < N; ++i)
        for (int32 j = 0; j < N; ++j)
            for (int32 k = 0; k < N; ++k)
            {
                const FVector Pt = CanvasCenter + FVector((i + 0.5f) / N * 2.f - 1.f, (j + 0.5f) / N * 2.f - 1.f, (k + 0.5f) / N * 2.f - 1.f) * CanvasExt;
                if (Volume->SampleWorldDensity(Pt) > 0.f) Clay += Pt;
            }
    if (!Clay.IsValid) Clay = FBox(CanvasCenter - FVector(150.f), CanvasCenter + FVector(150.f));
    OrbitCenter = Clay.GetCenter();
    const float Radius = FMath::Max(120.f, (float)Clay.GetExtent().Size());

    // Arranca desde donde miraba el jugador (horizontal), un poco arriba, a la distancia justa.
    const FVector CamLoc = P->PlayerCameraManager ? P->PlayerCameraManager->GetCameraLocation() : OrbitCenter - FVector(600, 0, 0);
    FVector Dir = OrbitCenter - CamLoc; Dir.Z = 0.f;
    Dir = Dir.IsNearlyZero() ? FVector::ForwardVector : Dir.GetSafeNormal();
    OrbitYaw = OrbitYaw0 = Dir.Rotation().Yaw;
    OrbitPitch = 18.f;
    OrbitDist = OrbitDist0 = Radius / FMath::Tan(FMath::DegreesToRadians(25.f)) * 1.35f + 80.f;

    FActorSpawnParameters SP;
    SP.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    PhotoCam = W->SpawnActor<ACameraActor>(OrbitCenter, FRotator::ZeroRotator, SP);
    if (PhotoCam)
    {
        PhotoCam->GetCameraComponent()->SetFieldOfView(50.f);
        PhotoCam->GetCameraComponent()->bConstrainAspectRatio = false;
        PlaceOrbitCamera();
        P->SetViewTargetWithBlend(PhotoCam, 0.8f, VTBlend_Cubic);
    }
}

void APTTutorialDirector::PlaceOrbitCamera()
{
    if (!PhotoCam) return;
    const FRotator R(-OrbitPitch, OrbitYaw, 0.f);
    PhotoCam->SetActorLocationAndRotation(OrbitCenter - R.Vector() * OrbitDist, R);
}

void APTTutorialDirector::TickPhotoOrbit(float Dt)
{
    APTSculptPlayerController* P = PC.Get();
    if (!P || !PhotoCam) return;
    // Riel en anillo alrededor del perro: A/D (o stick izquierdo) dan la vuelta; W/S suben o bajan la cámara.
    float Side = P->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
    float Up   = P->GetInputAnalogKeyState(EKeys::Gamepad_LeftY);
    if (P->IsInputKeyDown(EKeys::D)) Side += 1.f;
    if (P->IsInputKeyDown(EKeys::A)) Side -= 1.f;
    if (P->IsInputKeyDown(EKeys::W)) Up += 1.f;
    if (P->IsInputKeyDown(EKeys::S)) Up -= 1.f;
    OrbitYaw   = FRotator::NormalizeAxis(OrbitYaw - FMath::Clamp(Side, -1.f, 1.f) * 75.f * Dt);
    OrbitPitch = FMath::Clamp(OrbitPitch + FMath::Clamp(Up, -1.f, 1.f) * 35.f * Dt, 2.f, 55.f);
    // Rueda / gatillos: acercar o alejar un poco.
    if (P->WasInputKeyJustPressed(EKeys::MouseScrollUp))   OrbitDist *= 0.92f;
    if (P->WasInputKeyJustPressed(EKeys::MouseScrollDown)) OrbitDist *= 1.08f;
    OrbitDist += (P->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis)) * OrbitDist0 * 0.6f * Dt;
    OrbitDist = FMath::Clamp(OrbitDist, OrbitDist0 * 0.7f, OrbitDist0 * 1.5f);
    PlaceOrbitCamera();
}

void APTTutorialDirector::SetPhotoHidden(bool bHide)
{
    if (Sculpi) Sculpi->SetActorHiddenInGame(bHide);
    if (APTSculptPlayerController* P = PC.Get())
    {
        if (APawn* Pawn = P->GetPawn()) Pawn->SetActorHiddenInGame(bHide);
        if (AActor* Prev = P->GetBrushPreviewActor()) Prev->SetActorHiddenInGame(true);
        P->SetGameplayHUDVisible(!bHide);
    }
    if (Widget)
    {
        Widget->SetDialogVisible(!bHide);
        Widget->ShowCountdown(0);
    }
}

void APTTutorialDirector::OnScreenshot(int32 W, int32 H, const TArray<FColor>& Pixels)
{
    UGameViewportClient::OnScreenshotCaptured().Remove(ShotDelegate);
    ShotDelegate.Reset();
    if (W <= 0 || H <= 0 || Pixels.Num() != W * H) { EnterStep(EPTTutStep::End); return; }

    // La foto en una textura (BGRA, opaca) con VIÑETA: los bordes se oscurecen suave hacia negro.
    PhotoTex = UTexture2D::CreateTransient(W, H, PF_B8G8R8A8);
    if (PhotoTex)
    {
        FTexture2DMipMap& Mip = PhotoTex->GetPlatformData()->Mips[0];
        FColor* Dst = static_cast<FColor*>(Mip.BulkData.Lock(LOCK_READ_WRITE));
        const float Cx = W * 0.5f, Cy = H * 0.5f;
        for (int32 y = 0; y < H; ++y)
            for (int32 x = 0; x < W; ++x)
            {
                const float Dx = (x - Cx) / Cx, Dy = (y - Cy) / Cy;
                const float D = FMath::Sqrt(Dx * Dx * 0.85f + Dy * Dy * 0.6f); // elipse: más en los costados
                const float K = 1.f - 0.75f * FMath::SmoothStep(0.55f, 1.15f, D);
                const FColor& S = Pixels[y * W + x];
                Dst[y * W + x] = FColor(uint8(S.R * K), uint8(S.G * K), uint8(S.B * K), 255);
            }
        Mip.BulkData.Unlock();
        PhotoTex->UpdateResource();
    }

    PlaySfx(ShutterSound, 0.7f);
    if (Widget) Widget->Flash();

    // Ponerle nombre y guardar (Steam). El HUD de juego queda oculto hasta salir.
    Step = EPTTutStep::End;
    StepTime = 0.f;
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>()) GI->bTutorialWantsCursor = true;
    if (Widget)
    {
        Widget->SetHints(TArray<FPTTutHint>());
        Widget->SetProgress(-1.f, FText::GetEmpty());
        Widget->SetAim(FText::GetEmpty(), FLinearColor::White);
        Widget->SetDialogVisible(true);
        Widget->ShowPhoto(PhotoTex, PTText::Get(bQuickMode ? TEXT("TUT_MY_GIRAFFE") : TEXT("TUT_MY_DOG")));
        Widget->Say(PTText::Get(bQuickMode ? TEXT("TUT_NAME_GIRAFFE") : TEXT("TUT_NAME_DOG")));
    }
}

void APTTutorialDirector::OnSavePhoto()
{
    if (Step != EPTTutStep::End || bPhotoSaved || !Widget) return;
    FText Name = FText::TrimPrecedingAndTrailing(Widget->GetPhotoName());
    if (Name.IsEmpty()) Name = PTText::Get(bQuickMode ? TEXT("TUT_MY_GIRAFFE") : TEXT("TUT_MY_DOG"));
    SaveFramedPhoto(Name);
}

void APTTutorialDirector::SaveFramedPhoto(const FText& Caption)
{
    APTSculptPlayerController* P = PC.Get();
    FText SavedMsg = PTText::Get(TEXT("TUT_PHOTO_FAIL"));

    // Logo: forzarlo entero en memoria antes de dibujar (si no, puede faltar en la foto).
    UTexture2D* Logo = LogoTexture.LoadSynchronous();
    if (Logo) { Logo->SetForceMipLevelsToBeResident(30.f); Logo->WaitForStreaming(); }

    // El marco se dibuja a una textura con el mismo widget que se ve en pantalla (texto fijo, no editable).
    TArray<FColor> Px;
    int32 W = 0, H = 0;
    if (P && PhotoTex)
    {
        UPTTutorialPolaroid* Card = CreateWidget<UPTTutorialPolaroid>(P, UPTTutorialPolaroid::StaticClass());
        Card->Setup(PhotoTex, Caption, Logo, 1280.f, /*bEditable=*/false);
        TSharedRef<SWidget> Slate = Card->TakeWidget();
        Slate->SlatePrepass(1.f);
        const FVector2D Size = Slate->GetDesiredSize();
        W = FMath::RoundToInt(Size.X); H = FMath::RoundToInt(Size.Y);
        if (W > 16 && H > 16)
        {
            // Sin corrección de gamma: con ella la foto salía lavada (blanquecina).
            FWidgetRenderer* Renderer = new FWidgetRenderer(/*bUseGammaCorrection=*/false, /*bInClearTarget=*/true);
            UTextureRenderTarget2D* RT = FWidgetRenderer::CreateTargetFor(Size, TF_Bilinear, /*bUseGammaCorrection=*/true); // destino sRGB: la lectura sale con los colores de pantalla
            if (RT)
            {
                RT->ClearColor = FLinearColor::Black;
                Renderer->DrawWidget(RT, Slate, Size, 0.f);
                Renderer->DrawWidget(RT, Slate, Size, 0.f); // 2.ª pasada: con las fuentes ya cacheadas
                FlushRenderingCommands();
                if (FTextureRenderTargetResource* Res = RT->GameThread_GetRenderTargetResource()) Res->ReadPixels(Px);
            }
            FlushRenderingCommands();
            delete Renderer;
        }
    }

    if (Px.Num() == W * H && W > 0)
    {
        for (FColor& C : Px) C.A = 255;
        bool bSteam = false;
#if PT_WITH_STEAM
        if (SteamAPI_IsSteamRunning() && SteamScreenshots())
        {
            TArray<uint8> RGB;
            RGB.SetNumUninitialized(W * H * 3);
            for (int32 i = 0; i < W * H; ++i) { RGB[i * 3] = Px[i].R; RGB[i * 3 + 1] = Px[i].G; RGB[i * 3 + 2] = Px[i].B; }
            const ::ScreenshotHandle Shot = SteamScreenshots()->WriteScreenshot(RGB.GetData(), RGB.Num(), W, H);
            if (Shot != INVALID_SCREENSHOT_HANDLE)
            {
                SteamScreenshots()->SetLocation(Shot, TCHAR_TO_UTF8(*Caption.ToString()));
                bSteam = true;
                bSavedToSteam = true;
                SavedMsg = PTText::Get(TEXT("TUT_PHOTO_STEAM"));
            }
        }
#endif
        // Sin Steam: PNG en la carpeta de capturas. En desarrollo, siempre una copia (para revisarla).
        bool bWritePng = !bSteam;
#if !UE_BUILD_SHIPPING
        bWritePng = true;
#endif
        if (bWritePng)
        {
            IImageWrapperModule& IWM = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
            TSharedPtr<IImageWrapper> Png = IWM.CreateImageWrapper(EImageFormat::PNG);
            if (Png.IsValid() && Png->SetRaw(Px.GetData(), Px.Num() * sizeof(FColor), W, H, ERGBFormat::BGRA, 8))
            {
                const FString Dir = FPaths::ScreenShotDir();
                IFileManager::Get().MakeDirectory(*Dir, true);
                const FString File = Dir / FString::Printf(TEXT("Sculpturillo_MiPerro_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
                const TArray64<uint8>& Data = Png->GetCompressed(100);
                if (FFileHelper::SaveArrayToFile(Data, *File))
                {
                    if (!bSteam) SavedMsg = PTText::Get(TEXT("TUT_PHOTO_SAVED")); // "¡Imagen guardada!" (sin la ruta: queda feo)
                    UE_LOG(LogPTTutorial, Log, TEXT("Foto del perro (\"%s\") guardada: %s"), *Caption.ToString(), *File);
                }
            }
        }
    }

    bPhotoSaved = true;
    StepTime = 0.f;
    if (Widget)
    {
        Widget->ShowPhotoSaved(SavedMsg, bSavedToSteam);
        Widget->Say(PTText::Get(TEXT("TUT_END")));
    }
}

void APTTutorialDirector::OnSharePhoto()
{
    // El administrador de capturas de Steam del juego: desde ahí se sube a la comunidad con un clic.
    uint32 AppId = 5114580;
#if PT_WITH_STEAM
    if (SteamAPI_IsSteamRunning() && SteamUtils()) AppId = SteamUtils()->GetAppID();
#endif
    const FString Url = FString::Printf(TEXT("steam://open/screenshots/%u"), AppId);
    UE_LOG(LogPTTutorial, Log, TEXT("Compartir foto: %s"), *Url);
    FPlatformProcess::LaunchURL(*Url, nullptr, nullptr);
}

// ── Salir ───────────────────────────────────────────────────────────────────

void APTTutorialDirector::OnSkip()     { Finish(); }
void APTTutorialDirector::OnContinue()
{
    if (Step == EPTTutStep::End && bPhotoSaved && StepTime > 1.f)
    {
        if (bQuickMode) QuickShowChoice(); // rápido: tras la foto, ofrecer el avanzado (en vez de salir)
        else            Finish();
    }
}

void APTTutorialDirector::Finish()
{
    if (bExiting) return;
    bExiting = true;
    UE_LOG(LogPTTutorial, Log, TEXT("Tutorial terminado (lección %d)."), (int32)Step);
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>())
    {
        GI->bTutorialWantsCursor = false;
        GI->ExitTutorial();
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tutorial RÁPIDO ("imitá a Sculpi"): guía para hacer una JIRAFA. Sculpi ESCULPE cada parte como un
// jugador (su preview/brocha se mueve y el trazo se forma en AMARILLO); después el trazo se vuelve
// TRANSPARENTE (guía) y el jugador lo copia. La arcilla del jugador se ACUMULA → se construye la jirafa.
// Lecciones: 0 color amarillo, 1 patas, 2 torso (escala brocha), 3 cuello (cilindro), 4 cabeza+ojos.
// Luego experimentación libre → Enter → foto. Reusa los helpers; no toca la máquina del avanzado.
// ─────────────────────────────────────────────────────────────────────────────

void APTTutorialDirector::QuickBuildParts(int32 Lesson, TArray<FPTGhostPart>& Out, FLinearColor& OutColor) const
{
    Out.Reset();
    OutColor = FLinearColor(1.0f, 0.82f, 0.08f); // amarillo jirafa
    const FVector C(CanvasCenter.X, CanvasCenter.Y, FloorZ);
    const FVector R = Right, F = Fwd;
    auto Sp = [&](const FVector& P, float S) { Out.Add(FPTGhostPart{ EPTStampShape::Sphere, P, S }); };

    switch (Lesson)
    {
    case 1: // 4 patas: cada una 3 bolas (abajo→arriba), en orden de pata
    {
        const FVector Legs[4] = { R * 130.f + F * 85.f, R * 130.f - F * 85.f, R * -130.f + F * 85.f, R * -130.f - F * 85.f };
        for (const FVector& L : Legs)
            for (int32 z = 0; z < 3; ++z) Sp(C + L + FVector(0, 0, 80.f + z * 95.f), 160.f);
        break;
    }
    case 2: // torso: línea de bolas grandes
        for (int32 i = 0; i < 4; ++i) Sp(C + R * (-150.f + i * 100.f) + FVector(0, 0, 360.f), 300.f);
        break;
    case 3: // cuello: sube hacia adelante (lado derecho)
        for (int32 i = 0; i < 4; ++i) Sp(C + R * (170.f + i * 48.f) + FVector(0, 0, 430.f + i * 100.f), 180.f);
        break;
    case 4: // cabeza: 2 bolas arriba del cuello
        Sp(C + R * 380.f + FVector(0, 0, 820.f), 210.f);
        Sp(C + R * 450.f + FVector(0, 0, 835.f), 170.f);
        break;
    }
}

void APTTutorialDirector::QuickShowBrushAt(const FVector& P, float Size, const FLinearColor& Col)
{
    if (!QBrush)
    {
        QBrush = NewObject<UProceduralMeshComponent>(this);
        QBrush->SetupAttachment(Root);
        QBrush->RegisterComponent();
        QBrush->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        QBrush->SetCastShadow(false);
        QBrush->SetTranslucentSortPriority(10);
        TArray<FVector> V, N; TArray<int32> T;
        APTSculptVolume::BuildStampPreview(EPTStampShape::Sphere, 100.f, 12.f, V, T, N);
        QBrush->CreateMeshSection(0, V, T, N, {}, {}, {}, false);
    }
    // Material del preview = el de las guías, teñido con el COLOR de la escultura (antes salía gris default).
    if (!QBrushMID)
    {
        UMaterialInterface* Base = GhostMaterial.LoadSynchronous();
        if (!Base && PC.IsValid()) Base = PC->GetGhostMaterial();
        if (Base) { QBrushMID = UMaterialInstanceDynamic::Create(Base, this); KeepAlive.Add(QBrushMID); }
    }
    if (QBrushMID)
    {
        QBrushMID->SetVectorParameterValue(TEXT("Color"), Col * 1.7f);
        QBrushMID->SetScalarParameterValue(TEXT("GlowEnable"), 0.f);
        QBrush->SetMaterial(0, QBrushMID);
    }
    QBrush->SetWorldLocation(P);
    QBrush->SetWorldScale3D(FVector(FMath::Max(Size, 40.f) / 100.f));
    QBrush->SetVisibility(true);
}

void APTTutorialDirector::QuickLockSculpt(bool bLock)
{
    if (APTSculptPlayerController* P = PC.Get()) P->bTutorialLockSculpt = bLock;
    bQSculpiAtWork = bLock; // mientras está bloqueado (demo), Sculpi "trabaja" frente al trazo
    if (!bLock) QuickHideDots();
}

void APTTutorialDirector::QuickHideDots()
{
    for (UProceduralMeshComponent* D : QDots) if (D) D->SetVisibility(false);
}

void APTTutorialDirector::QuickDemoVisual()
{
    // Línea PUNTEADA de la cara de Sculpi hasta la brocha que "esculpe" (muestra que él es quien esculpe).
    if (!QBrush || !QBrush->IsVisible() || !Sculpi) { QuickHideDots(); return; }
    const FVector BrushLoc = QBrush->GetComponentLocation();
    const FVector Head = Sculpi->GetActorLocation() + FVector(0.f, 0.f, 75.f);
    const int32 ND = 9;
    for (int32 i = 0; i < ND; ++i)
    {
        if (!QDots.IsValidIndex(i) || !QDots[i])
        {
            UProceduralMeshComponent* D = NewObject<UProceduralMeshComponent>(this);
            D->SetupAttachment(Root); D->RegisterComponent();
            D->SetCollisionEnabled(ECollisionEnabled::NoCollision); D->SetCastShadow(false);
            D->SetTranslucentSortPriority(11);
            TArray<FVector> V, N; TArray<int32> T;
            APTSculptVolume::BuildStampPreview(EPTStampShape::Sphere, 100.f, 20.f, V, T, N);
            D->CreateMeshSection(0, V, T, N, {}, {}, {}, false);
            D->SetWorldScale3D(FVector(0.22f)); // puntito
            if (QBrushMID) D->SetMaterial(0, QBrushMID);
            if (QDots.IsValidIndex(i)) QDots[i] = D; else QDots.Add(D);
            KeepAlive.Add(D);
        }
        const float t = (i + 1) / (float)(ND + 1);
        QDots[i]->SetWorldLocation(FMath::Lerp(Head, BrushLoc, t));
        QDots[i]->SetVisibility(true);
    }
}

void APTTutorialDirector::QuickSetGhostColor(const FLinearColor& Col)
{
    if (!Ghosts.IsValidIndex(QGhost)) return;
    Ghosts[QGhost].Color = Col;
    if (Ghosts[QGhost].MID)        Ghosts[QGhost].MID->SetVectorParameterValue(TEXT("Color"), Col * GhostIntensity);
    if (Ghosts[QGhost].OverlayMID) Ghosts[QGhost].OverlayMID->SetVectorParameterValue(TEXT("Color"), Col);
}

bool APTTutorialDirector::QuickLoadRecording(int32 Lesson)
{
    QRec.Reset();
    TArray<uint8> Bytes;
    if (!FFileHelper::LoadFileToArray(Bytes, *QTutRecPath(Lesson)) || Bytes.Num() == 0) return false;
    FMemoryReader Ar(Bytes);
    QTutRecSerialize(Ar, QRec);
    return QRec.Num() > 0;
}

void APTTutorialDirector::QuickBuildGhostFromRec()
{
    QParts.Reset();
    FVector LastC(FLT_MAX, FLT_MAX, FLT_MAX);
    FLinearColor Col(1.f, 0.82f, 0.08f);
    for (const FPTTutRecStamp& S : QRec)
    {
        if ((EPTEditMode)S.Mode != EPTEditMode::Add) continue; // la guía = geometría agregada
        const FVector C = QOrigin + S.Pos;
        if ((C - LastC).SizeSquared() < FMath::Square(FMath::Max(S.Size * 0.5f, 40.f))) continue; // dedup espacial
        LastC = C;
        QParts.Add(FPTGhostPart{ (EPTStampShape)S.Shape, C, S.Size, S.Scale, S.Rot });
        Col = S.Color;
    }
    QGhost = AddGhost(QParts, Col);
    if (QLesson == 4 && QParts.Num() > 0) QHeadCenter = QParts[0].Center;
}

void APTTutorialDirector::QuickBeginLesson(int32 Idx)
{
    QLesson = Idx;
    QTime = 0.f; QReveal = 0; QRevealT = 0.f; QRepIdx = 0; bQRecorded = false; QRec.Reset();
    bStepDone = false; DoneTimer = 0.f; QGhost = INDEX_NONE; QParts.Reset();
    if (Widget) { Widget->SetProgress(-1.f, FText::GetEmpty()); Widget->SetHints(TArray<FPTTutHint>()); }
    ClearGhosts(); // saca la guía anterior (NO la arcilla: la jirafa se acumula)
    if (QBrush) QBrush->SetVisibility(false);
    CaptureOrientation();

    if (Idx >= QNumLessons) { QuickBeginFree(); return; }

    if (Idx == 0) // color amarillo (sin demo): el jugador puede operar (elegir color)
    {
        QPhase = EPTQPhase::Copy;
        bQSawPicker = false;
        QuickLockSculpt(false);
        Say(TEXT("TUT_G_COLOR"));
        SetHintsFor({ TEXT("ColorPick") }, true, false);
        return;
    }

    // Al empezar a modelar la jirafa (patas = 1ª parte), LIMPIAR lo que el jugador haya ensuciado antes.
    if (Idx == 1) ClearClay();

    // Durante el demo (Sculpi explica/esculpe) el jugador NO puede esculpir, solo moverse.
    QuickLockSculpt(true);

    const TCHAR* Line = TEXT("TUT_G_LEGS");
    if (Idx == 2) Line = TEXT("TUT_G_TORSO");
    if (Idx == 3) Line = TEXT("TUT_G_NECK");
    if (Idx == 4) Line = TEXT("TUT_G_HEAD");

    // ¿Hay una GRABACIÓN para esta parte? → Sculpi la reproduce con arcilla REAL (snapshot antes/restore
    // después). Si no, cae al trazo SCRIPTED (bolas que se revelan).
    if (QuickLoadRecording(Idx))
    {
        bQRecorded = true;
        QOrigin = CanvasCenter; // los sellos se grabaron relativos al CENTRO del cubo
        if (APTSculptVolume* V = Volume.Get()) { V->SaveFieldState(QSnapshot); QEyesSnapshot = V->GetEyeCount(); }
        QPhase = EPTQPhase::Demo;
        Say(Line);
        return;
    }

    // Lecciones con trazo (scripted): construir el trazo, agregarlo como guía amarilla, ocultar todo y ANIMAR.
    FLinearColor Color;
    QuickBuildParts(Idx, QParts, Color);
    QGhost = AddGhost(QParts, Color);
    if (Idx == 4 && QParts.Num() > 0) QHeadCenter = QParts[0].Center;
    if (Ghosts.IsValidIndex(QGhost))
        for (UPrimitiveComponent* M : Ghosts[QGhost].Meshes) if (M) M->SetVisibility(false);
    QuickSetGhostColor(Color * 1.7f); // brillante (arcilla amarilla "recién puesta")
    QPhase = EPTQPhase::Demo;
    Say(Line);
}

void APTTutorialDirector::QuickStartCopy()
{
    QPhase = EPTQPhase::Copy; QTime = 0.f; bStepDone = false;
    QuickLockSculpt(false); // ahora te toca a vos: se desbloquea el esculpido
    if (QBrush) QBrush->SetVisibility(false);
    // El trazo se vuelve TRANSPARENTE (guía): color tenue. UpdateGhostColor lo pone verde al llenarse.
    QuickSetGhostColor(FLinearColor(0.95f, 0.8f, 0.2f, 1.f) * 0.5f);
    Say(TEXT("TUT_G_COPY"));
    switch (QLesson)
    {
    case 1: SetHintsFor({ TEXT("ModeAdd"), TEXT("Sculpt") }, true, false); break;
    case 2: SetHintsFor({ TEXT("BrushSize"), TEXT("Sculpt") }, true, false); break;
    case 3: SetHintsFor({ TEXT("CycleShape"), TEXT("Sculpt") }, true, false); break;
    case 4: SetHintsFor({ TEXT("Sculpt"), TEXT("ModeEyes") }, true, false);
            QEyesBase = Volume.IsValid() ? Volume->GetEyeCount() : 0; break;
    }
}

bool APTTutorialDirector::QuickCopyDone()
{
    switch (QLesson)
    {
    case 0: // abrió el picker, eligió un AMARILLO y CERRÓ (confirmó): recién ahí cuenta, no al abrir
    {
        const APTSculptPlayerController* P = PC.Get();
        if (!bQSawPicker || !P) return false;
        if (P->IsColorPickerOpen()) return false; // mientras la rueda está abierta todavía no eligió
        const FLinearColor HSV = P->CurrentPaintColor.LinearRGBToHSV(); // R=hue(grados), G=sat, B=val
        return HSV.R >= 38.f && HSV.R <= 75.f && HSV.G > 0.35f && HSV.B > 0.45f;
    }
    case 1: return QuickLegsMinFill() >= 0.4f;         // patas: las 4 (mínimo por cuadrante)
    case 2: return MeasureFill(QGhost) >= 0.45f;       // torso
    case 3: return MeasureFill(QGhost) >= 0.40f;       // cuello
    case 4: return MeasureFill(QGhost) >= 0.40f && (Volume.IsValid() ? Volume->GetEyeCount() - QEyesBase : 0) >= 2;
    }
    return false;
}

void APTTutorialDirector::QuickBeginFree()
{
    QState = EPTQuick::Free; QTime = 0.f;
    ClearGhosts();
    if (QBrush) QBrush->SetVisibility(false);
    QuickLockSculpt(false);
    if (Widget) Widget->SetProgress(-1.f, FText::GetEmpty());
    Say(TEXT("TUT_G_FREE"));
    // Lista opcional de herramientas para experimentar + Enter para la foto.
    SetHintsFor({ TEXT("ModeErase"), TEXT("ModePaint"), TEXT("CycleShape"), TEXT("Undo"), TEXT("Finish") }, false, false);
}

void APTTutorialDirector::QuickTick(float Dt)
{
    if (QState == EPTQuick::Guess) { QuickTickGuess(Dt); return; }

    QTime += Dt;
    APTSculptPlayerController* P = PC.Get();

    if (QState == EPTQuick::Intro)
    {
        if (QTime > ReadTime() + 1.0f) { QState = EPTQuick::Lesson; QuickBeginLesson(0); }
        return;
    }
    if (QState == EPTQuick::Free)
    {
        if (P && P->WasInputKeyJustPressed(EKeys::Enter)) QuickToPhoto(); // Enter → foto
        return;
    }
    if (QState != EPTQuick::Lesson) return; // Idle (cartel) o Photo (flujo del avanzado)

    // Lección del color (sin demo): el jugador elige AMARILLO con el picker (no se fuerza).
    if (QLesson == 0)
    {
        if (P && P->IsColorPickerOpen()) bQSawPicker = true;
        if (!bStepDone)
        {
            if (QuickCopyDone()) { bStepDone = true; DoneTimer = 0.f; PlaySfx(SuccessSound, 0.5f); Say(TEXT("TUT_Q_GOOD")); }
            // Abrió el picker pero no es amarillo: recordárselo (una vez cada tanto).
            else if (bQSawPicker && QTime > 9.f && Widget && !Widget->IsTyping())
            { QTime = 0.f; Say(TEXT("TUT_G_COLOR")); }
        }
        else
        {
            if (Widget && Widget->IsTyping()) DoneTimer = 0.f; else DoneTimer += Dt;
            if (DoneTimer > ReadTime()) QuickBeginLesson(1);
        }
        return;
    }

    if (QPhase == EPTQPhase::Demo) // Sculpi esculpe
    {
        if (bQRecorded) // REPRODUCIR la grabación con arcilla REAL (como un jugador), luego borrarla
        {
            APTSculptVolume* V = Volume.Get();
            while (V && QRepIdx < QRec.Num() && QRec[QRepIdx].T <= QTime)
            {
                const FPTTutRecStamp& S = QRec[QRepIdx];
                if (S.Mode == PTTutRecEyeMode)
                    V->AddEye(QOrigin + S.Pos, S.Size);   // OJO grabado: Sculpi lo coloca (Size = radio)
                else
                    V->ApplyStamp(QOrigin + S.Pos, (EPTStampShape)S.Shape, S.Size, (EPTEditMode)S.Mode, S.Color, S.Rot, S.Scale);
                QuickShowBrushAt(QOrigin + S.Pos, S.Size, S.Color);
                ++QRepIdx;
            }
            QuickDemoVisual(); // Sculpi mirando el trazo + línea punteada
            const float LastT = QRec.Num() ? QRec.Last().T : 0.f;
            if (QRepIdx >= QRec.Num() && QTime > LastT + 1.3f)
            {
                if (V) { V->LoadFieldState(QSnapshot); V->SetEyeCount(QEyesSnapshot); } // saca arcilla Y ojos del demo
                QuickBuildGhostFromRec();             // la guía sale de los sellos grabados (los ojos se ignoran)
                QuickStartCopy();                     // se vuelve transparente + "copialo"
            }
            return;
        }

        // Scripted: revelar el trazo parte por parte + preview.
        const int32 N = Ghosts.IsValidIndex(QGhost) ? Ghosts[QGhost].Meshes.Num() : 0;
        if (QReveal < N)
        {
            QRevealT += Dt;
            if (QRevealT >= 0.30f)
            {
                QRevealT = 0.f;
                if (UPrimitiveComponent* M = Ghosts[QGhost].Meshes[QReveal]) M->SetVisibility(true);
                if (QParts.IsValidIndex(QReveal)) QuickShowBrushAt(QParts[QReveal].Center, QParts[QReveal].Size, FLinearColor(1.f, 0.82f, 0.08f));
                PlaySfx(VoiceSounds.Num() ? VoiceSounds[0] : TSoftObjectPtr<USoundBase>(), 0.25f);
                ++QReveal;
            }
        }
        else if (QTime > N * 0.30f + 1.3f) // terminó de dibujar → "ahora copialo"
        {
            QuickStartCopy();
        }
        QuickDemoVisual();
        return;
    }

    // Fase COPIA
    QuickTickGhostHighlight(); // la esfera-guía bajo la brocha se agranda + se tiñe (ayuda con la profundidad)
    if (!bStepDone)
    {
        const float Fill = MeasureFill(QGhost); // actualiza el verde de la guía a medida que se llena
        // Patas (lección 1): progreso = mínimo entre las 4 (no 3); el resto por relleno total.
        const float Prog = (QLesson == 1) ? (QuickLegsMinFill() / 0.4f) : (Fill / 0.45f);
        if (Widget) Widget->SetProgress(FMath::Clamp(Prog, 0.f, 1.f), FText::GetEmpty());
        if (QuickCopyDone())
        {
            bStepDone = true; DoneTimer = 0.f;
            PlaySfx(SuccessSound, 0.5f);
            Say(TEXT("TUT_Q_GOOD"));
            if (Widget) Widget->SetProgress(1.f, PTText::Get(TEXT("TUT_DONE_MARK")));
        }
    }
    else
    {
        if (Widget && Widget->IsTyping()) DoneTimer = 0.f; else DoneTimer += Dt;
        if (DoneTimer > ReadTime() + 0.6f) QuickBeginLesson(QLesson + 1);
    }
}

void APTTutorialDirector::QuickToPhoto()
{
    QState = EPTQuick::Photo;
    ClearGhosts();
    if (Widget) Widget->SetProgress(-1.f, FText::GetEmpty());
    Say(TEXT("TUT_AP_PHOTO"));
    // Handoff al flujo de FOTO del avanzado (cuenta regresiva + polaroid + guardar). El Tick deja de
    // llamar a QuickTick cuando QState==Photo y corre TickStep (que maneja Photo y End).
    EnterStep(EPTTutStep::Photo);
}

// ── Intro "adiviná la palabra": Sculpi modela un ÁRBOL grabado y vos escribís en el chat ──────────────
void APTTutorialDirector::QuickBeginGuess()
{
    // Cargar el árbol grabado; si no hay, saltar directo a la jirafa.
    QRec.Reset();
    TArray<uint8> Bytes;
    const bool bHaveTree = FFileHelper::LoadFileToArray(Bytes, *QTutRecPath(5)) && Bytes.Num() > 0;
    if (bHaveTree) { FMemoryReader Ar(Bytes); QTutRecSerialize(Ar, QRec); }
    if (!bHaveTree || QRec.Num() == 0)
    {
        QState = EPTQuick::Intro; QTime = 0.f; Say(TEXT("TUT_G_INTRO"));
        return;
    }

    // OJO: NO prendemos bTutorialGuessPhase todavía. Durante la presentación de Sculpi (fase 0) el HUD de
    // juego sigue oculto (solo se ve a Sculpi). Se prende recién cuando empieza a esculpir el árbol (fase 1).
    QState = EPTQuick::Guess; QGPhase = 0;   // 0 = entrada + saludo
    QTime = 0.f; QRepIdx = 0; bQTreeDone = false; bQGuessed = false; DoneTimer = 0.f;
    QRevealCount = 0; QRevealTimer = 0.f; QGReminderT = 0.f;
    QOrigin = CanvasCenter;
    ClearClay();

    const int32 Li = FMath::Clamp(PTText::GetCurrentLanguageIndex(), 0, (int32)UE_ARRAY_COUNT(GTreeWords) - 1);
    QWord = GTreeWords[Li]; // palabra en el idioma del jugador (para medir el largo de la máscara)

    // Sculpi entra DESDE AFUERA (lejos a la derecha): TickSculpi lo trae a cuadro con su interpolación.
    if (APTSculptPlayerController* P = PC.Get())
        if (P->PlayerCameraManager && Sculpi)
        {
            const FRotationMatrix MR(FRotator(0.f, P->PlayerCameraManager->GetCameraRotation().Yaw, 0.f));
            const FVector CamLoc = P->PlayerCameraManager->GetCameraLocation();
            Sculpi->SetActorLocation(CamLoc + MR.GetUnitAxis(EAxis::X) * 520.f
                                     + MR.GetUnitAxis(EAxis::Y) * 2600.f + FVector(0, 0, -95.f));
        }

    // Bloquear el esculpido, pero que Sculpi NO vaya al trazo todavía: primero entra y saluda de frente.
    QuickLockSculpt(true);
    bQSculpiAtWork = false;
    Say(TEXT("TUT_GUESS_GREET")); // "¡Hola! soy Sculpi, esto es Sculpturillo. Te doy los primeros pasos…"
}

// Pone el GameState como una ronda real: fase Drawing + máscara (todos los idiomas) + reloj de 30 s, sin
// escultor (así el jugador local es "adivinador": ve la máscara, anima las letras y puede chatear).
void APTTutorialDirector::QuickSetupGuessState()
{
    APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
    if (!G) return;
    G->bLocalParty = false;         // no es la "TV" de modo local: el jugador adivina normal
    G->CurrentSculptor = nullptr;   // sin escultor → IsLocalPlayerSculptor()=false
    G->CurrentRound = 0; G->TotalRounds = 0;  // sin "Ronda x/y"
    G->bTurnEndedAllGuessed = false;
    G->TurnPhase = EPTTurnPhase::Drawing;     // dibujando → se ven palabra + reloj
    G->TurnEndServerTime = G->GetServerWorldTimeSeconds() + 30.0; // 30 s de reloj
    QuickPushGuessMask(0, /*bAll=*/false);    // todo tapado
    G->OnTurnPhaseChanged.Broadcast();
}

void APTTutorialDirector::QuickPushGuessMask(int32 RevealN, bool bAll)
{
    APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
    if (!G) return;
    const int32 NumLangs = FMath::Max(1, PTText::GetAvailableLanguages().Num());
    G->MaskedWords.SetNum(NumLangs);
    for (int32 L = 0; L < NumLangs; ++L)
    {
        const FString W = (L < (int32)UE_ARRAY_COUNT(GTreeWords)) ? FString(GTreeWords[L]) : FString(GTreeWords[0]);
        const int32 Rev = bAll ? W.Len() : RevealN;
        // Mismo formato que APTSculptGameMode::MaskWord: celdas separadas por espacio, reveladas en mayúscula.
        FString M;
        for (int32 i = 0; i < W.Len(); ++i)
        {
            const TCHAR C = W[i];
            if (FChar::IsWhitespace(C))      M += TEXT("   ");
            else if (i < Rev)              { M.AppendChar(FChar::ToUpper(C)); M += TEXT(" "); }
            else                             M += TEXT("_ ");
        }
        G->MaskedWords[L] = M.TrimStartAndEnd();
    }
    G->RefreshLocalMasked(); // recalcula MaskedWord (idioma local) y refresca el HUD (anima la letra nueva)
}

void APTTutorialDirector::QuickEndGuessState()
{
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>()) GI->bTutorialGuessPhase = false;
    APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
    if (!G) return;
    // Restaurar el estado NORMAL del tutorial (igual que APTSculptGameMode::Tutorial_BeginFreeSculpt):
    // VOS sos el escultor y la fase queda en Drawing, para que CanLocalPlayerSculpt() sea true y puedas
    // esculpir la jirafa. (Antes lo dejaba en WaitingForPlayers y el esculpido quedaba muerto.)
    APTSculptPlayerController* P = PC.Get();
    G->CurrentSculptor = P ? P->GetPlayerState<APTPlayerState>() : nullptr;
    G->TurnPhase = EPTTurnPhase::Drawing;
    G->TurnEndServerTime = 0.0;   // sin reloj (el HUD del tutorial lo oculta igual)
    G->MaskedWords.Reset();
    G->RefreshLocalMasked();
    G->OnTurnPhaseChanged.Broadcast();
}

void APTTutorialDirector::OnTutorialChat(const FString& Msg)
{
    if (QState != EPTQuick::Guess || QGPhase != 1 || bQGuessed) return;
    // Aceptar el guess en CUALQUIER idioma (árbol/tree/árvore/Baum/arbre/albero).
    const FString Gn = QTutNorm(Msg);
    bool bMatch = false;
    for (const TCHAR* W : GTreeWords) if (Gn == QTutNorm(W)) { bMatch = true; break; }
    if (!bMatch) return;

    bQGuessed = true; QGPhase = 2; DoneTimer = 0.f;
    QuickPushGuessMask(0, /*bAll=*/true); // revelar la palabra completa
    PlaySfx(SuccessSound, 0.6f);
    // Popup de juego "¡adivinaste!" (palabra + puntos) + felicitación de Sculpi.
    if (APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr)
        G->OnYouGuessed.Broadcast(QWord, 100);
    Say(TEXT("TUT_GUESS_OK"));
}

void APTTutorialDirector::QuickTickGuess(float Dt)
{
    QTime += Dt;
    APTSculptVolume* V = Volume.Get();
    APTSculptGameState* G = GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;

    // ── Fase 0: entrada + saludo (Sculpi entra de afuera y se presenta, de frente) ──
    if (QGPhase == 0)
    {
        if (QTime > ReadTime() + 1.4f)
        {
            QGPhase = 1; QTime = 0.f; QRepIdx = 0; bQTreeDone = false;
            bQSculpiAtWork = true;       // ahora sí: Sculpi se pone frente al trazo que "esculpe"
            if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>()) GI->bTutorialGuessPhase = true;
            QuickSetupGuessState();      // palabra + reloj + chat con la UI de juego (ahora SÍ se ve)
            Say(TEXT("TUT_GUESS_INTRO")); // "¡mi turno! adiviná qué hago y escribilo en el chat (Enter)"
        }
        return;
    }

    // ── Fases 2/3: resuelto (adivinó o se acabó el tiempo) → leer y pasar a la jirafa ──
    if (QGPhase >= 2)
    {
        DoneTimer += Dt;
        if (DoneTimer > ReadTime() + 1.0f) QuickAfterGuessToGiraffe();
        return;
    }

    // ── Fase 1: adivinando ──
    // Reproducir el árbol (una vez); queda dibujado mientras adivinás.
    if (!bQTreeDone && V)
    {
        while (QRepIdx < QRec.Num() && QRec[QRepIdx].T <= QTime)
        {
            const FPTTutRecStamp& S = QRec[QRepIdx];
            if (S.Mode == PTTutRecEyeMode)
                V->AddEye(QOrigin + S.Pos, S.Size);
            else
                V->ApplyStamp(QOrigin + S.Pos, (EPTStampShape)S.Shape, S.Size, (EPTEditMode)S.Mode, S.Color, S.Rot, S.Scale);
            QuickShowBrushAt(QOrigin + S.Pos, S.Size, S.Color);
            ++QRepIdx;
        }
        QuickDemoVisual();
        if (QRepIdx >= QRec.Num()) { bQTreeDone = true; if (QBrush) QBrush->SetVisibility(false); QuickHideDots(); }
    }

    // ¿Se acabó el tiempo (30 s) sin adivinar? → revelar la palabra y "la palabra era «árbol»…".
    if (G && G->GetTurnSecondsRemaining() <= 0.f)
    {
        QGPhase = 3; DoneTimer = 0.f;
        G->TurnPhase = EPTTurnPhase::TurnEnd;   // fin de turno: la palabra se muestra completa
        QuickPushGuessMask(0, /*bAll=*/true);
        FFormatOrderedArguments A; A.Add(FText::FromString(QWord));
        if (Widget) Widget->Say(PTText::Format(FName(TEXT("TUT_GUESS_FAIL")), A));
        return;
    }

    // Recordatorio periódico: escribí tu respuesta en el chat (Enter).
    QGReminderT += Dt;
    if (QGReminderT >= 9.f) { QGReminderT = 0.f; Say(TEXT("TUT_GUESS_REMIND")); }

    // Revelar letras como PISTA (izq→der): la 1ª a los ~8 s, después cada ~10 s (deja la última sin revelar).
    QRevealTimer += Dt;
    const float Due = (QRevealCount == 0) ? 8.f : 10.f;
    if (QRevealCount < QWord.Len() - 1 && QRevealTimer >= Due)
    {
        QRevealTimer = 0.f; QGReminderT = 0.f;
        ++QRevealCount;
        QuickPushGuessMask(QRevealCount, /*bAll=*/false);
        Say(TEXT("TUT_GUESS_PISTA"));
    }
}

void APTTutorialDirector::QuickAfterGuessToGiraffe()
{
    QuickEndGuessState();         // restaurar el HUD de juego (saca palabra/reloj/chat)
    ClearClay();                  // borrar el árbol antes de la jirafa
    if (QBrush) QBrush->SetVisibility(false);
    QuickHideDots();
    QuickLockSculpt(false);
    QState = EPTQuick::Intro;
    QTime = 0.f;
    Say(TEXT("TUT_G_INTRO"));      // "ahora tu turno: vamos a modelar una jirafa…"
}

void APTTutorialDirector::QuickShowChoice()
{
    QState = EPTQuick::Idle;
    Say(TEXT("TUT_Q_END"));
    if (Widget) { Widget->SetProgress(-1.f, FText::GetEmpty()); Widget->SetHints(TArray<FPTTutHint>()); }

    APTSculptPlayerController* P = PC.Get();
    if (!P) { QuickMarkDoneAndExit(/*bAdvanced=*/false); return; }

    ChoiceWidget = CreateWidget<UPTTutorialChoiceWidget>(P, UPTTutorialChoiceWidget::StaticClass());
    if (!ChoiceWidget) { QuickMarkDoneAndExit(false); return; }
    ChoiceWidget->Setup(PTText::Get(TEXT("TUT_Q_ADV_Q")), PTText::Get(TEXT("TUT_Q_ADV_YES")), PTText::Get(TEXT("TUT_Q_ADV_NO")));
    ChoiceWidget->OnYes.AddUObject(this, &APTTutorialDirector::OnAdvancedYes);
    ChoiceWidget->OnNo.AddUObject(this, &APTTutorialDirector::OnAdvancedNo);
    ChoiceWidget->AddToViewport(40);

    P->bShowMouseCursor = true;
    FInputModeUIOnly Mode;
    Mode.SetWidgetToFocus(ChoiceWidget->TakeWidget());
    Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    P->SetInputMode(Mode);
    if (UPTGameInstance* GI = GetGameInstance<UPTGameInstance>()) GI->bTutorialWantsCursor = true;
}

void APTTutorialDirector::OnAdvancedYes() { QuickMarkDoneAndExit(/*bAdvanced=*/true); }
void APTTutorialDirector::OnAdvancedNo()  { QuickMarkDoneAndExit(/*bAdvanced=*/false); }

void APTTutorialDirector::QuickMarkDoneAndExit(bool bAdvanced)
{
    if (bExiting) return;
    bExiting = true;
    if (UPTGameUserSettings* S = UPTGameUserSettings::Get()) S->SetQuickTutorialDone(true);
    if (ChoiceWidget) { ChoiceWidget->RemoveFromParent(); ChoiceWidget = nullptr; }

    UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    if (GI) { GI->bTutorialWantsCursor = false; GI->bTutorialMode = false; GI->bQuickTutorial = false; }

    if (bAdvanced && GI) { GI->EnterTutorial(/*bQuick=*/false); return; } // abre el avanzado (re-travel)
    if (GI) GI->ExitTutorial(); // No → al menú
}
