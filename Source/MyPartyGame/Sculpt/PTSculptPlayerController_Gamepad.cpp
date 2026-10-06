// Joystick (gamepad) del escultor + overlay de la TV del modo local.
// Va en un archivo aparte para no engordar PTSculptPlayerController.cpp: son métodos del mismo
// controller, que reusan los handlers de teclado/mouse (OnStampPressed, OnScrollUp, ...) para que
// el joystick haga EXACTAMENTE lo mismo que el teclado.

#include "PTSculptPlayerController.h"
#include "PTSculptGameState.h"
#include "PTShapeRadialWidget.h"
#include "../UI/PTColorPickerWidget.h"
#include "../Lobby/PTLobbyCharacter.h"
#include "../PTGameInstance.h"
#include "../LocalParty/PTLocalPartyTVWidget.h"
#include "../LocalParty/PTLocalPartySettingsWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"
#include "../PTGamepad.h"

void APTSculptPlayerController::SetupGamepadInput()
{
    if (!InputComponent) return;
    // Los botones salen de PTGamepad (reasignables desde el panel "Joystick"); nada hardcodeado acá.
    const auto K = [](const TCHAR* Id) { return PTGamepad::GetKey(FName(Id)); };
    auto Bind = [this, &K](const TCHAR* Id, void (APTSculptPlayerController::*Pressed)(),
                           void (APTSculptPlayerController::*Released)())
    {
        const FKey Key = K(Id);
        if (!Key.IsValid()) return;
        if (Pressed)  InputComponent->BindKey(Key, IE_Pressed,  this, Pressed);
        if (Released) InputComponent->BindKey(Key, IE_Released, this, Released);
    };

    Bind(TEXT("Sculpt"),       &APTSculptPlayerController::OnStampPressed,        &APTSculptPlayerController::OnStampReleased);
    Bind(TEXT("BrushBigger"),  &APTSculptPlayerController::OnPadBiggerPressed,    &APTSculptPlayerController::OnPadBiggerReleased);
    Bind(TEXT("BrushSmaller"), &APTSculptPlayerController::OnPadSmallerPressed,   &APTSculptPlayerController::OnPadSmallerReleased);
    Bind(TEXT("ToolAdd"),      &APTSculptPlayerController::SetModeAdd,            nullptr);
    Bind(TEXT("ToolErase"),    &APTSculptPlayerController::SetModeErase,          nullptr);
    Bind(TEXT("ToolPaint"),    &APTSculptPlayerController::SetModePaint,          nullptr);
    Bind(TEXT("ToolEyes"),     &APTSculptPlayerController::SetModeEyes,           nullptr);
    Bind(TEXT("ColorPick"),    &APTSculptPlayerController::OnColorPickPressed,    &APTSculptPlayerController::OnColorPickReleased);
    Bind(TEXT("ShapeRadial"),  &APTSculptPlayerController::OnShapeRadialPressed,  &APTSculptPlayerController::OnShapeRadialReleased);
    Bind(TEXT("FlyUp"),        &APTSculptPlayerController::OnPadAscendPressed,    &APTSculptPlayerController::OnPadAscendReleased);
    Bind(TEXT("FlyDown"),      &APTSculptPlayerController::OnPadDescendPressed,   &APTSculptPlayerController::OnPadDescendReleased);
    Bind(TEXT("SurfaceSnap"),  &APTSculptPlayerController::OnSurfaceSnapPressed,  &APTSculptPlayerController::OnSurfaceSnapReleased);
    Bind(TEXT("AxisVertical"), &APTSculptPlayerController::OnAxisVerticalPressed, &APTSculptPlayerController::OnAxisVerticalReleased);
    Bind(TEXT("RotateShape"),  &APTSculptPlayerController::OnShapeRotatePressed,  &APTSculptPlayerController::OnShapeRotateReleased);
    Bind(TEXT("Undo"),         &APTSculptPlayerController::OnClearAllPressed,     &APTSculptPlayerController::OnClearAllReleased);
    Bind(TEXT("Pause"),        &APTSculptPlayerController::OnPausePressed,        nullptr);
}

void APTSculptPlayerController::RebuildGamepadInput()
{
    if (!InputComponent) return;
    // Sacar todos los bindings de botones de joystick y volver a armarlos con la tabla actual.
    InputComponent->KeyBindings.RemoveAll([](const FInputKeyBinding& B) { return B.Chord.Key.IsGamepadKey(); });
    SetupGamepadInput();
}

FVector2D APTSculptPlayerController::ReadStick(const FKey& X, const FKey& Y) const
{
    return PTGamepad::ReadStick(this, X, Y);
}

void APTSculptPlayerController::OnPadBiggerPressed()
{
    bUsingGamepad = true;
    // Con el radial de formas abierto, LB/RB cambian de página (como la rueda).
    if (bShapeRadialActive && ShapeRadial) { ShapeRadial->NextPage(); return; }
    OnScrollUp();
    bPadBigger = true;
    bPadSmaller = false;
    PadRepeatTimer = GamepadRepeatDelay;
}

