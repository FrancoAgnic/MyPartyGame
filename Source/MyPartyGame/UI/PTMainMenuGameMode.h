// Copyright Epic Games, Inc. All Rights Reserved.
// Fase 2 — GameMode del mapa MainMenu. Sin lógica de red; solo garantiza un entorno limpio de menú.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PTMainMenuGameMode.generated.h"

UCLASS()
class MYPARTYGAME_API APTMainMenuGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    APTMainMenuGameMode();
};
