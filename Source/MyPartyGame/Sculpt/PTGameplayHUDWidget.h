// HUD de la partida de Sculpturillo. Toda la lógica vive acá (C++): estado por fase,
// reloj, elección de palabra, chat con anti-spoiler y modo de input por fase.
// El WBP_GameplayHUD solo aporta los widgets (con estos nombres) y el layout.

#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PTSculptGameState.h" // EPTTurnPhase, EPTChatType
#include "PTGameplayHUDWidget.generated.h"

class UTextBlock;
class UButton;
class UEditableTextBox;
class UScrollBox;
class APTSculptPlayerController;

UCLASS()
class MYPARTYGAME_API UPTGameplayHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    /** Llamar desde el PlayerController al crear el HUD (mismo patrón que el lobby). */
    UFUNCTION(BlueprintCallable, Category="Game")
    void ShowHUD();

    /** Abre/enfoca el chat (lo llama el PlayerController con Enter). Muestra cursor y
     *  da foco al input; al enviar vuelve solo a Game Only. */
    void FocusChat();

    /** true mientras el chat está abierto/enfocado. Lo consulta el PlayerController para no
     *  interpretar teclas de juego (ej: BACKSPACE de "borrar todo") mientras estás escribiendo. */
    bool IsChatOpen() const { return bChatOpen; }

    /** Rellena la lista de controles desde PTInput::GetBindings() (fuente de verdad única).
     *  Llamar de nuevo tras un rebind y la UI queda actualizada. */
    void RebuildControls();

    /** Arma la barra de herramientas (una sola vez): tools 1/2/3/4 + formas (TAB). */
    void BuildToolbar();
    /** Actualiza qué está equipado y qué barras se ven según el modo. Se llama desde RefreshTick. */
    void RefreshToolbar();

