// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLockerWidget.h"
#include "PTLockerSlotWidget.h"
#include "../UI/PTSkinWorkshopWidget.h"
#include "../UI/PTToolSlotWidget.h"
#include "PTLockerSubsystem.h"
#include "PTLobbyPlayerController.h"
#include "PTLobbyCharacter.h"
#include "../PTTextTable.h"
#include "Components/PanelWidget.h"
#include "Components/PanelSlot.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

void UPTLockerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (HeadTabButton)    HeadTabButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnHeadTabClicked);
    if (BodyTabButton)    BodyTabButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnBodyTabClicked);
    if (AssignButton)     AssignButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnAssignClicked);
    if (EditActionButton) EditActionButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnEditClicked);
    if (BackButton)       BackButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnBackClicked);
    if (SkinWorkshopButton) SkinWorkshopButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnSkinWorkshopClicked);
    if (EmptySlotButton)       EmptySlotButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnEmptySlotClicked);
    if (ClearConfirmYesButton) ClearConfirmYesButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnClearConfirmYes);
    if (ClearConfirmNoButton)  ClearConfirmNoButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnClearConfirmNo);
    if (ClearConfirmText)      ClearConfirmText->SetText(PTText::Get(TEXT("LOCKER_CLEAR_CONFIRM")));
    if (ClearConfirmPanel)     ClearConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
    // Cruz WASD (igual que el HeadSculptHUD): poner los keycaps una vez; el "apretado" se actualiza en el tick.
    if (WasdUp)    WasdUp->SetSlot(nullptr,    FText::FromString(TEXT("W")), FText::GetEmpty());
    if (WasdDown)  WasdDown->SetSlot(nullptr,  FText::FromString(TEXT("S")), FText::GetEmpty());
    if (WasdLeft)  WasdLeft->SetSlot(nullptr,  FText::FromString(TEXT("A")), FText::GetEmpty());
    if (WasdRight) WasdRight->SetSlot(nullptr, FText::FromString(TEXT("D")), FText::GetEmpty());
    // Equipar = click en slot lleno; Crear = click en slot vacío. Ya no hay botón "Asignar/Crear".
    if (AssignButton)     AssignButton->SetVisibility(ESlateVisibility::Collapsed);
    BuildSlots();
    // En modo "un solo slot" (una sola grilla de skins) no hay pestañas Cabeza/Cuerpo: ocultarlas.
    if (bSkinMode)
    {
        if (HeadTabButton) HeadTabButton->SetVisibility(ESlateVisibility::Collapsed);
        if (BodyTabButton) BodyTabButton->SetVisibility(ESlateVisibility::Collapsed);
    }
    SwitchTab(0);
}

UPTLockerSubsystem* UPTLockerWidget::Locker() const
{
    return GetGameInstance() ? GetGameInstance()->GetSubsystem<UPTLockerSubsystem>() : nullptr;
}
APTLobbyPlayerController* UPTLockerWidget::LobbyPC() const
{
    return Cast<APTLobbyPlayerController>(GetOwningPlayer());
}
TArray<UPTLockerSlotWidget*>& UPTLockerWidget::ActiveList()
{
    if (bSkinMode) return SkinSlotWidgets;
    return (ActiveTab == 0) ? HeadSlotWidgets : BodySlotWidgets;
}
int32 UPTLockerWidget::ActiveCount() const
{
    if (bSkinMode) return SkinSlotWidgets.Num();
    return (ActiveTab == 0) ? HeadSlotWidgets.Num() : BodySlotWidgets.Num();
}

