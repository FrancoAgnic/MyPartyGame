// Copyright Epic Games, Inc. All Rights Reserved.
// Ajustes del Locker editables desde el editor: Project Settings → Game → Locker.
// El valor queda guardado en Config/DefaultGame.ini, así que se puede cambiar SIN recompilar.

#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PTLockerSettings.generated.h"

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Locker"))
class MYPARTYGAME_API UPTLockerSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Game"); }

    // Máximo de skins del casillero (cabezas = cuerpos). Editable en Project Settings.
    UPROPERTY(Config, EditAnywhere, Category = "Locker", meta = (ClampMin = "1", UIMin = "1", UIMax = "60"))
    int32 MaxSkinSlots = 22;

    // Acceso rápido y seguro al valor (>= 1).
    static int32 GetMaxSkinSlots()
    {
        const UPTLockerSettings* S = GetDefault<UPTLockerSettings>();
        return S ? FMath::Max(1, S->MaxSkinSlots) : 22;
    }
};
