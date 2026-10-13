// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTUserWidget.h"
#include "../PTGameInstance.h"
#include "../PTTextTable.h" // auto-traducción inmediata al construir (evita el parpadeo base→idioma)
#include "Components/Widget.h"
#include "Components/Image.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Blueprint/WidgetTree.h"
#include "UObject/UObjectIterator.h"
#include "TimerManager.h"
#include "Engine/World.h"

namespace
{
    constexpr float PT_POPIN_STEP = 1.f / 60.f; // paso del timer (~60 fps)

    // Pila de ventanas abiertas con bGameLogoHidesOthers (pausa, settings…), la última arriba. Solo se ve el
    // logo de la de arriba; sin ninguna abierta se ven los demás (esquina del HUD / menú principal).
    TArray<TWeakObjectPtr<UPTUserWidget>> GGameLogoOwners;

    UPTUserWidget* TopGameLogoOwner(const UWorld* World)
    {
        for (int32 i = GGameLogoOwners.Num() - 1; i >= 0; --i)
        {
            UPTUserWidget* W = GGameLogoOwners[i].Get();
            if (W && W->GetWorld() == World) return W;
        }
        return nullptr;
    }
}

void UPTUserWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Aplicar los sonidos de UI (definidos en el GameInstance) a todos los botones de este widget.
    // Cada widget que deriva de esta base lo hace solo → no hay que tocar botón por botón, y los
    // widgets creados dinámicamente (filas de sesión, slots, etc.) también quedan cubiertos.
    if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
        GI->ApplyUIButtonSounds(this);

    // Traducir el árbol de este widget YA, antes del primer frame, para que no se vea el texto base
    // (inglés) por medio segundo hasta el próximo barrido del PTLocalizationSubsystem. Es idempotente.
    PTText::LocalizeWidgetTree(this);

    ApplyGameLogo();
    // Un dueño del logo que ya arranca visible (lo normal es que arranque colapsado).
    if (bGameLogoHidesOthers && ShouldShowGameLogo() && IsVisible()) SetGameLogoOwner(true);

    if (bAutoPopIn) PlayPopIn();
}

void UPTUserWidget::NativeDestruct()
{
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(PopInTimer);
        W->GetTimerManager().ClearTimer(GameLogoPanelTimer);
    }
    // Si se destruye abierto (cambio de nivel con la pausa abierta), devolverles el logo a los demás.
    SetGameLogoOwner(false);
    Super::NativeDestruct();
}

// ==========================================================================
// Logo del juego
// ==========================================================================

