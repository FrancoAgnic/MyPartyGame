#include "PTGamepad.h"
#include "PTGameUserSettings.h"
#include "PTTextTable.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
    TArray<FPTGamepadAction> GActions;
    bool bBuilt = false;

    void BuildDefaults()
    {
        GActions.Reset();
        auto Add = [](const TCHAR* Id, const TCHAR* Label, const FKey& K)
        {
            FPTGamepadAction A;
            A.Id = FName(Id);
            A.LabelKey = FName(Label);
            A.DefaultKey = K;
            A.Key = K;
            GActions.Add(A);
        };
        Add(TEXT("Sculpt"),        TEXT("GPA_SCULPT"),       EKeys::Gamepad_RightTrigger);
        Add(TEXT("BrushSmaller"),  TEXT("GPA_SMALLER"),      EKeys::Gamepad_LeftShoulder);
        Add(TEXT("BrushBigger"),   TEXT("GPA_BIGGER"),       EKeys::Gamepad_RightShoulder);
        Add(TEXT("ToolAdd"),       TEXT("GPA_TOOL_ADD"),     EKeys::Gamepad_DPad_Up);
        Add(TEXT("ToolErase"),     TEXT("GPA_TOOL_ERASE"),   EKeys::Gamepad_DPad_Right);
        Add(TEXT("ToolPaint"),     TEXT("GPA_TOOL_PAINT"),   EKeys::Gamepad_DPad_Down);
        Add(TEXT("ToolEyes"),      TEXT("GPA_TOOL_EYES"),    EKeys::Gamepad_DPad_Left);
        Add(TEXT("ColorPick"),     TEXT("GPA_COLOR"),        EKeys::Gamepad_FaceButton_Left);
        Add(TEXT("ShapeRadial"),   TEXT("GPA_SHAPE"),        EKeys::Gamepad_FaceButton_Top);
        Add(TEXT("FlyUp"),         TEXT("GPA_FLY_UP"),       EKeys::Gamepad_FaceButton_Bottom);
        Add(TEXT("FlyDown"),       TEXT("GPA_FLY_DOWN"),     EKeys::Gamepad_FaceButton_Right);
        Add(TEXT("SurfaceSnap"),   TEXT("GPA_SURFACE"),      EKeys::Gamepad_LeftTrigger);
        Add(TEXT("AxisVertical"),  TEXT("GPA_AXIS"),         EKeys::Gamepad_LeftThumbstick);
        Add(TEXT("RotateShape"),   TEXT("GPA_ROTATE"),       EKeys::Gamepad_RightThumbstick);
        Add(TEXT("Undo"),          TEXT("GPA_UNDO"),         EKeys::Gamepad_Special_Left);
        Add(TEXT("Pause"),         TEXT("GPA_PAUSE"),        EKeys::Gamepad_Special_Right);
    }

    void EnsureBuilt()
    {
        if (bBuilt) return;
        bBuilt = true;
        BuildDefaults();
        if (const UPTGameUserSettings* S = UPTGameUserSettings::Get())
            for (FPTGamepadAction& A : GActions)
                if (const FString* Name = S->GetGamepadOverrides().Find(A.Id))
                {
                    const FKey K(**Name);
                    if (K.IsValid() && PTGamepad::IsAssignableKey(K)) A.Key = K;
                }
    }

    void Save()
    {
        UPTGameUserSettings* S = UPTGameUserSettings::Get();
        if (!S) return;
        S->ClearGamepadOverrides();
        for (const FPTGamepadAction& A : GActions)
            if (A.Key != A.DefaultKey) S->SetGamepadOverride(A.Id, A.Key.ToString());
        S->SaveSettings();
    }
}

const TArray<FPTGamepadAction>& PTGamepad::GetActions()
{
    EnsureBuilt();
    return GActions;
}

FKey PTGamepad::GetKey(FName Id)
{
    EnsureBuilt();
    for (const FPTGamepadAction& A : GActions) if (A.Id == Id) return A.Key;
    return EKeys::Invalid;
}

void PTGamepad::SetKey(FName Id, const FKey& NewKey)
{
    EnsureBuilt();
    if (!IsAssignableKey(NewKey)) return;
    FPTGamepadAction* Target = GActions.FindByPredicate([Id](const FPTGamepadAction& A) { return A.Id == Id; });
    if (!Target) return;
    // Si otra acción ya usaba ese botón, se queda con el viejo de esta (intercambio, nunca dos en uno).
    for (FPTGamepadAction& A : GActions)
        if (&A != Target && A.Key == NewKey) A.Key = Target->Key;
    Target->Key = NewKey;
    Save();
}

void PTGamepad::ResetToDefaults()
{
    EnsureBuilt();
    for (FPTGamepadAction& A : GActions) A.Key = A.DefaultKey;
    Save();
}

void PTGamepad::RefreshFromSettings()
{
    bBuilt = false;
    EnsureBuilt();
}

bool PTGamepad::IsAssignableKey(const FKey& K)
{
    if (!K.IsValid() || !K.IsGamepadKey() || K.IsAxis1D() || K.IsAxis2D()) return false;
    // Las direcciones "virtuales" de los sticks (Gamepad_LeftStick_Up...) los usa el movimiento.
    const FString N = K.GetFName().ToString();
    return !N.Contains(TEXT("Stick_"));
}

