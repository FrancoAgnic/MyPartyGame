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
#include "../PTGameUserSettings.h"
#include "../UI/PTControlRowWidget.h"
#include "../UI/PTToolSlotWidget.h"
#include "Components/PanelWidget.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "../PTNetStats.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Animation/WidgetAnimation.h"

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

    // Ocultar la letra revelada al terminar su animación (así no queda fija en pantalla).
    if (RevealLetterAnim)
    {
        FWidgetAnimationDynamicEvent D;
        D.BindDynamic(this, &UPTGameplayHUDWidget::OnRevealAnimFinished);
        BindToAnimationFinished(RevealLetterAnim, D);
    }
    if (RevealLetterText) RevealLetterText->SetVisibility(ESlateVisibility::Collapsed);

    // Al terminar la animación de "acertaste", revelar la palabra completa (sincronizado).
    if (YouGuessedAnim)
    {
        FWidgetAnimationDynamicEvent DG;
        DG.BindDynamic(this, &UPTGameplayHUDWidget::OnYouGuessedAnimFinished);
        BindToAnimationFinished(YouGuessedAnim, DG);
    }

    // Sonido de tecla (máquina de escribir) por cada cambio de texto del chat.
    if (ChatInput) ChatInput->OnTextChanged.AddDynamic(this, &UPTGameplayHUDWidget::OnChatTextChanged);

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
            // Con Agregar: los dos planos de trazo recto + ALT (detalle en capa aparte).
            AddHint(IconAxisVert,  PTInput::GetKey(TEXT("AxisVertical")),   PTText::Get(TEXT("HINT_PLANE_VERTICAL")));
            AddHint(IconAxisHoriz, PTInput::GetKey(TEXT("AxisHorizontal")), PTText::Get(TEXT("HINT_PLANE_HORIZONTAL")));
            AddHint(IconDetail,    FKey(EKeys::LeftAlt),                    PTText::Get(TEXT("TOOL_DETAIL")));
        }
    }

    // Resaltar el plano activo (eje) y el detalle (Alt) si están encendidos.
    if (!bPicker && !bEyes && PC->EditMode == EPTEditMode::Add && HintSlots.Num() >= 3)
    {
        const bool bAxis = PC->IsAxisLockActive();
        if (HintSlots[0]) HintSlots[0]->SetSelected(bAxis && !PC->IsAxisHorizontal());
        if (HintSlots[1]) HintSlots[1]->SetSelected(bAxis &&  PC->IsAxisHorizontal());
        if (HintSlots[2]) HintSlots[2]->SetSelected(PC->IsSurfaceSnapActive());
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
    StopCountdownSound(); // que no quede el countdown sonando si se cierra el HUD
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
        G->OnYouGuessed.AddDynamic(this, &UPTGameplayHUDWidget::OnYouGuessed);
        G->OnSomeoneGuessed.AddDynamic(this, &UPTGameplayHUDWidget::OnSomeoneGuessed);
        G->OnAllGuessed.AddDynamic(this, &UPTGameplayHUDWidget::OnAllGuessed);
        bChatBound = true;
    }
    if (!G) return;

    // Sonidos (tick por segundo, countdown, pista, fin de turno). Va ANTES de resetear bLocalGuessed,
    // porque el sonido de "se acabó el tiempo" depende de si adivinaste en este turno.
    UpdateGameplaySounds(G);

    // Fuera del turno de dibujo, olvidar que adivinaste (el próximo turno arranca con guiones de nuevo).
    if (G->TurnPhase != EPTTurnPhase::Drawing) bLocalGuessed = false;

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
        if (bSculptor && PC)      WordText = PC->CurrentSecretWord;
        else if (bLocalGuessed)   WordText = LocalGuessedWord; // ya adivinaste → palabra completa (verde)
        else if (UseDelayedReveal() && DisplayedMask.Len() == G->MaskedWord.Len())
            WordText = DisplayedMask; // que adivina: letra aparece recién cuando su animación aterriza
        else                      WordText = G->MaskedWord;
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

    // La palabra es un RichTextBlock: el coloreo se hace por TAGS (necesita un estilo "green" en su
    // Text Style Set). El texto plano usa el estilo "Default".
    if (TxtWord)
    {
        FString Rich;
        if (bSculptor && PC && G->TurnPhase == EPTTurnPhase::Drawing)
        {
            // ESCULTOR: palabra completa; las letras ya reveladas a los que adivinan van en VERDE.
            // La máscara viene con un espacio entre cada letra ("_ N _ E ..."), así que la recorremos por
            // CELDAS (salteando los espacios) y mapeamos cada celda a su letra del secreto → alineado.
            const FString& Full = PC->CurrentSecretWord;
            const FString& Mask = G->MaskedWord;
            int32 mi = 0;
            for (int32 i = 0; i < Full.Len(); ++i)
            {
                const TCHAR Ch = Full[i];
                if (FChar::IsWhitespace(Ch))
                {
                    Rich += TEXT(" ");
                    while (mi < Mask.Len() && Mask[mi] == TEXT(' ')) ++mi; // saltar el hueco entre palabras
                    continue;
                }
                while (mi < Mask.Len() && Mask[mi] == TEXT(' ')) ++mi;     // avanzar a la próxima celda
                const bool bRevealed = Mask.IsValidIndex(mi) && Mask[mi] != TEXT('_');
                Rich += bRevealed ? FString::Printf(TEXT("<green>%c</>"), Ch) : FString::Chr(Ch);
                ++mi; // consumir la celda
            }
        }
        else if (bLocalGuessed && G->TurnPhase == EPTTurnPhase::Drawing)
        {
            Rich = FString::Printf(TEXT("<green>%s</>"), *WordText); // vos adivinaste → toda verde
        }
        else
        {
            Rich = WordText; // guiones / máscara / "esperando" → texto plano
        }
        TxtWord->SetText(FText::FromString(Rich));
    }

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

    // Mientras la RUEDA DE COLOR está abierta (mantener RMB), el scroll del chat NO debe robar el
    // mouse (bug: se enfocaba/scrolleaba el chat y no se podía elegir color). Lo hacemos click-through
    // solo en ese caso; el resto del tiempo queda interactivo para leer/scrollear el historial.
    if (ChatScroll)
        ChatScroll->SetVisibility(bColorPicker ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Visible);

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

