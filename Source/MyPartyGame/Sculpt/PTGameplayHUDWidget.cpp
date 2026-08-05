#include "PTGameplayHUDWidget.h"
#include "PTSculptPlayerController.h"
#include "PTScoreRowWidget.h"
#include "../Lobby/PTPlayerState.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/ScrollBox.h"
#include "Components/PanelWidget.h"
#include "Components/Image.h"
#include "../PTInputBindings.h"
#include "../PTTextTable.h"
#include "../UI/PTControlRowWidget.h"
#include "../UI/PTToolSlotWidget.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "../PTNetStats.h"
#include "TimerManager.h"

bool UPTGameplayHUDWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (BtnWord0) BtnWord0->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnWord0);
    if (BtnWord1) BtnWord1->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnWord1);
    if (BtnWord2) BtnWord2->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnWord2);
    if (ChatInput) ChatInput->OnTextCommitted.AddDynamic(this, &UPTGameplayHUDWidget::OnChatCommitted);
    if (BtnPlayAgain)   BtnPlayAgain->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnPlayAgain);
    if (BtnReturnLobby) BtnReturnLobby->OnClicked.AddDynamic(this, &UPTGameplayHUDWidget::OnBtnReturnLobby);

    // Chat: envolver texto largo (que el mensaje baje de línea en vez de cortarse al salir del margen).
    if (TxtChat) TxtChat->SetAutoWrapText(true);

    // Lista de controles (pantalla de ayuda opcional) + barra de herramientas: se arman una vez.
    RebuildControls();
    BuildToolbar();

    return true;
}

void UPTGameplayHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    const APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(GetOwningPlayer());
    if (!PC) return;

    // Círculo de "borrar todo": se llena mientras mantenés la tecla, con el contador 3→1 adentro.
    // Va por frame (no por el RefreshTick de 10Hz) para que el llenado se vea fluido.
    if (ClearSlot)
    {
        const float P = PC->GetClearHoldProgress();
        ClearSlot->SetProgress(P, P > 0.f
            ? FText::AsNumber(FMath::CeilToInt(PC->GetClearHoldRemaining()))
            : FText::GetEmpty());
    }

    // Ícono de "prohibido construir": apuntando fuera de la zona de modelado (cualquier tool).
    if (OutOfBoundsIcon)
    {
        const APTSculptGameState* G = GetGS();
        const bool bSculpting = G && G->TurnPhase == EPTTurnPhase::Drawing && G->IsLocalPlayerSculptor();
        OutOfBoundsIcon->SetVisibility(bSculpting && PC->IsStampOutsideCanvas()
            ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void UPTGameplayHUDWidget::BuildToolbar()
{
    if (!ToolSlotClass) return;

    auto MakeSlot = [this](UPanelWidget* Box, UTexture2D* Icon, const FKey& Key, const FText& Label)
        -> UPTToolSlotWidget*
    {
        if (!Box) return nullptr;
        UPTToolSlotWidget* S = CreateWidget<UPTToolSlotWidget>(GetOwningPlayer(), ToolSlotClass);
        if (!S) return nullptr;
        S->SetSlot(Icon, Key.GetDisplayName(), Label);
        Box->AddChild(S);
        return S;
    };

    // Tools: la tecla sale de PTInput (misma tabla que bindea el controller) → si se rebindea,
    // el cuadrito muestra la tecla nueva sin tocar nada acá.
    if (ToolsBox)
    {
        ToolsBox->ClearChildren();
        ToolSlots.Reset();
        ToolSlots.Add(MakeSlot(ToolsBox, IconAdd,   PTInput::GetKey(TEXT("ModeAdd")),   PTText::Get(TEXT("TOOL_ADD"))));
        ToolSlots.Add(MakeSlot(ToolsBox, IconErase, PTInput::GetKey(TEXT("ModeErase")), PTText::Get(TEXT("TOOL_ERASE"))));
        ToolSlots.Add(MakeSlot(ToolsBox, IconPaint, PTInput::GetKey(TEXT("ModePaint")), PTText::Get(TEXT("TOOL_PAINT"))));
        ToolSlots.Add(MakeSlot(ToolsBox, IconEyes,  PTInput::GetKey(TEXT("ModeEyes")),  PTText::Get(TEXT("TOOL_EYES"))));
    }

    // Formas: todas muestran la tecla de ciclar (TAB), y se resalta la equipada.
    if (ShapesBox)
    {
        ShapesBox->ClearChildren();
        ShapeSlots.Reset();
        const FKey TabKey = PTInput::GetKey(TEXT("CycleShape"));
        ShapeSlots.Add(MakeSlot(ShapesBox, IconSphere,   TabKey, PTText::Get(TEXT("SHAPE_SPHERE"))));
        ShapeSlots.Add(MakeSlot(ShapesBox, IconCube,     TabKey, PTText::Get(TEXT("SHAPE_CUBE"))));
        ShapeSlots.Add(MakeSlot(ShapesBox, IconCylinder, TabKey, PTText::Get(TEXT("SHAPE_CYLINDER"))));
        ShapeSlots.Add(MakeSlot(ShapesBox, IconCone,     TabKey, PTText::Get(TEXT("SHAPE_CONE"))));
    }

    // Borrar todo (BACKSPACE mantenido): cuadrito fijo con círculo de progreso + contador.
    if (ClearBox)
    {
        ClearBox->ClearChildren();
        ClearSlot = MakeSlot(ClearBox, IconClearAll, PTInput::GetKey(TEXT("ClearAll")),
                             PTText::Get(TEXT("TOOL_CLEAR_ALL")));
        if (ClearSlot) ClearSlot->SetProgress(0.f, FText::GetEmpty()); // arranca sin círculo
    }
}

void UPTGameplayHUDWidget::RefreshToolbar()
{
    APTSculptPlayerController* PC = Cast<APTSculptPlayerController>(GetOwningPlayer());
    APTSculptGameState* G = GetGS();
    if (!PC) return;

    // La barra es del escultor: solo se ve cuando te toca dibujar.
    const bool bSculpting = G && G->TurnPhase == EPTTurnPhase::Drawing && G->IsLocalPlayerSculptor();
    const ESlateVisibility Vis = bSculpting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed;
    if (ToolsBox)  ToolsBox->SetVisibility(Vis);
    if (HintsBox)  HintsBox->SetVisibility(Vis);
    if (ClearBox)  ClearBox->SetVisibility(Vis);

    if (!bSculpting)
    {
        if (ShapesBox) ShapesBox->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }


    // ── Tool equipada ──
    const bool bEyes = PC->IsEyesToolActive();
    const int32 ToolIdx = bEyes ? 3
        : (PC->EditMode == EPTEditMode::Add   ? 0
        :  PC->EditMode == EPTEditMode::Erase ? 1
        :  PC->EditMode == EPTEditMode::Paint ? 2 : -1);
    for (int32 i = 0; i < ToolSlots.Num(); ++i)
        if (ToolSlots[i]) ToolSlots[i]->SetSelected(i == ToolIdx);

    // ── Formas: Agregar, Borrar y Pintar usan la forma elegida (Ojos siempre es esfera) ──
    const bool bShowShapes = PC->ToolUsesShapes();
    if (ShapesBox) ShapesBox->SetVisibility(bShowShapes ? ESlateVisibility::HitTestInvisible
                                                        : ESlateVisibility::Collapsed);
    if (bShowShapes)
    {
        const int32 ShapeIdx = (int32)PC->StampShape; // Sphere, Cube, Cylinder, TriPrism
        for (int32 i = 0; i < ShapeSlots.Num(); ++i)
            if (ShapeSlots[i]) ShapeSlots[i]->SetSelected(i == ShapeIdx);
    }

    // ── Atajos contextuales: cambian según lo que tengas equipado ──
    if (!HintsBox || !ToolSlotClass) return;

    const bool bPicker = PC->IsColorPickerOpen();
    // Firma del contexto: solo rearmamos los cuadritos si cambió (no cada tick).
    const FString Sig = FString::Printf(TEXT("%d|%d|%d|%d"), (int32)PC->EditMode, bEyes ? 1 : 0,
                                        bPicker ? 1 : 0, PC->IsAxisLockActive() ? 1 : 0);
    if (Sig != CachedHintSig)
    {
        CachedHintSig = Sig;
        HintsBox->ClearChildren();
        HintSlots.Reset();

        auto AddHint = [this](UTexture2D* Icon, const FKey& Key, const FText& Label) -> UPTToolSlotWidget*
        {
            UPTToolSlotWidget* S = CreateWidget<UPTToolSlotWidget>(GetOwningPlayer(), ToolSlotClass);
            if (!S) return nullptr;
            S->SetSlot(Icon, Key.GetDisplayName(), Label);
            HintsBox->AddChild(S);
            HintSlots.Add(S);
            return S;
        };

        if (bPicker)
        {
            // Con la rueda de color abierta: E guarda el color donde tenés el puntero.
            AddHint(IconSaveColor, PTInput::GetKey(TEXT("SaveColor")), PTText::Get(TEXT("HINT_SAVE_COLOR")));
        }
        else if (!bEyes && PC->EditMode == EPTEditMode::Add)
        {
            // Con Agregar: los dos planos de trazo recto.
            AddHint(IconAxisVert,  PTInput::GetKey(TEXT("AxisVertical")),   PTText::Get(TEXT("HINT_PLANE_VERTICAL")));
            AddHint(IconAxisHoriz, PTInput::GetKey(TEXT("AxisHorizontal")), PTText::Get(TEXT("HINT_PLANE_HORIZONTAL")));
        }
    }

    // Resaltar el plano activo (si el modo eje está encendido).
    if (!bPicker && !bEyes && PC->EditMode == EPTEditMode::Add && HintSlots.Num() >= 2)
    {
        const bool bAxis = PC->IsAxisLockActive();
        if (HintSlots[0]) HintSlots[0]->SetSelected(bAxis && !PC->IsAxisHorizontal());
        if (HintSlots[1]) HintSlots[1]->SetSelected(bAxis &&  PC->IsAxisHorizontal());
    }
}

void UPTGameplayHUDWidget::RebuildControls()
{
    if (!ControlsBox || !ControlRowClass) return;

    ControlsBox->ClearChildren();
    // Una fila por acción, leyendo la MISMA tabla que usa el PlayerController para bindear.
    for (const FPTKeyBinding& B : PTInput::GetBindings())
    {
        UPTControlRowWidget* Row = CreateWidget<UPTControlRowWidget>(GetOwningPlayer(), ControlRowClass);
        if (!Row) continue;
        Row->SetRow(B.Label, B.Key.GetDisplayName());
        ControlsBox->AddChild(Row);
    }
}

void UPTGameplayHUDWidget::ShowHUD()
{
    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);
    RefreshTick();
    if (UWorld* World = GetWorld())
        World->GetTimerManager().SetTimer(RefreshTimer, this, &UPTGameplayHUDWidget::RefreshTick, 0.1f, true);

    // La hotbar y las filas de controles se arman una sola vez: si el jugador cambia el idioma
    // desde el menú de pausa hay que rehacerlas, si no quedan en el idioma anterior.
    if (!LanguageHandle.IsValid())
        LanguageHandle = PTText::OnLanguageChanged().AddWeakLambda(this, [this]()
        {
            BuildToolbar();
            RebuildControls();
        });
}

void UPTGameplayHUDWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld())
        World->GetTimerManager().ClearTimer(RefreshTimer);
    if (LanguageHandle.IsValid())
    {
        PTText::OnLanguageChanged().Remove(LanguageHandle);
        LanguageHandle.Reset();
    }
    Super::NativeDestruct();
}