FString PTGamepad::KeyLabel(const FKey& K)
{
    if (K == EKeys::Gamepad_FaceButton_Bottom) return TEXT("A");
    if (K == EKeys::Gamepad_FaceButton_Right)  return TEXT("B");
    if (K == EKeys::Gamepad_FaceButton_Left)   return TEXT("X");
    if (K == EKeys::Gamepad_FaceButton_Top)    return TEXT("Y");
    if (K == EKeys::Gamepad_LeftShoulder)      return TEXT("LB");
    if (K == EKeys::Gamepad_RightShoulder)     return TEXT("RB");
    if (K == EKeys::Gamepad_LeftTrigger)       return TEXT("LT");
    if (K == EKeys::Gamepad_RightTrigger)      return TEXT("RT");
    if (K == EKeys::Gamepad_LeftThumbstick)    return TEXT("L3");
    if (K == EKeys::Gamepad_RightThumbstick)   return TEXT("R3");
    if (K == EKeys::Gamepad_Special_Left)      return TEXT("View");
    if (K == EKeys::Gamepad_Special_Right)     return TEXT("Menu");
    if (K == EKeys::Gamepad_DPad_Up)           return TEXT("↑");
    if (K == EKeys::Gamepad_DPad_Down)         return TEXT("↓");
    if (K == EKeys::Gamepad_DPad_Left)         return TEXT("←");
    if (K == EKeys::Gamepad_DPad_Right)        return TEXT("→");
    return K.GetDisplayName().ToString();
}

FString PTGamepad::BuildHintLine()
{
    FString Out;
    for (const FPTGamepadAction& A : GetActions())
    {
        if (A.Id == TEXT("Pause")) continue;
        if (!Out.IsEmpty()) Out += TEXT("  ·  ");
        Out += KeyLabel(A.Key) + TEXT(" ") + PTText::GetStr(A.LabelKey);
    }
    return Out;
}

float PTGamepad::LookSensitivity()
{
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    return S ? S->GetGamepadLookSensitivity() : 1.f;
}

float PTGamepad::MoveSensitivity()
{
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    return S ? S->GetGamepadMoveSensitivity() : 1.f;
}

bool PTGamepad::InvertY()
{
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    return S && S->GetGamepadInvertY();
}

float PTGamepad::DeadZone()
{
    const UPTGameUserSettings* S = UPTGameUserSettings::Get();
    return S ? S->GetGamepadDeadZone() : 0.2f;
}

FVector2D PTGamepad::ReadStick(const APlayerController* PC, const FKey& X, const FKey& Y)
{
    if (!PC) return FVector2D::ZeroVector;
    const FVector2D Raw(PC->GetInputAnalogKeyState(X), PC->GetInputAnalogKeyState(Y));
    const float Mag = Raw.Size();
    const float Dz = FMath::Clamp(DeadZone(), 0.f, 0.9f);
    if (Mag <= Dz) return FVector2D::ZeroVector;
    return Raw / Mag * ((FMath::Min(Mag, 1.f) - Dz) / (1.f - Dz));
}

void PTGamepad::TickMoveLook(APlayerController* PC, float DeltaTime, bool bAllowMove, bool bAllowLook,
                             float YawSpeed, float PitchSpeed, float Exponent, bool bExtraInvertY)
{
    if (!PC || !PC->IsLocalController()) return;

    if (bAllowLook && !PC->IsLookInputIgnored())
    {
        const FVector2D R = ReadStick(PC, EKeys::Gamepad_RightX, EKeys::Gamepad_RightY);
        if (!R.IsZero())
        {
            // Curva: más precisión cerca del centro, velocidad completa a fondo.
            const float Mag = FMath::Pow(FMath::Min(R.Size(), 1.f), FMath::Max(1.f, Exponent));
            const FVector2D D = R.GetSafeNormal() * Mag;
            const float Sens = LookSensitivity();
            const bool bInvert = InvertY() != bExtraInvertY;

            FRotator Rot = PC->GetControlRotation();
            Rot.Yaw += D.X * YawSpeed * Sens * DeltaTime;
            const float Pitch = FRotator::NormalizeAxis(Rot.Pitch) + D.Y * (bInvert ? -1.f : 1.f) * PitchSpeed * Sens * DeltaTime;
            const APlayerCameraManager* Cam = PC->PlayerCameraManager;
            const float MinP = FMath::Max(Cam ? Cam->ViewPitchMin : -89.f, -89.f);
            const float MaxP = FMath::Min(Cam ? Cam->ViewPitchMax :  89.f,  89.f);
            Rot.Pitch = FMath::Clamp(Pitch, MinP, MaxP);
            PC->SetControlRotation(Rot);
        }
    }

    if (bAllowMove)
    {
        const FVector2D L = ReadStick(PC, EKeys::Gamepad_LeftX, EKeys::Gamepad_LeftY) * MoveSensitivity();
        if (!L.IsZero())
            if (APawn* P = PC->GetPawn())
            {
                const FRotator YawRot(0.f, PC->GetControlRotation().Yaw, 0.f);
                P->AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::X), L.Y);
                P->AddMovementInput(FRotationMatrix(YawRot).GetUnitAxis(EAxis::Y), L.X);
            }
    }
}