void UPTLockerWidget::BuildSlots()
{
    if (bBuilt || !SlotWidgetClass) return;
    bBuilt = true;
    UPTLockerSubsystem* L = Locker();
    if (L) L->RefreshSlotCount(); // releer el máximo de slots (Project Settings) antes de armar la grilla
    const int32 NHead = L ? L->NumHeadSlots() : UPTLockerSaveGame::DefaultSlots;
    const int32 NBody = L ? L->NumBodySlots() : UPTLockerSaveGame::DefaultSlots;
    auto Make = [this](UPanelWidget* Box, int32 Count, TArray<UPTLockerSlotWidget*>& Out)
    {
        if (!Box) return;
        Box->ClearChildren();
        Out.Reset();
        const int32 PerRow = FMath::Max(1, SlotsPerRow);
        for (int32 i = 0; i < Count; ++i)
            if (UPTLockerSlotWidget* S = CreateWidget<UPTLockerSlotWidget>(this, SlotWidgetClass))
            {
                UPanelSlot* PS = Box->AddChild(S);
                // Si el contenedor es un Uniform Grid, hay que asignar fila/columna a mano (si no, todos
                // caen en la celda 0,0 y se ven encimados). Horizontal/Vertical/Wrap Box ya se ordenan solos.
                if (UUniformGridSlot* GS = Cast<UUniformGridSlot>(PS))
                {
                    GS->SetRow(i / PerRow);
                    GS->SetColumn(i % PerRow);
                }
                Out.Add(S);
            }
    };
    // Modo "un solo slot": si el WBP trae SkinSlotsBox, UNA sola grilla de skins (cabeza+cuerpo por índice).
    // Si no, el modo clásico de dos grillas (cabeza / cuerpo) con pestañas.
    if (SkinSlotsBox)
    {
        bSkinMode = true;
        Make(SkinSlotsBox, FMath::Min(NHead, NBody), SkinSlotWidgets);
    }
    else
    {
        bSkinMode = false;
        Make(HeadSlotsBox, NHead, HeadSlotWidgets);
        Make(BodySlotsBox, NBody, BodySlotWidgets);
    }
}

void UPTLockerWidget::RefreshSlots()
{
    UPTLockerSubsystem* L = Locker();
    if (!L) return;
    if (bSkinMode)
    {
        const int32 EqSkin = L->GetEquippedSkin();
        for (int32 i = 0; i < SkinSlotWidgets.Num(); ++i)
            if (UPTLockerSlotWidget* S = SkinSlotWidgets[i])
            {
                S->SetupSkin(this, i, L->IsSkinSlotUsed(i), EqSkin == i);
                S->SetThumbnailTexture(APTLobbyCharacter::MakeTextureFromPNG(S, L->GetSkinThumb(i)));
            }
        ApplySelectionVisual();
        return;
    }
    for (int32 i = 0; i < HeadSlotWidgets.Num(); ++i)
        if (UPTLockerSlotWidget* S = HeadSlotWidgets[i])
        {
            S->Setup(this, i, true, L->IsHeadSlotUsed(i), L->GetEquippedHead() == i);
            S->SetThumbnailTexture(APTLobbyCharacter::MakeTextureFromPNG(S, L->GetHeadThumb(i)));
        }
    for (int32 i = 0; i < BodySlotWidgets.Num(); ++i)
        if (UPTLockerSlotWidget* S = BodySlotWidgets[i])
        {
            S->Setup(this, i, false, L->IsBodySlotUsed(i), L->GetEquippedBody() == i);
            S->SetThumbnailTexture(APTLobbyCharacter::MakeTextureFromPNG(S, L->GetBodyThumb(i)));
        }
    ApplySelectionVisual();
}

