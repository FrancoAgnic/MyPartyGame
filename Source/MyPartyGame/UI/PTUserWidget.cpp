// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTUserWidget.h"
#include "../PTGameInstance.h"

void UPTUserWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Aplicar los sonidos de UI (definidos en el GameInstance) a todos los botones de este widget.
    // Cada widget que deriva de esta base lo hace solo → no hay que tocar botón por botón, y los
    // widgets creados dinámicamente (filas de sesión, slots, etc.) también quedan cubiertos.
    if (UPTGameInstance* GI = Cast<UPTGameInstance>(GetGameInstance()))
        GI->ApplyUIButtonSounds(this);
}
