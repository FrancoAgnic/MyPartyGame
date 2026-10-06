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
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "GameFramework/Pawn.h"
#include "InputCoreTypes.h"

void APTSculptPlayerController::SetupGamepadInput()
{
    if (!InputComponent) return;

    // Esculpir (igual que el click izquierdo).
    InputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Pressed,  this, &APTSculptPlayerController::OnStampPressed);
    InputComponent->BindKey(EKeys::Gamepad_RightTrigger, IE_Released, this, &APTSculptPlayerController::OnStampReleased);

    // Tamaño (igual que la rueda; con la rueda de color abierta ajusta el brillo). Mantener = repetir.
    InputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Pressed,  this, &APTSculptPlayerController::OnPadBiggerPressed);
    InputComponent->BindKey(EKeys::Gamepad_RightShoulder, IE_Released, this, &APTSculptPlayerController::OnPadBiggerReleased);
    InputComponent->BindKey(EKeys::Gamepad_LeftShoulder,  IE_Pressed,  this, &APTSculptPlayerController::OnPadSmallerPressed);
    InputComponent->BindKey(EKeys::Gamepad_LeftShoulder,  IE_Released, this, &APTSculptPlayerController::OnPadSmallerReleased);

    // Herramientas en la cruceta (mismo orden que el hotbar 1-2-3-4, en sentido horario desde arriba).
    InputComponent->BindKey(EKeys::Gamepad_DPad_Up,    IE_Pressed, this, &APTSculptPlayerController::SetModeAdd);
    InputComponent->BindKey(EKeys::Gamepad_DPad_Right, IE_Pressed, this, &APTSculptPlayerController::SetModeErase);
    InputComponent->BindKey(EKeys::Gamepad_DPad_Down,  IE_Pressed, this, &APTSculptPlayerController::SetModePaint);
    InputComponent->BindKey(EKeys::Gamepad_DPad_Left,  IE_Pressed, this, &APTSculptPlayerController::SetModeEyes);

    // Menús radiales (mantener): X = color, Y = forma. El stick elige; soltar confirma.
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Pressed,  this, &APTSculptPlayerController::OnColorPickPressed);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Left, IE_Released, this, &APTSculptPlayerController::OnColorPickReleased);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Top,  IE_Pressed,  this, &APTSculptPlayerController::OnShapeRadialPressed);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Top,  IE_Released, this, &APTSculptPlayerController::OnShapeRadialReleased);

    // Vuelo: A sube, B baja (Espacio / Ctrl).
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Pressed,  this, &APTSculptPlayerController::OnPadAscendPressed);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Bottom, IE_Released, this, &APTSculptPlayerController::OnPadAscendReleased);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right,  IE_Pressed,  this, &APTSculptPlayerController::OnPadDescendPressed);
    InputComponent->BindKey(EKeys::Gamepad_FaceButton_Right,  IE_Released, this, &APTSculptPlayerController::OnPadDescendReleased);

    // LT (mantener): pegar el sello a la superficie (Alt). L3: plano vertical (Z). R3: rotar la forma.
    InputComponent->BindKey(EKeys::Gamepad_LeftTrigger,        IE_Pressed,  this, &APTSculptPlayerController::OnSurfaceSnapPressed);
    InputComponent->BindKey(EKeys::Gamepad_LeftTrigger,        IE_Released, this, &APTSculptPlayerController::OnSurfaceSnapReleased);
    InputComponent->BindKey(EKeys::Gamepad_LeftThumbstick,     IE_Pressed,  this, &APTSculptPlayerController::OnAxisVerticalPressed);
    InputComponent->BindKey(EKeys::Gamepad_LeftThumbstick,     IE_Released, this, &APTSculptPlayerController::OnAxisVerticalReleased);
    InputComponent->BindKey(EKeys::Gamepad_RightThumbstick,    IE_Pressed,  this, &APTSculptPlayerController::OnShapeRotatePressed);
    InputComponent->BindKey(EKeys::Gamepad_RightThumbstick,    IE_Released, this, &APTSculptPlayerController::OnShapeRotateReleased);

    // View: deshacer (toque) / borrar todo (mantener 3 s). Menu: pausa.
    InputComponent->BindKey(EKeys::Gamepad_Special_Left,  IE_Pressed,  this, &APTSculptPlayerController::OnClearAllPressed);
    InputComponent->BindKey(EKeys::Gamepad_Special_Left,  IE_Released, this, &APTSculptPlayerController::OnClearAllReleased);
    InputComponent->BindKey(EKeys::Gamepad_Special_Right, IE_Pressed,  this, &APTSculptPlayerController::OnPausePressed);
}

FVector2D APTSculptPlayerController::ReadStick(const FKey& X, const FKey& Y) const
{
    const FVector2D Raw(GetInputAnalogKeyState(X), GetInputAnalogKeyState(Y));
    const float Mag = Raw.Size();
    const float Dz = FMath::Clamp(GamepadDeadZone, 0.f, 0.9f);
    if (Mag <= Dz) return FVector2D::ZeroVector;
    // Zona muerta RADIAL reescalada: justo afuera de la zona muerta arranca en 0 (sin salto).
    return Raw / Mag * ((FMath::Min(Mag, 1.f) - Dz) / (1.f - Dz));
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

    // Cámara con el stick derecho (si no lo está usando un menú o la rotación).
    const bool bRightBusy = bRadial || bPicker || bRotatingShape;
    if (!bRightBusy && !R.IsZero() && !IsLookInputIgnored())
    {
        // Curva: más precisión cerca del centro, velocidad completa a fondo.
        const float Mag = FMath::Pow(FMath::Min(R.Size(), 1.f), FMath::Max(1.f, GamepadLookExponent));
        const FVector2D D = R.GetSafeNormal() * Mag;

        FRotator Rot = GetControlRotation();
        Rot.Yaw += D.X * GamepadLookYawSpeed * DeltaTime;
        float Pitch = FRotator::NormalizeAxis(Rot.Pitch) + D.Y * (bGamepadInvertY ? -1.f : 1.f) * GamepadLookPitchSpeed * DeltaTime;
        const float MinP = PlayerCameraManager ? PlayerCameraManager->ViewPitchMin : -89.f;
        const float MaxP = PlayerCameraManager ? PlayerCameraManager->ViewPitchMax :  89.f;
        Rot.Pitch = FMath::Clamp(Pitch, FMath::Max(MinP, -89.f), FMath::Min(MaxP, 89.f));
        SetControlRotation(Rot);
    }

    // Movimiento con el stick izquierdo (no con un menú abierto: ahí el stick apunta).
    if (!bRadial && !bPicker && !L.IsZero())
    {
        if (APawn* P = GetPawn())
        {
            const FRotator YawRot(0.f, GetControlRotation().Yaw, 0.f);
            P->AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), L.Y);
            P->AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), L.X);
        }
    }
}

void APTSculptPlayerController::CreateLocalPartyTV()
{
    if (!IsLocalController() || LocalPartyTV) return;
    const UPTGameInstance* GI = GetGameInstance<UPTGameInstance>();
    if (!GI || !GI->bLocalPartyMode) return;

    TSubclassOf<UPTLocalPartyTVWidget> Cls = LocalPartyTVClass;
    if (!Cls) Cls = UPTLocalPartyTVWidget::StaticClass();
    LocalPartyTV = CreateWidget<UPTLocalPartyTVWidget>(this, Cls);
    if (LocalPartyTV) LocalPartyTV->AddToViewport(15);
}
