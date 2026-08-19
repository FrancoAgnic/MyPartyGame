// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTBootWidget.h"
#include "PTLanguageSelectWidget.h"
#include "../PTGameUserSettings.h"
#include "Animation/WidgetAnimation.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

void UPTBootWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (LanguageSelectPanel)
        LanguageSelectPanel->SetVisibility(ESlateVisibility::Collapsed);

    if (LogoAnim)
    {
        // Al terminar la animación del logo → OnLogoFinished.
        FWidgetAnimationDynamicEvent Ev;
        Ev.BindUFunction(this, FName("OnLogoFinished"));
        BindToAnimationFinished(LogoAnim, Ev);
        PlayAnimation(LogoAnim);
    }
    else
    {
        // Sin animación: esperar un rato mostrando el logo estático y seguir igual.
        if (UWorld* W = GetWorld())
            W->GetTimerManager().SetTimer(LogoTimer, this, &UPTBootWidget::OnLogoFinished,
                                          FMath::Max(FallbackLogoSeconds, 0.1f), false);
    }
}

void UPTBootWidget::OnLogoFinished()
{
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    const bool bNeedPick = !(S && S->HasChosenLanguage());

    // Primer arranque → mostrar la selección de idioma en este mismo level; al confirmar, al menú.
    if (bNeedPick && LanguageSelectPanel)
    {
        LanguageSelectPanel->OnLanguageChosen.AddUObject(this, &UPTBootWidget::OnLanguageChosen);
        LanguageSelectPanel->Refresh();
        LanguageSelectPanel->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        GoToMainMenu();
    }
}

void UPTBootWidget::OnLanguageChosen()
{
    GoToMainMenu();
}

void UPTBootWidget::GoToMainMenu()
{
    UGameplayStatics::OpenLevel(this, FName(*MainMenuMap));
}