void UPTGameplayHUDWidget::OnChatTextChanged(const FText& Text)
{
    if (!SndTyping) return;
    // Se puede desactivar en Configuración (a algunos les molesta).
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    if (S && !S->IsTypingSoundEnabled()) return;
    UGameplayStatics::PlaySound2D(this, SndTyping); // el Sound Cue elige uno de sus 3 sonidos al azar
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

void UPTGameplayHUDWidget::OnYouGuessed(const FString& Word, int32 Points)
{
    if (SndYouGuessed) UGameplayStatics::PlaySound2D(this, SndYouGuessed); // sonido: VOS adivinaste

    // Popup: "La palabra era «X»" + "+N". Textos localizados.
    if (TxtGuessWord)
    {
        FFormatOrderedArguments A; A.Add(FText::FromString(Word));
        TxtGuessWord->SetText(PTText::Format(FName(TEXT("GUESS_YOU_WORD")), A));
    }
    if (TxtGuessPoints)
    {
        FFormatOrderedArguments A; A.Add(FText::AsNumber(Points));
        TxtGuessPoints->SetText(PTText::Format(FName(TEXT("GUESS_POINTS")), A));
    }
    if (GuessPopup) GuessPopup->SetVisibility(ESlateVisibility::HitTestInvisible);

    // Revelar la palabra COMPLETA (verde) apenas adivinás.
    bLocalGuessed    = true;
    LocalGuessedWord = Word;

    if (YouGuessedAnim)
    {
        PlayAnimation(YouGuessedAnim); // el popup se oculta cuando termina la animación (OnYouGuessedAnimFinished)
    }
    else if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().SetTimer(GuessPopupTimer, this, &UPTGameplayHUDWidget::HideGuessPopup,
                                      GuessFeedbackSeconds, false);
    }
}

void UPTGameplayHUDWidget::OnYouGuessedAnimFinished()
{
    if (GuessPopup) GuessPopup->SetVisibility(ESlateVisibility::Collapsed); // ocultar el popup al terminar
}

