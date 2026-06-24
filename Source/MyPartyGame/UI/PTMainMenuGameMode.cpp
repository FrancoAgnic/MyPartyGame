// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTMainMenuGameMode.h"

APTMainMenuGameMode::APTMainMenuGameMode()
{
    // Sin pawn por defecto en el menú; la cámara y el input se manejan por UI.
    DefaultPawnClass = nullptr;
}