void UPTLockerWidget::SwitchTab(int32 Tab)
{
    if (bSkinMode)
    {
        // No hay pestañas: arrancar seleccionando la skin EQUIPADA y refrescar la grilla.
        if (SkinSlotsBox) SkinSlotsBox->SetVisibility(ESlateVisibility::Visible);
        SelectedIndex = 0;
        if (UPTLockerSubsystem* L = Locker()) SelectedIndex = FMath::Max(0, L->GetEquippedSkin());
        RefreshSlots();
        return;
    }
    ActiveTab = FMath::Clamp(Tab, 0, 1);
    // Arrancar seleccionando lo EQUIPADO de esa pestaña (así ves marcado lo que tenés puesto).
    SelectedIndex = 0;
    if (UPTLockerSubsystem* L = Locker())
        SelectedIndex = FMath::Max(0, ActiveTab == 0 ? L->GetEquippedHead() : L->GetEquippedBody());
    // Mostrar el panel activo, ocultar el otro.
    if (HeadSlotsBox) HeadSlotsBox->SetVisibility(ActiveTab == 0 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (BodySlotsBox) BodySlotsBox->SetVisibility(ActiveTab == 1 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ApplyTabVisual();
    RefreshSlots();
}

void UPTLockerWidget::ApplyTabVisual()
{
    // Tiñe el fondo del botón de la pestaña activa (la otra queda en el color inactivo).
    if (HeadTabButton) HeadTabButton->SetBackgroundColor(ActiveTab == 0 ? TabActiveColor : TabInactiveColor);
    if (BodyTabButton) BodyTabButton->SetBackgroundColor(ActiveTab == 1 ? TabActiveColor : TabInactiveColor);
}

void UPTLockerWidget::SelectSlot(int32 Index, bool bHead)
{
    if (!bSkinMode && bHead != (ActiveTab == 0)) SwitchTab(bHead ? 0 : 1);
    SelectedIndex = FMath::Clamp(Index, 0, FMath::Max(0, ActiveCount() - 1));
    ApplySelectionVisual();
}

void UPTLockerWidget::HoverSlot(int32 Index, bool bHead)
{
    // Hover sobre slot LLENO: lo selecciona (Editar apunta ahí) y previsualiza la skin en el personaje.
    TArray<UPTLockerSlotWidget*>& List = bSkinMode ? SkinSlotWidgets : (bHead ? HeadSlotWidgets : BodySlotWidgets);
    if (!List.IsValidIndex(Index) || !List[Index] || !List[Index]->IsUsed()) return;
    SelectSlot(Index, bHead);
    if (APTLobbyPlayerController* PC = LobbyPC())
    {
        if (bSkinMode) PC->PreviewSkinSlot(Index);
        else           PC->PreviewLookSlot(Index, bHead);
    }
    bPreviewingHover = true;
}

void UPTLockerWidget::EndHoverPreview()
{
    // Al salir de los slots: el personaje y la selección vuelven a lo EQUIPADO (Editar = el equipado).
    if (APTLobbyPlayerController* PC = LobbyPC()) PC->RevertLookPreview();
    if (UPTLockerSubsystem* L = Locker())
    {
        if (bSkinMode) SelectSlot(FMath::Max(0, L->GetEquippedSkin()), true);
        else           SelectSlot(ActiveTab == 0 ? FMath::Max(0, L->GetEquippedHead()) : FMath::Max(0, L->GetEquippedBody()), ActiveTab == 0);
    }
    bPreviewingHover = false;
}

void UPTLockerWidget::CreateSlotNow(int32 Index, bool bHead)
{
    // Click en slot VACÍO → entra directo a crearlo (sin pasar por un botón Crear).
    SelectSlot(Index, bHead);
    EditSelected(); // skin: EnterSkinEditForSlot; clásico: EnterHeadSculptForSlot / EnterBodyPaintForSlot
}

void UPTLockerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);

    // Cruz WASD: se "aprietan" (animación OnPressed del WBP_ToolSlot) mientras mantenés la tecla, igual
    // que en el HeadSculptHUD. Así ves que te movés con el personaje mientras elegís skins.
    if (WasdUp || WasdDown || WasdLeft || WasdRight)
        if (APlayerController* PC = GetOwningPlayer())
        {
            if (WasdUp)    WasdUp->SetPressed(PC->IsInputKeyDown(EKeys::W));
            if (WasdDown)  WasdDown->SetPressed(PC->IsInputKeyDown(EKeys::S));
            if (WasdLeft)  WasdLeft->SetPressed(PC->IsInputKeyDown(EKeys::A));
            if (WasdRight) WasdRight->SetPressed(PC->IsInputKeyDown(EKeys::D));
        }

    // Si estábamos previsualizando por hover y el mouse ya no está sobre NINGÚN slot (te fuiste del
    // menú o quedaste en un hueco), volver al equipado. Así el preview nunca se queda "pegado".
    if (!bPreviewingHover) return;
    bool bAnyHovered = false;
    for (UPTLockerSlotWidget* S : ActiveList())
        if (S && S->IsSlotHovered()) { bAnyHovered = true; break; }
    if (!bAnyHovered) EndHoverPreview();
}

void UPTLockerWidget::EquipSlotNow(int32 Index, bool bHead)
{
    TArray<UPTLockerSlotWidget*>& List = bSkinMode ? SkinSlotWidgets : (bHead ? HeadSlotWidgets : BodySlotWidgets);
    if (!List.IsValidIndex(Index) || !List[Index] || !List[Index]->IsUsed()) return; // vacío: no equipa
    SelectSlot(Index, bHead);
    if (APTLobbyPlayerController* PC = LobbyPC())
    {
        if (bSkinMode)  PC->EquipSkinSlot(Index);
        else if (bHead) PC->EquipHeadSlot(Index);
        else            PC->EquipBodySlot(Index);
    }
    RefreshSlots();
}

