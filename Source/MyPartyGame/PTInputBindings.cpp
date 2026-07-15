// Copyright Epic Games, Inc. All Rights Reserved.

#include "PTInputBindings.h"
#include "PTGameUserSettings.h"

namespace PTInput
{
    static TArray<FPTKeyBinding> GCached;
    static bool GBuilt = false;

    // Defaults. El ORDEN es el que se ve en la UI.
    static void BuildDefaults(TArray<FPTKeyBinding>& Out)
    {
        auto Add = [&Out](const TCHAR* Id, const TCHAR* Label, const FKey& K, bool bRebind = true)
        {
            FPTKeyBinding B;
            B.Id          = FName(Id);
            B.Label       = FText::FromString(Label);
            B.Key         = K;
            B.bRebindable = bRebind;
            Out.Add(B);
        };

        // Movimiento/cámara: vienen de Enhanced Input (mapping contexts), se muestran como info.
        Add(TEXT("Move"),   TEXT("Mover"),   EKeys::W, /*bRebindable=*/false);
        Add(TEXT("Look"),   TEXT("Cámara"),  EKeys::Mouse2D, false);
        Add(TEXT("FlyUp"),  TEXT("Subir"),   EKeys::SpaceBar, false);
        Add(TEXT("FlyDown"),TEXT("Bajar"),   EKeys::LeftControl, false);

        // Esculpido (estas sí se bindean por BindKey → rebindeables).
        Add(TEXT("Sculpt"),         TEXT("Esculpir"),          EKeys::LeftMouseButton, false); // Action Mapping "Sculpt"
        Add(TEXT("BrushSize"),      TEXT("Tamaño de brocha"),  EKeys::MouseWheelAxis,  false);
        Add(TEXT("ModeAdd"),        TEXT("Modo: Agregar"),     EKeys::One);
        Add(TEXT("ModeErase"),      TEXT("Modo: Borrar"),      EKeys::Two);
        Add(TEXT("ModePaint"),      TEXT("Modo: Pintar"),      EKeys::Three);
        Add(TEXT("ModeEyes"),       TEXT("Modo: Ojos"),        EKeys::Four);
        Add(TEXT("CycleShape"),     TEXT("Cambiar forma"),     EKeys::Tab);
        Add(TEXT("AxisVertical"),   TEXT("Plano vertical"),    EKeys::Z);
        Add(TEXT("AxisHorizontal"), TEXT("Plano horizontal"),  EKeys::X);
        Add(TEXT("RotateShape"),    TEXT("Rotar forma"),       EKeys::MiddleMouseButton);
        Add(TEXT("ClearAll"),       TEXT("Deshacer / Borrar todo (3s)"), EKeys::BackSpace);
        Add(TEXT("ColorPick"),      TEXT("Elegir color"),      EKeys::RightMouseButton);
        Add(TEXT("SaveColor"),      TEXT("Guardar color"),     EKeys::E);
        Add(TEXT("Chat"),           TEXT("Chat"),              EKeys::Enter);
        Add(TEXT("Pause"),          TEXT("Pausa"),             EKeys::Escape);
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
