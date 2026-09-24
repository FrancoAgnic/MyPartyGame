// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLobbyEscapeMenuWidget.h"
#include "PTSettingsWidget.h"
#include "PTLobbyGameMode.h"
#include "../PTGameInstance.h"
#include "../UI/PTLoadingScreenWidget.h"
#include "TimerManager.h"
#include "../Mods/PTMapAuthorGameMode.h"
#include "../Mods/PTMapEnvironment.h"
#include "../UI/PTSaveMapWidget.h"
#include "Engine/World.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "../PTTextTable.h"

bool UPTLobbyEscapeMenuWidget::Initialize()
{
    if (!Super::Initialize()) return false;

    if (LeaveGameButton) LeaveGameButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnLeaveGameClicked);
    if (SettingsButton)  SettingsButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnSettingsClicked);
    if (ResumeButton)    ResumeButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnResumeClicked);
    if (SaveMapButton)   SaveMapButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnSaveMapClicked);
    if (DiscardDontSaveButton) DiscardDontSaveButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnDiscardDontSave);
    if (DiscardSaveButton)     DiscardSaveButton->OnClicked.AddDynamic(this, &UPTLobbyEscapeMenuWidget::OnDiscardSave);
    if (DiscardPopup)          DiscardPopup->SetVisibility(ESlateVisibility::Collapsed);

    SetVisibility(ESlateVisibility::Collapsed);
    return true;
}

bool UPTLobbyEscapeMenuWidget::IsMenuOpen() const
{
    return GetVisibility() != ESlateVisibility::Collapsed;
}

