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

    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    const bool bNeedPick = !(S && S->HasChosenLanguage());

    if (bNeedPick && LanguageSelectPanel)
    {
        // PRIMER arranque: primero el idioma. Al confirmar arranca la secuencia del título.
        LanguageSelectPanel->OnLanguageChosen.AddUObject(this, &UPTBootWidget::StartTitleSequence);
        LanguageSelectPanel->Refresh();
        LanguageSelectPanel->SetVisibility(ESlateVisibility::Visible);
    }
    else
    {
        // Ya eligió idioma antes → directo al título.
        StartTitleSequence();
    }
}

void UPTBootWidget::StartTitleSequence()
{
    if (LanguageSelectPanel)
        LanguageSelectPanel->SetVisibility(ESlateVisibility::Collapsed);

    if (TitleAnim)
    {
        // El borrado del título está keyframeado DENTRO de TitleAnim; al terminar → menú.
        FWidgetAnimationDynamicEvent Ev;
        Ev.BindUFunction(this, FName("OnTitleFinished"));
        BindToAnimationFinished(TitleAnim, Ev);
        PlayAnimation(TitleAnim);
    }
    else
    {
        // Sin animación: esperar unos segundos y continuar.
        if (UWorld* W = GetWorld())
            W->GetTimerManager().SetTimer(TitleTimer, this, &UPTBootWidget::OnTitleFinished,
                                          FMath::Max(FallbackTitleSeconds, 0.1f), false);
    }
}

void UPTBootWidget::OnTitleFinished()
{
    GoToMainMenu();
}

void UPTBootWidget::GoToMainMenu()
{
    // Carga el lobby/MainMenu. Su widget reproduce su animación de entrada (SpawnMainMenu).
    UGameplayStatics::OpenLevel(this, FName(*MainMenuMap));
}
