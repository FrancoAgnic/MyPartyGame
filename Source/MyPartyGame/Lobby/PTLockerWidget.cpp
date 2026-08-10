// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLockerWidget.h"
#include "PTLockerSlotWidget.h"
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

void UPTLockerWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (HeadTabButton)    HeadTabButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnHeadTabClicked);
    if (BodyTabButton)    BodyTabButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnBodyTabClicked);
    if (AssignButton)     AssignButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnAssignClicked);
    if (EditActionButton) EditActionButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnEditClicked);
    if (BackButton)       BackButton->OnClicked.AddDynamic(this, &UPTLockerWidget::OnBackClicked);
    // Equipar ahora es con click directo sobre el slot → ocultamos el botón Equipar (queda Editar).
    if (AssignButton)     AssignButton->SetVisibility(ESlateVisibility::Collapsed);
    BuildSlots();
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
    return (ActiveTab == 0) ? HeadSlotWidgets : BodySlotWidgets;
}
int32 UPTLockerWidget::ActiveCount() const
{
    return (ActiveTab == 0) ? HeadSlotWidgets.Num() : BodySlotWidgets.Num();
}

void UPTLockerWidget::BuildSlots()
{
    if (bBuilt || !SlotWidgetClass) return;
    bBuilt = true;
    auto Make = [this](UPanelWidget* Box, int32 Count, bool bHead, TArray<UPTLockerSlotWidget*>& Out)
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
    Make(HeadSlotsBox, UPTLockerSaveGame::NumHeadSlots, true,  HeadSlotWidgets);
    Make(BodySlotsBox, UPTLockerSaveGame::NumBodySlots, false, BodySlotWidgets);
}

void UPTLockerWidget::RefreshSlots()
{
    UPTLockerSubsystem* L = Locker();
    if (!L) return;
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
    if (bHead != (ActiveTab == 0)) SwitchTab(bHead ? 0 : 1);
    SelectedIndex = FMath::Clamp(Index, 0, FMath::Max(0, ActiveCount() - 1));
    ApplySelectionVisual();
}

void UPTLockerWidget::HoverSlot(int32 Index, bool bHead)
{
    // Solo previsualizar slots LLENOS (los vacíos no cambian nada).
    TArray<UPTLockerSlotWidget*>& List = (bHead ? HeadSlotWidgets : BodySlotWidgets);
    if (!List.IsValidIndex(Index) || !List[Index] || !List[Index]->IsUsed()) return;
    SelectSlot(Index, bHead); // que Editar apunte al slot bajo el mouse mientras lo estás mirando
    if (APTLobbyPlayerController* PC = LobbyPC()) PC->PreviewLookSlot(Index, bHead);
    bPreviewingHover = true;
}

void UPTLockerWidget::EndHoverPreview()
{
    if (APTLobbyPlayerController* PC = LobbyPC()) PC->RevertLookPreview(); // personaje vuelve a lo equipado
    // La SELECCIÓN también vuelve al equipado → Editar solo edita el slot que tenés puesto.
    if (UPTLockerSubsystem* L = Locker())
        SelectSlot(ActiveTab == 0 ? FMath::Max(0, L->GetEquippedHead()) : FMath::Max(0, L->GetEquippedBody()), ActiveTab == 0);
    bPreviewingHover = false;
}

void UPTLockerWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
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
    TArray<UPTLockerSlotWidget*>& List = (bHead ? HeadSlotWidgets : BodySlotWidgets);
    if (!List.IsValidIndex(Index) || !List[Index] || !List[Index]->IsUsed()) return; // vacío: no equipa
    SelectSlot(Index, bHead);
    if (APTLobbyPlayerController* PC = LobbyPC())
    {
        if (bHead) PC->EquipHeadSlot(Index); else PC->EquipBodySlot(Index);
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
    TArray<UPTLockerSlotWidget*>& List = ActiveList();
    for (int32 i = 0; i < List.Num(); ++i)
        if (List[i]) List[i]->SetSelected(i == SelectedIndex);

    // Botón Asignar dice "Asignar" si el slot tiene algo, o "Crear" si está vacío.
    const bool bUsed = List.IsValidIndex(SelectedIndex) && List[SelectedIndex] && List[SelectedIndex]->IsUsed();
    if (AssignLabel) AssignLabel->SetText(PTText::Get(bUsed ? TEXT("LOCKER_EQUIP") : TEXT("LOCKER_CREATE")));
    // Editar solo tiene sentido si el slot está lleno.
    if (EditActionButton) EditActionButton->SetVisibility(bUsed ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
        if (bHead) PC->EquipHeadSlot(SelectedIndex); else PC->EquipBodySlot(SelectedIndex);
        RefreshSlots();
    }
    else EditSelected(); // vacío → crear
}

void UPTLockerWidget::EditSelected()
{
    // G / botón Editar: entra a editar (o crear si está vacío) el slot seleccionado.
    const bool bHead = (ActiveTab == 0);
    if (APTLobbyPlayerController* PC = LobbyPC())
    {
        if (bHead) PC->EnterHeadSculptForSlot(SelectedIndex);
        else       PC->EnterBodyPaintForSlot(SelectedIndex);
    }
}

// ── Botones ──
void UPTLockerWidget::OnHeadTabClicked() { SwitchTab(0); }
void UPTLockerWidget::OnBodyTabClicked() { SwitchTab(1); }
void UPTLockerWidget::OnAssignClicked()  { ActivateSelected(); }
void UPTLockerWidget::OnEditClicked()    { EditSelected(); }
void UPTLockerWidget::OnBackClicked()
{
    if (APTLobbyPlayerController* PC = LobbyPC()) PC->CloseLocker();
    else RemoveFromParent();
}

// ── Teclado ──
FReply UPTLockerWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    const FKey Key = InKeyEvent.GetKey();
    if (Key == EKeys::Tab)   { SwitchTab(ActiveTab == 0 ? 1 : 0); return FReply::Handled(); }
    if (Key == EKeys::Right) { MoveSelection(+1, 0); return FReply::Handled(); }
    if (Key == EKeys::Left)  { MoveSelection(-1, 0); return FReply::Handled(); }
    if (Key == EKeys::Down)  { MoveSelection(0, +1); return FReply::Handled(); }
    if (Key == EKeys::Up)    { MoveSelection(0, -1); return FReply::Handled(); }
    if (Key == EKeys::Enter || Key == EKeys::SpaceBar)       { ActivateSelected(); return FReply::Handled(); }
    if (Key == EKeys::G)                                     { EditSelected(); return FReply::Handled(); }
    if (Key == EKeys::Escape || Key == EKeys::BackSpace)     { OnBackClicked(); return FReply::Handled(); }
    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}