void UPTUserWidget::ApplyGameLogo()
{
    const bool bWant = ShouldShowGameLogo();
    if (!bWant && !GameLogo) return;

    UTexture2D* Tex = bWant ? GameLogoTexture.LoadSynchronous() : nullptr;
    if (bWant && !Tex)
        UE_LOG(LogTemp, Warning, TEXT("[Branding] %s: no se pudo cargar GameLogoTexture (%s)."),
               *GetClass()->GetName(), *GameLogoTexture.ToString());

    // El WBP no trae un Image "GameLogo": crearlo en la raíz (Canvas u Overlay), una sola vez.
    if (!GameLogo && Tex && WidgetTree)
    {
        UImage* Img = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GameLogo"));
        const FVector2D A = GameLogoAnchor;
        const FVector2D Off = GameLogoOffset;
        if (UCanvasPanel* Canvas = Cast<UCanvasPanel>(GetRootWidget()))
        {
            UCanvasPanelSlot* S = Canvas->AddChildToCanvas(Img);
            S->SetAnchors(FAnchors(A.X, A.Y));
            S->SetAlignment(A);
            S->SetPosition(Off);
            S->SetAutoSize(true);  // tamaño = ImageSize del brush (ancho pedido + alto proporcional)
            S->SetZOrder(1000);    // encima de todo lo de este widget
        }
        else if (UOverlay* Overlay = Cast<UOverlay>(GetRootWidget()))
        {
            // En un Overlay no hay anclas: se aproxima con alineación + padding.
            UOverlaySlot* S = Overlay->AddChildToOverlay(Img);
            S->SetHorizontalAlignment(A.X < 0.25f ? HAlign_Left : (A.X > 0.75f ? HAlign_Right  : HAlign_Center));
            S->SetVerticalAlignment  (A.Y < 0.25f ? VAlign_Top  : (A.Y > 0.75f ? VAlign_Bottom : VAlign_Center));
            S->SetPadding(FMargin(FMath::Max(0.f, Off.X), FMath::Max(0.f, Off.Y),
                                  FMath::Max(0.f, -Off.X), FMath::Max(0.f, -Off.Y)));
        }
        else
        {
            UE_LOG(LogTemp, Warning, TEXT("[Branding] %s: la raíz del WBP no es Canvas ni Overlay; agregá un Image "
                   "llamado \"GameLogo\" donde quieras el logo."), *GetClass()->GetName());
            return;
        }
        GameLogo = Img;
        bGameLogoCreated = true;
    }
    if (!GameLogo) return;

    // Si ya hay una ventana abierta que se quedó con el logo (p.ej. pausa), este arranca escondido.
    const UPTUserWidget* Top = TopGameLogoOwner(GetWorld());
    bGameLogoSuppressed = Top && Top != this;

    // Sobre el panel: solo con un logo creado en una raíz Canvas (si no, queda donde está y se muestra ya).
    // Se reubica con un timer porque el panel recién tiene tamaño al dibujarse y puede cambiar de alto.
    const bool bOnPanel = bGameLogoOnPanel && bGameLogoCreated && Cast<UCanvasPanelSlot>(GameLogo->Slot);
    if (!bOnPanel) bGameLogoPlaced = true;
    else if (UWorld* W = GetWorld())
        W->GetTimerManager().SetTimer(GameLogoPanelTimer, this, &UPTUserWidget::UpdateGameLogoOnPanel, 0.05f, true);

    if (Tex)
    {
        FSlateBrush Brush = GameLogo->GetBrush();
        if (bGameLogoCreated)
        {
            Brush.SetResourceObject(Tex);
            // Proporción del archivo importado: GetSurfaceWidth/Height dan un tamaño provisorio mientras el
            // editor todavía compila la textura (primera carga) y el logo salía deformado.
            FIntPoint Src = Tex->GetImportedSize();
            if (Src.X <= 0 || Src.Y <= 0) Src = FIntPoint(Tex->GetSizeX(), Tex->GetSizeY());
            const float H = GameLogoWidth * Src.Y / FMath::Max(1, Src.X);
            Brush.ImageSize = FVector2D(GameLogoWidth, H);
            GameLogo->SetBrush(Brush);
            GameLogo->SetColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, GameLogoOpacity));
        }
        else if (!Brush.GetResourceObject())
        {
            // Image del WBP sin textura asignada: ponerle la del logo y dejar su layout como está.
            GameLogo->SetBrushFromTexture(Tex, /*bMatchSize=*/false);
        }
    }
    UpdateGameLogoVisibility();
}

