// Copyright Epic Games, Inc. All Rights Reserved.
// GameMode del level de arranque ("boot"): al empezar crea el widget del boot (logo + idioma) y
// deja el input en la UI con el cursor visible. Asignar en el World Settings del level Boot.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PTBootGameMode.generated.h"

UCLASS()
class MYPARTYGAME_API APTBootGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    APTBootGameMode();

protected:
    virtual void BeginPlay() override;

    // WBP del boot (reparentado a UPTBootWidget). Asignar en BP_BootGameMode.
    UPROPERTY(EditDefaultsOnly, Category = "Boot")
    TSubclassOf<class UUserWidget> BootWidgetClass;
};
