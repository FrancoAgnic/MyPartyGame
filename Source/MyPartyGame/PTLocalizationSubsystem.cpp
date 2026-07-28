// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTLocalizationSubsystem.h"
#include "PTTextTable.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "UObject/UObjectIterator.h"

namespace { constexpr float SweepPeriod = 0.5f; }

void UPTLocalizationSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UPTLocalizationSubsystem::Tick), SweepPeriod);

    // Al cambiar de idioma no se espera al próximo barrido: se traduce en el acto.
    LanguageHandle = PTText::OnLanguageChanged().AddUObject(
        this, &UPTLocalizationSubsystem::LocalizeAllOnScreen);
}

void UPTLocalizationSubsystem::Deinitialize()
{
    if (TickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
        TickerHandle.Reset();
    }
    if (LanguageHandle.IsValid())
    {
        PTText::OnLanguageChanged().Remove(LanguageHandle);
        LanguageHandle.Reset();
    }
    Super::Deinitialize();
}

bool UPTLocalizationSubsystem::Tick(float /*DeltaTime*/)
{
    LocalizeAllOnScreen();
    return true; // seguir corriendo
}

void UPTLocalizationSubsystem::LocalizeAllOnScreen()
{
    const UWorld* MyWorld = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!MyWorld) return;

    for (TObjectIterator<UUserWidget> It; It; ++It)
    {
        UUserWidget* W = *It;
        // Solo lo que realmente se está mostrando en ESTE mundo: así no se toca nada del Editor
        // ni de otra instancia de PIE, y el barrido queda en unos pocos widgets.
        if (!IsValid(W) || W->GetWorld() != MyWorld || !W->IsInViewport()) continue;

        PTText::LocalizeWidgetTree(W); // idempotente: si ya está traducido no hace nada
    }
}
