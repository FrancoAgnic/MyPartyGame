// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTInputBindings.h"
#include "PTGameUserSettings.h"
#include "PTTextTable.h"

namespace PTInput
{
    static TArray<FPTKeyBinding> GCached;
    static bool GBuilt = false;

    // Defaults. El ORDEN es el que se ve en la UI.
    static void BuildDefaults(TArray<FPTKeyBinding>& Out)
    {
        auto Add = [&Out](const TCHAR* Id, const TCHAR* LabelKey, const FKey& K, bool bRebind = true)
        {
            FPTKeyBinding B;
            B.Id          = FName(Id);
            B.LabelKey    = FName(LabelKey); // el texto se resuelve en GetBindings (idioma actual)
            B.Key         = K;
            B.bRebindable = bRebind;
            Out.Add(B);
        };

        // Movimiento/cámara: vienen de Enhanced Input (mapping contexts), se muestran como info.
        Add(TEXT("Move"),   TEXT("KEY_MOVE"),     EKeys::W, /*bRebindable=*/false);
        Add(TEXT("Look"),   TEXT("KEY_LOOK"),     EKeys::Mouse2D, false);
        Add(TEXT("FlyUp"),  TEXT("KEY_FLY_UP"),   EKeys::SpaceBar, false);
        Add(TEXT("FlyDown"),TEXT("KEY_FLY_DOWN"), EKeys::LeftControl, false);

        // Esculpido (estas sí se bindean por BindKey → rebindeables).
        Add(TEXT("Sculpt"),         TEXT("KEY_SCULPT"),      EKeys::LeftMouseButton, false); // Action Mapping "Sculpt"
        Add(TEXT("BrushSize"),      TEXT("KEY_BRUSH_SIZE"),  EKeys::MouseWheelAxis,  false);
        Add(TEXT("ModeAdd"),        TEXT("KEY_MODE_ADD"),    EKeys::One);
        Add(TEXT("ModeErase"),      TEXT("KEY_MODE_ERASE"),  EKeys::Two);
        Add(TEXT("ModePaint"),      TEXT("KEY_MODE_PAINT"),  EKeys::Three);
        Add(TEXT("ModeEyes"),       TEXT("KEY_MODE_EYES"),   EKeys::Four);
        Add(TEXT("CycleShape"),     TEXT("KEY_CYCLE_SHAPE"), EKeys::Tab);
        Add(TEXT("AxisVertical"),   TEXT("KEY_AXIS_VERT"),   EKeys::Z);
        Add(TEXT("AxisHorizontal"), TEXT("KEY_AXIS_HORIZ"),  EKeys::X);
        Add(TEXT("RotateShape"),    TEXT("KEY_ROTATE_SHAPE"),EKeys::MiddleMouseButton);
        Add(TEXT("ClearAll"),       TEXT("KEY_CLEAR_ALL"),   EKeys::BackSpace);
        Add(TEXT("ColorPick"),      TEXT("KEY_COLOR_PICK"),  EKeys::RightMouseButton);
        Add(TEXT("SaveColor"),      TEXT("KEY_SAVE_COLOR"),  EKeys::E);
        Add(TEXT("Chat"),           TEXT("KEY_CHAT"),        EKeys::Enter);
        Add(TEXT("Pause"),          TEXT("KEY_PAUSE"),       EKeys::Escape);
    }

    static void EnsureBuilt()
    {
        if (GBuilt) return;
        GBuilt = true;
        GCached.Reset();
        BuildDefaults(GCached);

        // Aplicar los overrides guardados por el usuario.
        if (const UPTGameUserSettings* S = UPTGameUserSettings::Get())
        {
            for (FPTKeyBinding& B : GCached)
            {
                if (!B.bRebindable) continue;
                if (const FString* KeyName = S->GetKeyOverrides().Find(B.Id))
                {
                    const FKey K(**KeyName);
                    if (K.IsValid()) B.Key = K;
                }
            }
        }
    }

    const TArray<FPTKeyBinding>& GetBindings()
    {
        EnsureBuilt();
        // Los nombres visibles se resuelven acá y no en la caché: si el jugador cambia el idioma,
        // la próxima lectura ya sale traducida sin invalidar los rebinds.
        for (FPTKeyBinding& B : GCached)
            B.Label = PTText::Get(B.LabelKey);
        return GCached;
    }

    FKey GetKey(FName Id)
    {
        EnsureBuilt();
        for (const FPTKeyBinding& B : GCached)
            if (B.Id == Id) return B.Key;
        return EKeys::Invalid;
    }

    void SetKey(FName Id, const FKey& NewKey)
    {
        EnsureBuilt();
        if (!NewKey.IsValid()) return;

        for (FPTKeyBinding& B : GCached)
        {
            if (B.Id != Id || !B.bRebindable) continue;
            B.Key = NewKey;
            if (UPTGameUserSettings* S = UPTGameUserSettings::Get())
            {
                S->SetKeyOverride(Id, NewKey.ToString());
                S->SaveSettings();
            }
            return;
        }
    }

    void RefreshFromSettings()
    {
        GBuilt = false;
        EnsureBuilt();
    }
}