void UPTUserWidget::UpdateGameLogoVisibility()
{
    if (!GameLogo) return;
    const bool bShow = ShouldShowGameLogo() && !bGameLogoSuppressed && bGameLogoPlaced
                    && GameLogo->GetBrush().GetResourceObject();
    GameLogo->SetVisibility(bShow ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}

void UPTUserWidget::UpdateGameLogoOnPanel()
{
    UCanvasPanel* Canvas = Cast<UCanvasPanel>(GetRootWidget());
    UCanvasPanelSlot* LogoSlot = GameLogo ? Cast<UCanvasPanelSlot>(GameLogo->Slot) : nullptr;
    if (!Canvas || !LogoSlot || !IsVisible()) return;

    // Panel principal = el hijo visible más grande de la raíz (sin contar el logo).
    UWidget* Panel = nullptr;
    float BestArea = 0.f;
    for (UWidget* Child : Canvas->GetAllChildren())
    {
        if (!Child || Child == GameLogo || !Child->IsVisible()) continue;
        const FVector2D S = Child->GetCachedGeometry().GetLocalSize();
        if (S.X * S.Y > BestArea) { BestArea = S.X * S.Y; Panel = Child; }
    }
    if (!Panel) return; // todavía no se dibujó: esperar al próximo tick (el logo sigue oculto)

    // Centro del borde de arriba del panel, en coordenadas de la raíz. Con el "blop" (escala 0) la
    // conversión no es válida: se saltea ese tick.
    const FGeometry& RootGeo  = Canvas->GetCachedGeometry();
    const FGeometry& PanelGeo = Panel->GetCachedGeometry();
    const FVector2D TopCenter = RootGeo.AbsoluteToLocal(
        PanelGeo.LocalToAbsolute(FVector2D(PanelGeo.GetLocalSize().X * 0.5f, 0.f)));
    if (!FMath::IsFinite(TopCenter.X) || !FMath::IsFinite(TopCenter.Y)) return;

    LogoSlot->SetAnchors(FAnchors(0.f, 0.f));
    LogoSlot->SetAlignment(FVector2D(0.5f, 1.f - GameLogoPanelOverlap));
    LogoSlot->SetPosition(TopCenter + GameLogoOffset);
    if (!bGameLogoPlaced)
    {
        bGameLogoPlaced = true;
        UpdateGameLogoVisibility();
    }
}

void UPTUserWidget::SetVisibility(ESlateVisibility InVisibility)
{
    Super::SetVisibility(InVisibility);
    if (bGameLogoHidesOthers && ShouldShowGameLogo()) SetGameLogoOwner(IsVisible());
}

void UPTUserWidget::SetGameLogoOwner(bool bOwner)
{
    UWorld* World = GetWorld();
    const int32 Idx = GGameLogoOwners.IndexOfByKey(this);
    if (bOwner == (Idx != INDEX_NONE) && (!bOwner || Idx == GGameLogoOwners.Num() - 1)) return; // sin cambios
    GGameLogoOwners.RemoveAll([this](const TWeakObjectPtr<UPTUserWidget>& W) { return !W.IsValid() || W.Get() == this; });
    if (bOwner) GGameLogoOwners.Add(this); // el último que se abre queda arriba (Settings sobre la pausa)
    if (World) RefreshGameLogos(World);
}

void UPTUserWidget::RefreshGameLogos(UWorld* World)
{
    // Se ve SOLO el logo del dueño de arriba (o, sin dueños abiertos, los de siempre): nunca dos a la vez.
    UPTUserWidget* Top = TopGameLogoOwner(World);
    for (TObjectIterator<UPTUserWidget> It; It; ++It)
    {
        UPTUserWidget* W = *It;
        if (!IsValid(W) || W->IsTemplate() || !W->GameLogo || W->GetWorld() != World) continue;
        W->bGameLogoSuppressed = Top && Top != W;
        W->UpdateGameLogoVisibility();
    }
}

void UPTUserWidget::PlayPopIn()
{
    PlayPopInOn(this);
}

void UPTUserWidget::PlayPopInOn(UWidget* Target)
{
    UWorld* W = GetWorld();
    if (!W || !Target) return;

    PopInTarget = Target;
    // Pivote en el centro para que escale desde el medio; arranca en 0.
    Target->SetRenderTransformPivot(FVector2D(0.5f, 0.5f));
    Target->SetRenderScale(FVector2D(0.f, 0.f));
    PopInElapsed = 0.f;

    W->GetTimerManager().SetTimer(PopInTimer, this, &UPTUserWidget::PopInStep, PT_POPIN_STEP, /*loop=*/true);
}

void UPTUserWidget::PopInStep()
{
    UWidget* Target = PopInTarget.Get();
    if (!Target)
    {
        if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(PopInTimer);
        return;
    }

    PopInElapsed += PT_POPIN_STEP;
    const float Dur = FMath::Max(0.05f, PopInDuration);
    const float T   = FMath::Clamp(PopInElapsed / Dur, 0.f, 1.f);

    // Ease "back out": sobrepasa un poco 1.0 y vuelve → efecto de rebote/blop.
    const float C1 = 1.70158f;
    const float C3 = C1 + 1.f;
    const float U  = T - 1.f;
    const float Scale = 1.f + C3 * U * U * U + C1 * U * U;

    Target->SetRenderScale(FVector2D(Scale, Scale));

    if (T >= 1.f)
    {
        Target->SetRenderScale(FVector2D(1.f, 1.f)); // asentar exacto
        if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(PopInTimer);
    }
}