void UPTLobbyEscapeMenuWidget::ToggleMenu()
{
    const bool bOpen = !IsMenuOpen();
    SetVisibility(bOpen ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    // "Guardar mapa" solo tiene sentido en el modo autoría de mapa.
    if (SaveMapButton)
        SaveMapButton->SetVisibility(IsMapAuthorMode() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    // En el MAIN MENU no hay partida que abandonar → el botón dice "Exit Game"/"Cerrar juego" (cierra el
    // juego). En partida/lobby/autoría dice "Leave Game". El label se toma de LeaveGameLabel si lo bindeaste;
    // si no, se busca el primer TextBlock DENTRO del botón (así funciona sin tocar el WBP).
    if (bOpen)
    {
        UTextBlock* Label = LeaveGameLabel;
        if (!Label && LeaveGameButton && LeaveGameButton->GetChildrenCount() > 0)
            Label = Cast<UTextBlock>(LeaveGameButton->GetChildAt(0));
        if (Label)
            Label->SetText(PTText::Get(IsMainMenuLevel() ? FName("UI_EXIT_GAME") : FName("UI_LEAVE_GAME")));
    }

    if (APlayerController* PC = GetOwningPlayer())
    {
        if (bOpen)
        {
            FInputModeGameAndUI InputMode;
            InputMode.SetWidgetToFocus(TakeWidget());
            InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(InputMode);
            PC->SetShowMouseCursor(true);
        }
        else
        {
            PC->SetInputMode(FInputModeGameOnly());
            PC->SetShowMouseCursor(false);
        }
    }
}

void UPTLobbyEscapeMenuWidget::HandleEscape()
{
    // Sub-popups de autoría abiertos: ESC los cierra a ELLOS primero (no togglea el menú, así no se
    // reactiva el input de juego con el popup abierto).
    if (SaveMapForm && SaveMapForm->GetVisibility() != ESlateVisibility::Collapsed)
    {
        SaveMapForm->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    if (DiscardPopup && DiscardPopup->GetVisibility() != ESlateVisibility::Collapsed)
    {
        DiscardPopup->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // Si el settings está abierto → volver atrás (cerrar settings, seguir en el menú).
    if (SettingsPanel && SettingsPanel->GetVisibility() != ESlateVisibility::Collapsed)
    {
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
        return;
    }
    // Si no, alternar el menú completo. Al abrir, asegurar el settings oculto.
    ToggleMenu();
    if (IsMenuOpen() && SettingsPanel)
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
}

void UPTLobbyEscapeMenuWidget::OnLeaveGameClicked()
{
    // En el MAIN MENU no hay partida que abandonar: el botón funciona como "Exit Game" → cerrar el juego.
    if (IsMainMenuLevel())
    {
        UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, /*bIgnorePlatformRestrictions=*/false);
        return;
    }

    // En modo AUTORÍA de mapa: preguntar antes de salir (podés perder cambios sin guardar).
    if (IsMapAuthorMode() && DiscardPopup)
    {
        DiscardPopup->SetVisibility(ESlateVisibility::Visible);
        return;
    }
    DoLeaveGame();
}

void UPTLobbyEscapeMenuWidget::DoLeaveGame()
{
    if (bLeaving) return;

    // Transición: mostrar la pantalla de carga (AnimIn) y hacer la salida REAL recién cuando ya tapó la
    // pantalla → así se ve AnimIn ANTES del cambio de nivel (no una pantalla negra de golpe). Con fallback:
    // si no hay clase de loading, sale directo como antes.
    UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    if (GI && GI->LoadingScreenClass)
    {
        if (UPTLoadingScreenWidget* LS = GI->CreateLoadingScreen(/*bStartAtLoop=*/false))
        {
            GI->bTransitionCovering = true; // el MainMenu arranca tapado y revela con OUT al llegar
            LS->OnCovered.AddDynamic(this, &UPTLobbyEscapeMenuWidget::DoLeaveGameNow);
            // Seguridad: si el AnimIn no avisa (anim faltante), salir igual tras 2s.
            if (UWorld* W = GetWorld())
                W->GetTimerManager().SetTimer(LeaveSafetyTimer, this, &UPTLobbyEscapeMenuWidget::DoLeaveGameNow, 2.0f, false);
            return;
        }
    }
    DoLeaveGameNow();
}

void UPTLobbyEscapeMenuWidget::DoLeaveGameNow()
{
    if (bLeaving) return; // OnCovered + el timer de seguridad podrían llamar los dos
    bLeaving = true;
    if (UWorld* W = GetWorld()) W->GetTimerManager().ClearTimer(LeaveSafetyTimer);

    UWorld* World = GetWorld();

    // Si el que se va es el ANFITRIÓN, no se puede cerrar el mundo de una: eso deja a los clientes
    // con la conexión muerta y se les cae el juego. El GameMode los saca primero y después cierra.
    if (World && World->GetNetMode() == NM_ListenServer)
    {
        if (APTLobbyGameMode* GM = World->GetAuthGameMode<APTLobbyGameMode>())
        {
            GM->HostLeaveGame();
            return;
        }
    }

    // Cliente / autoría / partida local: OpenLevel desconecta del server; en el servidor eso dispara
    // APTLobbyGameMode::Logout, que ya limpia la sesión si era el último jugador.
    UGameplayStatics::OpenLevel(this, FName("MainMenu"));
}

void UPTLobbyEscapeMenuWidget::OnDiscardDontSave()
{
    // "No guardar": salir directo (se pierden los cambios sin guardar).
    if (DiscardPopup) DiscardPopup->SetVisibility(ESlateVisibility::Collapsed);
    DoLeaveGame();
}

void UPTLobbyEscapeMenuWidget::OnDiscardSave()
{
    // "Guardar": cerrar el aviso y abrir el formulario de Guardar (título/desc/miniatura).
    if (DiscardPopup) DiscardPopup->SetVisibility(ESlateVisibility::Collapsed);
    OnSaveMapClicked();
}

void UPTLobbyEscapeMenuWidget::OnSettingsClicked()
{
    if (SettingsPanel) SettingsPanel->ShowPanel();
}

void UPTLobbyEscapeMenuWidget::OnResumeClicked()
{
    ToggleMenu();
}

bool UPTLobbyEscapeMenuWidget::IsMapAuthorMode() const
{
    return GetWorld() && Cast<APTMapAuthorGameMode>(GetWorld()->GetAuthGameMode()) != nullptr;
}

bool UPTLobbyEscapeMenuWidget::IsMainMenuLevel() const
{
    UWorld* W = GetWorld();
    if (!W) return false;
    // GetMapName trae el nombre corto; en PIE lleva el prefijo (UEDPIE_0_) → sacarlo antes de comparar.
    FString Map = W->GetMapName();
    Map.RemoveFromStart(W->StreamingLevelsPrefix);
    return Map.Equals(TEXT("MainMenu"), ESearchCase::IgnoreCase);
}

void UPTLobbyEscapeMenuWidget::OnSaveMapClicked()
{
    // Abre el formulario de Guardar (título/desc/miniatura autocompletados, editables). Al confirmar ahí
    // se guarda el escenario + los metadatos.
    if (SaveMapClass)
    {
        if (!SaveMapForm)
        {
            SaveMapForm = CreateWidget<UPTSaveMapWidget>(this, SaveMapClass);
            if (SaveMapForm) SaveMapForm->AddToViewport(80);
        }
        if (SaveMapForm) { SaveMapForm->ShowPanel(); return; }
    }

    // Fallback (sin WBP de formulario): guardar directo el mapa (props) sin metadatos.
    UWorld* W = GetWorld();
    UPTGameInstance* GI = W ? Cast<UPTGameInstance>(W->GetGameInstance()) : nullptr;
    APTMapEnvironment* Env = W ? Cast<APTMapEnvironment>(
        UGameplayStatics::GetActorOfClass(W, APTMapEnvironment::StaticClass())) : nullptr;
    if (!GI || !Env) return;
    TArray<uint8> Blob;
    Env->SerializeEnvironment(Blob);
    GI->SaveAuthoredMap(Blob);
}