void UPTLockerWidget::MoveSelection(int32 DX, int32 DY)
{
    const int32 N = ActiveCount();
    if (N <= 0) return;
    const int32 Cols = FMath::Max(1, SlotsPerRow);
    const int32 Rows = FMath::DivideAndRoundUp(N, Cols);
    int32 R = SelectedIndex / Cols;
    int32 C = SelectedIndex % Cols;

    // Horizontal: mover columna con wrap; si cae en celda vacía (última fila incompleta), seguir buscando.
    if (DX != 0)
        for (int32 k = 0; k < Cols; ++k) { C = (C + DX + Cols) % Cols; if (R * Cols + C < N) break; }
    // Vertical: mover fila con wrap; saltear filas donde esa columna no tiene slot.
    if (DY != 0)
        for (int32 k = 0; k < Rows; ++k) { R = (R + DY + Rows) % Rows; if (R * Cols + C < N) break; }

    SelectedIndex = FMath::Clamp(R * Cols + C, 0, N - 1);
    ApplySelectionVisual();
}

void UPTLockerWidget::ApplySelectionVisual()
{
    // Al navegar/refrescar, cancelar cualquier confirmación de vaciado pendiente (evita borrar el slot
    // equivocado si te moviste después de abrir el popup).
    if (ClearConfirmPanel) ClearConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);

    TArray<UPTLockerSlotWidget*>& List = ActiveList();
    for (int32 i = 0; i < List.Num(); ++i)
        if (List[i]) List[i]->SetSelected(i == SelectedIndex);

    // Editar solo tiene sentido si el slot está lleno (crear/equipar son con click directo en el slot).
    const bool bUsed = List.IsValidIndex(SelectedIndex) && List[SelectedIndex] && List[SelectedIndex]->IsUsed();
    if (EditActionButton) EditActionButton->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    // Vaciar: solo si el slot está lleno y NO es el slot 0 (Default, que no se borra).
    const bool bCanEmpty = bUsed && SelectedIndex != 0;
    if (EmptySlotButton) EmptySlotButton->SetVisibility(bCanEmpty ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UPTLockerWidget::ActivateSelected()
{
    // Enter / botón Asignar: si el slot tiene algo → equipar; si está vacío → crear (editar nuevo).
    TArray<UPTLockerSlotWidget*>& List = ActiveList();
    if (!List.IsValidIndex(SelectedIndex) || !List[SelectedIndex]) return;
    const bool bHead = (ActiveTab == 0);
    APTLobbyPlayerController* PC = LobbyPC();
    if (!PC) return;
    if (List[SelectedIndex]->IsUsed())
    {
        if (bSkinMode)  PC->EquipSkinSlot(SelectedIndex);
        else if (bHead) PC->EquipHeadSlot(SelectedIndex);
        else            PC->EquipBodySlot(SelectedIndex);
        RefreshSlots();
    }
    else EditSelected(); // vacío → crear
}

void UPTLockerWidget::EditSelected()
{
    // Edita el slot SELECCIONADO (lo usa CreateSlotNow para crear un slot vacío recién clickeado).
    APTLobbyPlayerController* PC = LobbyPC();
    if (!PC) return;
    if (bSkinMode) { PC->EnterSkinEditForSlot(SelectedIndex); return; } // skin completa (secuencial cabeza→cuerpo)
    const bool bHead = (ActiveTab == 0);
    if (bHead) PC->EnterHeadSculptForSlot(SelectedIndex);
    else       PC->EnterBodyPaintForSlot(SelectedIndex);
}

void UPTLockerWidget::EditEquipped()
{
    // Editar SIEMPRE la skin EQUIPADA (no la que quedó bajo el hover ni la navegada). Un hover rápido
    // + click en Editar entraba a editar la skin equivocada / mostraba dos skins. Revertimos el
    // preview de hover primero (así queda aplicada la equipada) y editamos esa.
    if (bPreviewingHover) EndHoverPreview();

    UPTLockerSubsystem* L = Locker();
    APTLobbyPlayerController* PC = LobbyPC();
    if (!L || !PC) return;

    if (bSkinMode)
    {
        const int32 EqSkin = FMath::Max(0, L->GetEquippedSkin());
        SelectSlot(EqSkin, true);
        PC->EnterSkinEditForSlot(EqSkin); // editar la skin equipada (secuencial cabeza→cuerpo)
        return;
    }

    const bool bHead = (ActiveTab == 0);
    const int32 Equipped = bHead ? L->GetEquippedHead() : L->GetEquippedBody();
    if (Equipped < 0) return; // no hay skin equipada en esta pestaña → nada que editar

    SelectSlot(Equipped, bHead); // dejar la selección en la equipada (coherencia visual)
    if (bHead) PC->EnterHeadSculptForSlot(Equipped);
    else       PC->EnterBodyPaintForSlot(Equipped);
}

void UPTLockerWidget::OnSkinWorkshopClicked()
{
    // Abre el popup del Workshop de skins (se crea una vez y se reusa). Queda por encima del Locker.
    if (!SkinWorkshopClass) return;
    if (!SkinWorkshop)
        SkinWorkshop = CreateWidget<UPTSkinWorkshopWidget>(this, SkinWorkshopClass);
    if (SkinWorkshop)
    {
        if (!SkinWorkshop->IsInViewport()) SkinWorkshop->AddToViewport(60); // re-agregar tras cerrarlo
        SkinWorkshop->ShowPanel();
    }
}

void UPTLockerWidget::OnEmptySlotClicked()
{
    // Mostrar la confirmación solo si hay algo que vaciar y no es el Default (slot 0).
    TArray<UPTLockerSlotWidget*>& List = ActiveList();
    const bool bUsed = List.IsValidIndex(SelectedIndex) && List[SelectedIndex] && List[SelectedIndex]->IsUsed();
    if (!bUsed || SelectedIndex == 0) return;
    if (ClearConfirmPanel) ClearConfirmPanel->SetVisibility(ESlateVisibility::Visible);
}

void UPTLockerWidget::OnClearConfirmNo()
{
    if (ClearConfirmPanel) ClearConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTLockerWidget::OnClearConfirmYes()
{
    if (ClearConfirmPanel) ClearConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);
    UPTLockerSubsystem* L = Locker();
    if (!L) return;
    const int32 Idx = SelectedIndex;
    if (Idx == 0) return; // el Default (slot 0) no se borra

    // Una SKIN ocupa el MISMO índice en cabeza y cuerpo, así que vaciar un slot borra los DOS (la cabeza
    // Idx y el cuerpo Idx), sin importar en qué pestaña estés. Si alguno de los dos estaba equipado,
    // volver al Default (slot 0) para que el personaje no quede sin look.
    const int32 WasHeadEq = L->GetEquippedHead();
    const int32 WasBodyEq = L->GetEquippedBody();

    L->ClearHeadSlot(Idx);
    L->ClearBodySlot(Idx);

    if (APTLobbyPlayerController* PC = LobbyPC())
    {
        if (WasHeadEq == Idx) PC->EquipHeadSlot(0);
        if (WasBodyEq == Idx) PC->EquipBodySlot(0);
    }

    RefreshSlots(); // refresca miniaturas/selección/botones en ambas pestañas
}

// ── Botones ──
void UPTLockerWidget::OnHeadTabClicked() { SwitchTab(0); }
void UPTLockerWidget::OnBodyTabClicked() { SwitchTab(1); }
void UPTLockerWidget::OnAssignClicked()  { ActivateSelected(); }
void UPTLockerWidget::OnEditClicked()    { EditEquipped(); }
void UPTLockerWidget::OnBackClicked()
{
    if (APTLobbyPlayerController* PC = LobbyPC()) PC->CloseLocker();
    else RemoveFromParent();
}


// ── Teclado ──
FReply UPTLockerWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    // Durante la edición (cabeza/cuerpo) el Locker queda COLAPSADO pero puede conservar el foco de teclado
    // (sobre todo en la build empaquetada). Si procesara Escape acá, cerraría el Locker y "volvería al
    // menú" en vez de dejar que el PlayerController abra el popup de guardar/descartar. Colapsado/oculto =
    // no procesar teclas: que caigan al PlayerController.
    const ESlateVisibility Vis = GetVisibility();
    if (Vis == ESlateVisibility::Collapsed || Vis == ESlateVisibility::Hidden)
        return FReply::Unhandled();

    const FKey Key = InKeyEvent.GetKey();
    // En modo "un solo slot" no hay pestañas: Tab no hace nada (lo consumimos para que no mueva el foco).
    if (Key == EKeys::Tab)   { if (!bSkinMode) SwitchTab(ActiveTab == 0 ? 1 : 0); return FReply::Handled(); }
    if (Key == EKeys::Right) { MoveSelection(+1, 0); return FReply::Handled(); }
    if (Key == EKeys::Left)  { MoveSelection(-1, 0); return FReply::Handled(); }
    if (Key == EKeys::Down)  { MoveSelection(0, +1); return FReply::Handled(); }
    if (Key == EKeys::Up)    { MoveSelection(0, -1); return FReply::Handled(); }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar)       { ActivateSelected(); return FReply::Handled(); }
    if (Key == EKeys::G)                                     { EditEquipped(); return FReply::Handled(); }
    if (Key == EKeys::Escape || Key == EKeys::BackSpace)     { OnBackClicked(); return FReply::Handled(); }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