void APTSculptPlayerController::OnPadSmallerPressed()
{
    bUsingGamepad = true;
    if (bShapeRadialActive && ShapeRadial) { ShapeRadial->PrevPage(); return; }
    OnScrollDown();
    bPadSmaller = true;
    bPadBigger = false;
    PadRepeatTimer = GamepadRepeatDelay;
}

void APTSculptPlayerController::OnPadAscendPressed()
{
    if (APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn())) C->SetGamepadAscend(true);
}

void APTSculptPlayerController::OnPadAscendReleased()
{
    if (APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn())) C->SetGamepadAscend(false);
}

void APTSculptPlayerController::OnPadDescendPressed()
{
    if (APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn())) C->SetGamepadDescend(true);
}

void APTSculptPlayerController::OnPadDescendReleased()
{
    if (APTLobbyCharacter* C = Cast<APTLobbyCharacter>(GetPawn())) C->SetGamepadDescend(false);
}

void APTSculptPlayerController::TickGamepad(float DeltaTime)
{
    if (!IsLocalController()) return;

    const FVector2D L = ReadStick(EKeys::Gamepad_LeftX,  EKeys::Gamepad_LeftY);
    const FVector2D R = ReadStick(EKeys::Gamepad_RightX, EKeys::Gamepad_RightY);

    // ¿Quién está mandando: joystick o mouse? El cursor virtual de los menús solo se mueve con joystick
    // (si no, le pisaría el mouse a quien juega con teclado).
    float MouseDX = 0.f, MouseDY = 0.f;
    GetInputMouseDelta(MouseDX, MouseDY);
    if (!L.IsZero() || !R.IsZero()) bUsingGamepad = true;
    else if (FMath::Abs(MouseDX) + FMath::Abs(MouseDY) > 0.5f) bUsingGamepad = false;

    // LB/RB mantenidos: repetir el cambio de tamaño.
    if (bPadBigger || bPadSmaller)
    {
        PadRepeatTimer -= DeltaTime;
        if (PadRepeatTimer <= 0.f)
        {
            if (bPadBigger) OnScrollUp(); else OnScrollDown();
            PadRepeatTimer = FMath::Max(0.02f, GamepadRepeatInterval);
        }
    }

    const bool bRadial = bShapeRadialActive && ShapeRadial;
    const bool bPicker = bQuickColorActive && ColorPicker;

    // Menús radiales: cualquiera de los dos sticks apunta (el que esté más inclinado). Soltar el stick
    // deja la selección donde quedó, así se puede soltar el botón con calma.
    if ((bRadial || bPicker) && bUsingGamepad)
    {
        const FVector2D S = (R.SizeSquared() >= L.SizeSquared()) ? R : L;
        if (!S.IsZero())
        {
            if (bRadial)
            {
                int32 VX = 0, VY = 0;
                GetViewportSize(VX, VY);
                if (VX > 0 && VY > 0)
                {
                    const float Rad = VY * GamepadRadialRadius;
                    const FVector2D Dir = S.GetSafeNormal();
                    SetMouseLocation(FMath::RoundToInt(VX * 0.5f + Dir.X * Rad), FMath::RoundToInt(VY * 0.5f - Dir.Y * Rad));
                }
            }
            else if (UPTColorPickerWidget* CP = Cast<UPTColorPickerWidget>(ColorPicker))
            {
                CP->SetCursorFromStick(S);
            }
        }
    }

    // R3 mantenido: el stick derecho rota la forma (como arrastrar con la rueda apretada).
    if (bRotatingShape && !R.IsZero())
    {
        StampRotation.Yaw   += R.X * GamepadRotateSpeed * DeltaTime;
        StampRotation.Pitch -= R.Y * GamepadRotateSpeed * DeltaTime;
        StampRotation.Normalize();
    }

    // Cámara (stick der.) y movimiento (stick izq.), con la sensibilidad / invertir Y del jugador.
    // Con un menú radial abierto los sticks apuntan, y con R3 el derecho rota la forma.
    const bool bRightBusy = bRadial || bPicker || bRotatingShape;
    PTGamepad::TickMoveLook(this, DeltaTime, /*bAllowMove=*/!bRadial && !bPicker, /*bAllowLook=*/!bRightBusy,
                            GamepadLookYawSpeed, GamepadLookPitchSpeed, GamepadLookExponent, bGamepadInvertY);
}

void APTSculptPlayerController::CreateLocalPartyTV()
{
    if (!IsLocalController() || LocalPartyTV) return;
    const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    if (!GI || !GI->bLocalPartyMode) return;

    TSubclassOf<UPTLocalPartyTVWidget> Cls = LocalPartyTVClass;
    if (!Cls) Cls = UPTLocalPartyTVWidget::StaticClass();
    LocalPartyTV = CreateWidget<UPTLocalPartyTVWidget>(this, Cls);
    if (LocalPartyTV) LocalPartyTV->AddToViewport(1); // bajo: el menú de pausa y los popups van arriba
    // Panel de configuración de la partida (visible solo en la espera; se oculta solo).
    LocalPartySettings = CreateWidget<UPTLocalPartySettingsWidget>(this, UPTLocalPartySettingsWidget::StaticClass());
    if (LocalPartySettings) LocalPartySettings->AddToViewport(2);
}