void UPTGameplayHUDWidget::HideGuessPopup()
{
    if (GuessPopup) GuessPopup->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTGameplayHUDWidget::OnSomeoneGuessed(APTPlayerState* Guesser, int32 Points)
{
    if (!Guesser) return;
    // Sonido "otro adivinó" (no para uno mismo: eso ya suena con SndYouGuessed).
    if ((APlayerState*)Guesser != GetOwningPlayerState() && SndOtherGuessed)
        UGameplayStatics::PlaySound2D(this, SndOtherGuessed);
    // El "+N" y el conteo animado se ven en la fila de CUALQUIERA que adivine, incluido uno mismo.

    // Registrar el flash "+N" para ese jugador; RebuildScoreboard lo aplica a su fila (y lo re-aplica
    // aunque el marcador se reconstruya al cambiar el puntaje). Se apaga solo al vencer el tiempo.
    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    GuessPlusUntil.Add(Guesser, Now + GuessFeedbackSeconds);
    GuessPlusPoints.Add(Guesser, Points);
    CachedScoreSig.Reset();  // forzar rebuild para que aparezca ya
    RebuildScoreboard();
}

void UPTGameplayHUDWidget::StopCountdownSound()
{
    if (CountdownAudio) { CountdownAudio->Stop(); CountdownAudio = nullptr; }
}

void UPTGameplayHUDWidget::StartNextRevealAnim()
{
    if (PendingReveals.Num() == 0) { bRevealAnimating = false; if (RevealLetterText) RevealLetterText->SetVisibility(ESlateVisibility::Collapsed); return; }

    APTSculptGameState* G = GetGS();
    const int32 Idx = PendingReveals[0];
    const TCHAR Ch = (G && G->MaskedWord.IsValidIndex(Idx)) ? G->MaskedWord[Idx] : TEXT('?');
    if (RevealLetterText)
    {
        RevealLetterText->SetText(FText::FromString(FString::Chr(Ch)));
        RevealLetterText->SetVisibility(ESlateVisibility::HitTestInvisible);
    }
    bRevealAnimating = true;
    if (RevealLetterAnim) PlayAnimation(RevealLetterAnim); // aparece al centro y sube desvaneciéndose
}

void UPTGameplayHUDWidget::OnRevealAnimFinished()
{
    // La animación aterrizó: AHORA sí se agrega la letra a la palabra (coincide con el final de la animación).
    if (PendingReveals.Num() > 0)
    {
        const int32 Idx = PendingReveals[0];
        PendingReveals.RemoveAt(0);
        APTSculptGameState* G = GetGS();
        if (G && G->MaskedWord.IsValidIndex(Idx) && DisplayedMask.IsValidIndex(Idx))
            DisplayedMask[Idx] = G->MaskedWord[Idx];
    }
    // Encadenar la próxima (si hay), o cerrar.
    if (PendingReveals.Num() > 0) StartNextRevealAnim();
    else { bRevealAnimating = false; if (RevealLetterText) RevealLetterText->SetVisibility(ESlateVisibility::Collapsed); }
}

void UPTGameplayHUDWidget::UpdateGameplaySounds(APTSculptGameState* G)
{
    if (!G) return;
    const EPTTurnPhase Phase = G->TurnPhase;
    // Fases con cuenta regresiva (tick + reloj): elegir palabra (15s) y dibujar.
    const bool bCountdownPhase = (Phase == EPTTurnPhase::ChoosingWord || Phase == EPTTurnPhase::Drawing);

    // ── Transiciones de fase (one-shots de fin de turno + reset de contadores) ──
    if (Phase != PrevSoundPhase)
    {
        StopCountdownSound(); // cortar cualquier clip de countdown de la fase anterior

        if (PrevSoundPhase == EPTTurnPhase::Drawing && Phase == EPTTurnPhase::TurnEnd)
        {
            if (G->bTurnEndedAllGuessed) { if (SndAllGuessed) UGameplayStatics::PlaySound2D(this, SndAllGuessed); }
            else if (!bLocalGuessed)     { if (SndTimeUp)     UGameplayStatics::PlaySound2D(this, SndTimeUp); }
        }
        if (bCountdownPhase) { PrevSecondsLeft = -1; bCountdownPlayed = false; } // reinicia el reloj de esta fase
        if (Phase == EPTTurnPhase::Drawing)
        {
            PrevMaskedForReveal.Reset();
            DisplayedMask.Reset(); PendingReveals.Reset(); bRevealAnimating = false;
            if (RevealLetterText) RevealLetterText->SetVisibility(ESlateVisibility::Collapsed);
        }
        PrevSoundPhase = Phase;
    }

    if (!bCountdownPhase) return;

    // ── Tick por segundo + countdown de los últimos 10 (en elegir-palabra y en dibujar) ──
    const float Rem  = (Phase == EPTTurnPhase::Drawing) ? G->GetTurnSecondsRemaining() : G->GetPhaseSecondsRemaining();
    const int32 Left = FMath::Max(0, FMath::CeilToInt(Rem));
    if (Left != PrevSecondsLeft)
    {
        if (PrevSecondsLeft != -1) // no sonar en el primer sample del turno
        {
            // Countdown (clip de 10s): una sola vez al llegar a 10.
            if (Left == 10 && !bCountdownPlayed)
            {
                bCountdownPlayed = true;
                if (SndCountdown) CountdownAudio = UGameplayStatics::SpawnSound2D(this, SndCountdown);
            }
            // Tick por segundo: suena SIEMPRE hasta el segundo 1 (se solapa con el countdown en los últimos 10).
            if (Left >= 1 && SndTickSecond) UGameplayStatics::PlaySound2D(this, SndTickSecond);
        }
        PrevSecondsLeft = Left;
    }

    if (Phase != EPTTurnPhase::Drawing) return; // la pista/revelado solo aplica dibujando

    // ── Pista ──
    const FString& Cur = G->MaskedWord;

    // Sonido: cuando aparece cualquier letra nueva en el mask real (para todos).
    if (!PrevMaskedForReveal.IsEmpty() && Cur.Len() == PrevMaskedForReveal.Len())
        for (int32 i = 0; i < Cur.Len(); ++i)
            if (PrevMaskedForReveal[i] == TEXT('_') && Cur[i] != TEXT('_') && Cur[i] != TEXT(' '))
            { if (SndHintLetter) UGameplayStatics::PlaySound2D(this, SndHintLetter); break; }
    PrevMaskedForReveal = Cur;

    // Visual con RETARDO, solo para los que adivinan (no escultor, no quien ya adivinó).
    const bool bGuesser = !G->IsLocalPlayerSculptor() && !bLocalGuessed;
    if (bGuesser && UseDelayedReveal())
    {
        // Inicializar la máscara mostrada al entrar (copia el estado actual: lo ya revelado se ve sin animar).
        if (DisplayedMask.Len() != Cur.Len()) { DisplayedMask = Cur; PendingReveals.Reset(); }

        // Encolar las letras que están reveladas en el mask real pero todavía no se mostraron ni están en cola.
        for (int32 i = 0; i < Cur.Len(); ++i)
            if (Cur[i] != TEXT('_') && Cur[i] != TEXT(' ')
                && DisplayedMask.IsValidIndex(i) && DisplayedMask[i] == TEXT('_')
                && !PendingReveals.Contains(i))
                PendingReveals.Add(i);

        if (!bRevealAnimating && PendingReveals.Num() > 0) StartNextRevealAnim();
    }
}

void UPTGameplayHUDWidget::OnAllGuessed()
{
    // Aviso al escultor: ya adivinaron todos (el turno se cierra enseguida). Texto multi-idioma.
    if (TxtAllGuessed)
        TxtAllGuessed->SetText(PTText::Get(FName(TEXT("GUESS_ALL"))));
    if (AllGuessedPopup)
    {
        AllGuessedPopup->SetVisibility(ESlateVisibility::HitTestInvisible);
        if (UWorld* W = GetWorld())
            W->GetTimerManager().SetTimer(AllGuessedTimer, this, &UPTGameplayHUDWidget::HideAllGuessedPopup,
                                          GuessFeedbackSeconds, false);
    }
}

void UPTGameplayHUDWidget::HideAllGuessedPopup()
{
    if (AllGuessedPopup) AllGuessedPopup->SetVisibility(ESlateVisibility::Collapsed);
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

    const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
    auto IsFlashing = [&](APTPlayerState* PT) -> bool
    {
        const float* Until = GuessPlusUntil.Find(PT);
        return Until && Now < *Until;
    };

    // Firma del estado visible: solo reconstruimos las filas si algo cambió (no cada tick). Incluye el
    // flash "+N" para que el marcador se rearme cuando aparece Y cuando se vence (y así ocultarlo).
    FString Sig;
    for (APTPlayerState* PT : Players)
        Sig += FString::Printf(TEXT("%s:%d:%d:%d|"), *PT->GetDisplayNameSafe(),
                               PT->GameScore, PT == G->CurrentSculptor ? 1 : 0, IsFlashing(PT) ? 1 : 0);
    if (Sig == CachedScoreSig) return;
    CachedScoreSig = Sig;

    ScoreboardBox->ClearChildren();
    for (APTPlayerState* PT : Players)
    {
        UPTScoreRowWidget* Row = CreateWidget<UPTScoreRowWidget>(GetOwningPlayer(), ScoreRowClass);
        if (!Row) continue;
        Row->SetRow(PT->GetDisplayNameSafe(), PT->GameScore, PT == G->CurrentSculptor);
        // "+N" al lado del nombre + conteo animado del puntaje si este jugador acaba de adivinar.
        if (IsFlashing(PT))
        {
            const int32* Pts = GuessPlusPoints.Find(PT);
            const int32 Earned = Pts ? *Pts : 0;
            Row->SetGuessPlus(Earned, true);
            // Conteo con rebote desde el puntaje anterior (final - ganados) hasta el actual.
            Row->AnimateScore(PT->GameScore - Earned, PT->GameScore, 1.5f);
        }
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
