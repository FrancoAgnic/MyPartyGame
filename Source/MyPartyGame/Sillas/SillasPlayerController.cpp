// Copyright Epic Games, Inc. All Rights Reserved.

#include "SillasPlayerController.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "UObject/ConstructorHelpers.h"

ASillasPlayerController::ASillasPlayerController()
{
    // Assets de input del template (en /Game/Input, NO /Game/ThirdPerson/Input).
    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCDefault(
        TEXT("/Game/Input/IMC_Default.IMC_Default"));
    if (IMCDefault.Succeeded()) DefaultMappingContexts.Add(IMCDefault.Object);

    static ConstructorHelpers::FObjectFinder<UInputMappingContext> IMCMouse(
        TEXT("/Game/Input/IMC_MouseLook.IMC_MouseLook"));
    if (IMCMouse.Succeeded()) DefaultMappingContexts.Add(IMCMouse.Object);
}

void ASillasPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController()) return;

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
            ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        for (int32 i = 0; i < DefaultMappingContexts.Num(); ++i)
        {
            if (DefaultMappingContexts[i])
            {
                Subsystem->AddMappingContext(DefaultMappingContexts[i], i);
            }
        }
    }

    AsegurarKitRuntime();

    // El menú deja el input en UIOnly; asegurar juego puro en la arena
    // (mismo criterio que PTLobbyPlayerController en el lobby).
    SetInputMode(FInputModeGameOnly());
    SetShowMouseCursor(false);
}

void ASillasPlayerController::AsegurarKitRuntime()
{
    if (!IsLocalController()) return;

    if (!KitIMC)
    {
        auto NuevaAccion = [this](const TCHAR* Nombre) -> UInputAction*
        {
            UInputAction* IA = NewObject<UInputAction>(this, Nombre);
            IA->ValueType = EInputActionValueType::Boolean;
            return IA;
        };
        IA_Captura   = NuevaAccion(TEXT("IA_Captura_Runtime"));
        IA_Sprint    = NuevaAccion(TEXT("IA_Sprint_Runtime"));
        IA_Empujon   = NuevaAccion(TEXT("IA_Empujon_Runtime"));
        IA_Habilidad = NuevaAccion(TEXT("IA_Habilidad_Runtime"));
        IA_Aguantar  = NuevaAccion(TEXT("IA_Aguantar_Runtime"));

        // Un solo contexto para todo el kit. Clic izquierdo dispara captura Y
        // empujón: cada pawn bindea solo la acción de su rol, la otra cae al vacío.
        KitIMC = NewObject<UInputMappingContext>(this, TEXT("IMC_KitSillas_Runtime"));
        KitIMC->MapKey(IA_Captura,   EKeys::LeftMouseButton);
        KitIMC->MapKey(IA_Empujon,   EKeys::LeftMouseButton);
        KitIMC->MapKey(IA_Sprint,    EKeys::LeftShift);
        KitIMC->MapKey(IA_Habilidad, EKeys::Q);
        KitIMC->MapKey(IA_Aguantar,  EKeys::LeftControl);
    }

    if (!bKitAgregado)
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
                ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
        {
            Subsystem->AddMappingContext(KitIMC, /*Priority=*/10);
            bKitAgregado = true;
        }
    }
}