protected:
    virtual bool Initialize() override;
    virtual void NativeDestruct() override;
    // Solo para el círculo de "borrar todo": RefreshTick va a 10Hz y se vería a saltos.
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

    // ── Widgets del WBP (todos opcionales: el WBP compila aunque falte alguno) ──
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtSculptor;   // "X está esculpiendo"
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtWord;       // "_ _ _ _" o la palabra
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtTimer;      // segundos restantes
    UPROPERTY(meta=(BindWidgetOptional)) UWidget*         WordPickPanel; // panel de las 3 palabras
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtChooseTimer;// cuenta regresiva DENTRO del panel de elección (opcional)
    UPROPERTY(meta=(BindWidgetOptional)) UButton*         BtnWord0;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*         BtnWord1;
    UPROPERTY(meta=(BindWidgetOptional)) UButton*         BtnWord2;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtWord0;      // texto dentro de BtnWord0
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtWord1;
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*      TxtWord2;
    UPROPERTY(meta=(BindWidgetOptional)) UScrollBox*          ChatScroll;

    // ── Lista de controles completa (opcional: para una pantalla de ayuda/settings) ────
    UPROPERTY(meta=(BindWidgetOptional)) class UPanelWidget*  ControlsBox;
    UPROPERTY(EditAnywhere, Category="UI")
    TSubclassOf<class UPTControlRowWidget> ControlRowClass;

    // ── Barra de herramientas (esquina inferior derecha, estilo hotbar) ──────
    // Contenedores en el WBP (HorizontalBox): C++ los llena con cuadritos.
    UPROPERTY(meta=(BindWidgetOptional)) class UPanelWidget* ToolsBox;   // 1/2/3/4: Add/Erase/Paint/Ojos
    UPROPERTY(meta=(BindWidgetOptional)) class UPanelWidget* ShapesBox;  // TAB: formas (solo en Add/Paint)
    UPROPERTY(meta=(BindWidgetOptional)) class UPanelWidget* HintsBox;   // atajos contextuales (Z/X, E...)
    // Cuadrito de "borrar todo" (BACKSPACE mantenido): contenedor propio porque, a diferencia de
    // los hints, NO se rearma por contexto — se construye una vez y se le actualiza el progreso.
    UPROPERTY(meta=(BindWidgetOptional)) class UPanelWidget* ClearBox;

    // Ícono de "prohibido construir" (🚫) en el centro de la pantalla: aparece cuando apuntás
    // fuera de la zona de modelado, con CUALQUIER herramienta. La textura se asigna directo en el
    // brush de este Image dentro del WBP; C++ solo lo muestra/oculta.
    UPROPERTY(meta=(BindWidgetOptional)) class UImage* OutOfBoundsIcon;

    /** WBP del cuadrito (derivado de UPTToolSlotWidget). */
    UPROPERTY(EditAnywhere, Category="UI")
    TSubclassOf<class UPTToolSlotWidget> ToolSlotClass;

    // Iconos de cada tool/forma/atajo. Asignar en el WBP; si falta alguno el cuadrito muestra
    // igual la tecla y el nombre.
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconAdd       = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconErase     = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconPaint     = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconEyes      = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconSphere    = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconCube      = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconCylinder  = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconCone      = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconAxisVert  = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconAxisHoriz = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconSaveColor = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconClearAll  = nullptr;
    UPROPERTY(EditAnywhere, Category="UI|Icons") UTexture2D* IconDetail    = nullptr; // ALT (detalle)

    // Cuadritos creados (para actualizar el equipado sin reconstruir).
    UPROPERTY() TArray<class UPTToolSlotWidget*> ToolSlots;   // orden: Add, Erase, Paint, Ojos
    UPROPERTY() TArray<class UPTToolSlotWidget*> ShapeSlots;  // orden: Sphere, Cube, Cylinder, Cono
    UPROPERTY() TArray<class UPTToolSlotWidget*> HintSlots;
    UPROPERTY() class UPTToolSlotWidget* ClearSlot = nullptr; // BACKSPACE (con círculo de progreso)
    FString CachedHintSig; // los atajos contextuales solo se rearman si cambia el contexto
    // RichTextBlock: el nombre usa el estilo "name" (color); el mensaje queda en el default.
    UPROPERTY(meta=(BindWidgetOptional)) class URichTextBlock* TxtChat;    // log de chat (Auto Wrap)
    UPROPERTY(meta=(BindWidgetOptional)) UEditableTextBox* ChatInput;

    // ── Rondas + marcador + pantalla de fin (todos opcionales) ──
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock*       TxtRound;      // "Ronda 2 / 3"
    // Contenedor del marcador en vivo (VerticalBox/ScrollBox). C++ crea una fila por jugador.
    UPROPERTY(meta=(BindWidgetOptional)) class UPanelWidget* ScoreboardBox;
    UPROPERTY(meta=(BindWidgetOptional)) UWidget*    ResultsPanel;   // se muestra solo en GameOver
    UPROPERTY(meta=(BindWidgetOptional)) UTextBlock* TxtResults;     // ranking final (multilínea)
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    BtnPlayAgain;   // solo host
    UPROPERTY(meta=(BindWidgetOptional)) UButton*    BtnReturnLobby; // solo host

    // Clase de la fila del marcador (asignar WBP_ScoreRow reparentado a UPTScoreRowWidget).
    UPROPERTY(EditAnywhere, Category="UI")
    TSubclassOf<class UPTScoreRowWidget> ScoreRowClass;

    UFUNCTION() void OnBtnWord0();
    UFUNCTION() void OnBtnWord1();
    UFUNCTION() void OnBtnWord2();
    UFUNCTION() void OnChatCommitted(const FText& Text, ETextCommit::Type CommitMethod);
    UFUNCTION() void OnChatLine(const FString& Name, const FString& Message, EPTChatType Type);
    UFUNCTION() void OnBtnPlayAgain();
    UFUNCTION() void OnBtnReturnLobby();

private:
    FTimerHandle RefreshTimer;
    FDelegateHandle LanguageHandle; // suscripción a PTText::OnLanguageChanged
    FString      ChatLog;
    bool         bChatBound     = false;
    bool         bChatOpen      = false;
    bool         bInputModeInit = false;
    bool         bWantsGameOnly = false;

    APTSculptGameState*        GetGS() const;
    APTSculptPlayerController* GetSculptPC() const;

    void RefreshTick();          // polling: pinta estado/reloj/panel/input según la fase
    void ChooseWord(int32 Index);
    void ApplyInputMode(bool bGameOnly);
    FString BuildScoreboard() const; // "Nombre: pts" ordenado por puntaje desc. (para resultados)
    void    RebuildScoreboard();     // rehace las filas del marcador en vivo (solo si cambió)
    FString CachedScoreSig;          // evita reconstruir cada tick si nada cambió
    bool    IsLocalPlayerHost() const;
};