APTSculptGameState* UPTGameplayHUDWidget::GetGS() const
{
    return GetWorld() ? GetWorld()->GetGameState<APTSculptGameState>() : nullptr;
}

APTSculptPlayerController* UPTGameplayHUDWidget::GetSculptPC() const
{
    return Cast<APTSculptPlayerController>(GetOwningPlayer());
}

void UPTGameplayHUDWidget::RefreshTick()
{
    APTSculptGameState* G = GetGS();

    // Engancharse al chat una sola vez, cuando el GameState ya esté replicado.
    if (G && !bChatBound)
    {
        G->OnChatLine.AddDynamic(this, &UPTGameplayHUDWidget::OnChatLine);
        bChatBound = true;
    }
    if (!G) return;

    // Barra de herramientas: qué está equipado / qué barras se ven.
    RefreshToolbar();

    APTSculptPlayerController* PC = GetSculptPC();
    const bool bSculptor = G->IsLocalPlayerSculptor();

    // El "quién esculpe" ya NO va arriba: se muestra en el marcador con el emoji 🖌️.
    // Arriba queda solo el timer + la palabra. Limpiamos TxtSculptor si sigue en el WBP.
    if (TxtSculptor)
        TxtSculptor->SetText(FText::GetEmpty());

    // ── Panel de palabras + texto de la palabra, según la fase ──
    FString WordText;
    bool bShowPanel = false;
    switch (G->TurnPhase)
    {
    case EPTTurnPhase::ChoosingWord:
        if (bSculptor)
        {
            bShowPanel = true;
            const TArray<FString>& Ch = PC ? PC->CurrentWordChoices : TArray<FString>();
            if (TxtWord0) TxtWord0->SetText(FText::FromString(Ch.IsValidIndex(0) ? Ch[0] : FString()));
            if (TxtWord1) TxtWord1->SetText(FText::FromString(Ch.IsValidIndex(1) ? Ch[1] : FString()));
            if (TxtWord2) TxtWord2->SetText(FText::FromString(Ch.IsValidIndex(2) ? Ch[2] : FString()));
        }
        break;
    // (el contador de elección lo maneja el bloque del reloj de abajo)
    case EPTTurnPhase::Drawing:
        WordText = bSculptor && PC ? PC->CurrentSecretWord : G->MaskedWord;
        break;
    case EPTTurnPhase::TurnEnd:
        WordText = G->MaskedWord; // ya revelada
        break;
    case EPTTurnPhase::WaitingForPlayers:
        WordText = PTText::GetStr(TEXT("HUD_WAITING_PLAYERS"));
        break;
    default: break; // GameOver (el panel de resultados tapa esto)
    }
    if (WordPickPanel)
        WordPickPanel->SetVisibility(bShowPanel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (TxtWord)
        TxtWord->SetText(FText::FromString(WordText));

    // La caja de texto SÓLO se muestra mientras el chat está abierto (Enter). Colapsada,
    // Tab no puede darle foco ni congelar el input. Vale para todos, incluido el escultor
    // (que ahora también puede chatear con Enter; el anti-spoiler descarta la palabra).
    if (ChatInput)
        ChatInput->SetVisibility(bChatOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    const int32 SecsLeft = FMath::Max(0, FMath::CeilToInt(G->GetTurnSecondsRemaining()));
    const FString TimerStr = FString::Printf(TEXT("%d %s"), SecsLeft, *PTText::GetStr(TEXT("HUD_SECONDS_SHORT")));

    // ── Reloj de arriba: SOLO en Drawing (tiempo del turno). ──
    if (TxtTimer)
        TxtTimer->SetText(G->TurnPhase == EPTTurnPhase::Drawing ? FText::FromString(TimerStr) : FText::GetEmpty());

    // ── Contador de ELEGIR PALABRA: su propio texto DENTRO del WordPickPanel. Solo al escultor mientras elige. ──
    // Usa GetPhaseSecondsRemaining (no GetTurnSecondsRemaining, que da 0 fuera de Drawing).
    if (TxtChooseTimer)
    {
        const bool bChoosing = (G->TurnPhase == EPTTurnPhase::ChoosingWord) && bSculptor;
        TxtChooseTimer->SetVisibility(bChoosing ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
        if (bChoosing)
        {
            const int32 ChooseLeft = FMath::Max(0, FMath::CeilToInt(G->GetPhaseSecondsRemaining()));
            TxtChooseTimer->SetText(FText::FromString(FString::Printf(TEXT("%d %s"),
                ChooseLeft, *PTText::GetStr(TEXT("HUD_SECONDS_SHORT")))));
        }
    }

    // ── Ronda + marcador en vivo ──
    if (TxtRound)
    {
        if (G->TotalRounds > 0 && G->TurnPhase != EPTTurnPhase::WaitingForPlayers &&
            G->TurnPhase != EPTTurnPhase::GameOver)
            TxtRound->SetText(FText::FromString(FString::Printf(TEXT("%s %d/%d"),
                                                               *PTText::GetStr(TEXT("HUD_ROUND")),
                                                               G->CurrentRound, G->TotalRounds)));
        else
            TxtRound->SetText(FText::GetEmpty());
    }
    RebuildScoreboard();

    // ── Pantalla de fin de partida ──
    const bool bGameOver = (G->TurnPhase == EPTTurnPhase::GameOver);
    if (ResultsPanel)
        ResultsPanel->SetVisibility(bGameOver ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (bGameOver)
    {
        if (TxtResults)
            TxtResults->SetText(FText::FromString(PTText::GetStr(TEXT("HUD_RESULTS")) + TEXT("\n") + BuildScoreboard()));
        // Los botones de decisión son solo para el anfitrión.
        const bool bHost = IsLocalPlayerHost();
        const ESlateVisibility BtnVis = bHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed;
        if (BtnPlayAgain)   BtnPlayAgain->SetVisibility(BtnVis);
        if (BtnReturnLobby) BtnReturnLobby->SetVisibility(BtnVis);
    }

    // ── Modo de input ──
    // Base = Game Only (moverse/esculpir siempre funcionan). Solo se muestra el cursor
    // cuando de verdad hace falta la UI: el escultor eligiendo palabra, con el chat
    // abierto (Enter), la pantalla de fin (para clickear los botones), el menú de pausa
    // ESC abierto, o la rueda de color (mantener RMB) — si no, el refresh del HUD pisaría el
    // cursor que esos abren directo y la flecha desaparecía (no se podía elegir color al esculpir).
    const bool bMenuOpen    = PC && PC->IsEscapeMenuOpen();
    const bool bColorPicker = PC && PC->IsColorPickerOpen();
    const bool bWantUI = bChatOpen || bGameOver || bMenuOpen || bColorPicker ||
                         (bSculptor && G->TurnPhase == EPTTurnPhase::ChoosingWord);
    ApplyInputMode(!bWantUI);

    // ── Diagnóstico de red (ping + packet loss) ──
    // On-screen (visible en build Development). Sirve para ver el lag en el test con un amigo.
    if (GEngine)
    {
        const PTNetStats::FLine NS = PTNetStats::Build(GetOwningPlayer());
        if (!NS.Text.IsEmpty())
            GEngine->AddOnScreenDebugMessage(987711, 1.5f, NS.Color, NS.Text);
    }
}

void UPTGameplayHUDWidget::FocusChat()
{
    if (bChatOpen) return;
    bChatOpen = true;
    if (ChatInput) ChatInput->SetVisibility(ESlateVisibility::Visible);
    ApplyInputMode(false); // GameAndUI + cursor
    // Recién visible: enfocar en el próximo tick (Slate necesita un frame para darle foco).
    if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimerForNextTick([this]() { if (ChatInput) ChatInput->SetKeyboardFocus(); });
}

void UPTGameplayHUDWidget::ApplyInputMode(bool bGameOnly)
{
    APlayerController* PC = GetOwningPlayer();
    if (!PC) return;

    // Re-aplicar si cambió lo que queremos, O si el cursor real quedó desincronizado de lo que
    // debería estar. Esto último pasa porque el PlayerController setea FInputModeGameOnly directo
    // en otros lados (color picker, etc.) sin avisarle al cache de acá: sin este chequeo, cuando
    // después queremos el cursor (mostrar el scoreboard de fin, o elegir palabra en la ronda
    // nueva), el early-return por cache creía que "ya estaba" y la flecha quedaba escondida.
    const bool bWantCursor = !bGameOnly;
    if (bInputModeInit && bGameOnly == bWantsGameOnly && PC->bShowMouseCursor == bWantCursor)
        return;
    bInputModeInit = true;
    bWantsGameOnly = bGameOnly;

    if (bGameOnly)
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->SetShowMouseCursor(false);
    }
    else
    {
        FInputModeGameAndUI Mode;
        Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Mode);
        PC->SetShowMouseCursor(true);
    }
}

void UPTGameplayHUDWidget::OnBtnWord0() { ChooseWord(0); }
void UPTGameplayHUDWidget::OnBtnWord1() { ChooseWord(1); }
void UPTGameplayHUDWidget::OnBtnWord2() { ChooseWord(2); }

void UPTGameplayHUDWidget::ChooseWord(int32 Index)
{
    if (APTSculptPlayerController* PC = GetSculptPC())
        PC->Server_ChooseWord(Index);
    if (WordPickPanel)
        WordPickPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTGameplayHUDWidget::OnChatCommitted(const FText& Text, ETextCommit::Type CommitMethod)
{
    if (CommitMethod == ETextCommit::OnEnter)
    {
        const FString Msg = Text.ToString();
        if (!Msg.IsEmpty())
            if (APTSculptPlayerController* PC = GetSculptPC())
                PC->Server_SendChat(Msg);
    }
    // Enviar (Enter) o perder el foco: limpiar, COLAPSAR (para que Tab no la reenfoque)
    // y devolver el control al juego.
    if (ChatInput)
    {
        ChatInput->SetText(FText::GetEmpty());
        ChatInput->SetVisibility(ESlateVisibility::Collapsed);
    }
    bChatOpen = false;
    ApplyInputMode(true); // Game Only
}

void UPTGameplayHUDWidget::OnChatLine(const FString& Name, const FString& Message, EPTChatType Type)
{
    const FString ShortName = Name.Left(10); // nombre máximo 10 caracteres
    // El nombre va en color (estilo "name" del RichTextBlock); el mensaje queda blanco.
    FString Line;
    switch (Type)
    {
    case EPTChatType::Correct: Line = FString::Printf(TEXT("<name>%s</> <correct>%s</>"), *ShortName,
                                                      *PTText::GetStr(TEXT("CHAT_GUESSED_IT"))); break;
    case EPTChatType::System:  Line = Message; break;
    default:                   Line = FString::Printf(TEXT("<name>%s</>: %s"), *ShortName, *Message); break; // Normal
    }
    ChatLog += Line + TEXT("\n");
    if (TxtChat)    TxtChat->SetText(FText::FromString(ChatLog));
    if (ChatScroll) ChatScroll->ScrollToEnd();
}

void UPTGameplayHUDWidget::OnBtnPlayAgain()
{
    if (APTSculptPlayerController* PC = GetSculptPC())
        PC->Server_RequestPlayAgain();
}

void UPTGameplayHUDWidget::OnBtnReturnLobby()
{
    if (APTSculptPlayerController* PC = GetSculptPC())
        PC->Server_RequestReturnToLobby();
}

FString UPTGameplayHUDWidget::BuildScoreboard() const
{
    APTSculptGameState* G = GetGS();
    if (!G) return FString();

    TArray<APTPlayerState*> Players;
    for (APlayerState* PS : G->PlayerArray)
        if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
            Players.Add(PT);

    Players.Sort([](const APTPlayerState& A, const APTPlayerState& B){ return A.GameScore > B.GameScore; });

    FString Out;
    for (const APTPlayerState* PT : Players)
        Out += FString::Printf(TEXT("%s: %d\n"), *PT->GetDisplayNameSafe().Left(10), PT->GameScore);
    return Out;
}

void UPTGameplayHUDWidget::RebuildScoreboard()
{
    if (!ScoreboardBox || !ScoreRowClass) return;
    APTSculptGameState* G = GetGS();
    if (!G) return;

    TArray<APTPlayerState*> Players;
    for (APlayerState* PS : G->PlayerArray)
        if (APTPlayerState* PT = Cast<APTPlayerState>(PS))
            Players.Add(PT);
    Players.Sort([](const APTPlayerState& A, const APTPlayerState& B){ return A.GameScore > B.GameScore; });

    // Firma del estado visible: solo reconstruimos las filas si algo cambió (no cada tick).
    FString Sig;
    for (const APTPlayerState* PT : Players)
        Sig += FString::Printf(TEXT("%s:%d:%d|"), *PT->GetDisplayNameSafe(),
                               PT->GameScore, PT == G->CurrentSculptor ? 1 : 0);
    if (Sig == CachedScoreSig) return;
    CachedScoreSig = Sig;

    ScoreboardBox->ClearChildren();
    for (APTPlayerState* PT : Players)
    {
        UPTScoreRowWidget* Row = CreateWidget<UPTScoreRowWidget>(GetOwningPlayer(), ScoreRowClass);
        if (!Row) continue;
        Row->SetRow(PT->GetDisplayNameSafe(), PT->GameScore, PT == G->CurrentSculptor);
        ScoreboardBox->AddChild(Row);
    }
}

bool UPTGameplayHUDWidget::IsLocalPlayerHost() const
{
    if (const APlayerController* PC = GetOwningPlayer())
        if (const APTPlayerState* PT = PC->GetPlayerState<APTPlayerState>())
            return PT->bIsHost;
    return false;
}
