// Copyright Epic Games, Inc. All Rights Reserved.
// Settings del template (sonido/idioma/gráficos), persistentes entre sesiones.
// Los gráficos reusan la escalabilidad nativa de UGameUserSettings; volumen e idioma son propios.

#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "PTGameUserSettings.generated.h"

UCLASS()
class MYPARTYGAME_API UPTGameUserSettings : public UGameUserSettings
{
    GENERATED_BODY()

public:
    virtual void SetToDefaults() override;

    /** Acceso rápido al settings activo, ya cargado desde disco. */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    static UPTGameUserSettings* Get();

    UFUNCTION(BlueprintCallable, Category = "Settings")
    float GetMasterVolume() const { return MasterVolume; }

    /** Aplica el volumen inmediatamente (FAudioDevice) y lo deja pendiente de guardar. */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetMasterVolume(float InVolume);

    UFUNCTION(BlueprintCallable, Category = "Settings")
    FString GetLanguageCode() const { return LanguageCode; }

    /** "en" o "es". Cambia la cultura activa de inmediato. */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetLanguageCode(const FString& InLanguageCode);

    /** Wrapper simple sobre la escalabilidad nativa: 0=Low .. 3=Epic. */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    int32 GetGraphicsQuality() const;

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetGraphicsQuality(int32 InQuality);

    /** Aplica el volumen y la cultura guardados. Llamar una vez al arrancar (MainMenu). */
    void ApplyAudioAndLanguage(UWorld* World);

    // ── Rebind de teclas (ver PTInputBindings.h) ────────────────────────────
    // Override por acción: Id de acción → nombre de tecla (FKey::ToString). Solo se guardan las
    // que el usuario cambió; el resto usa el default de la tabla.
    const TMap<FName, FString>& GetKeyOverrides() const { return KeyOverrides; }
    void SetKeyOverride(FName ActionId, const FString& KeyName) { KeyOverrides.Add(ActionId, KeyName); }
    void ClearKeyOverrides() { KeyOverrides.Reset(); }

private:
    UPROPERTY(Config)
    float MasterVolume = 1.0f;

    UPROPERTY(Config)
    FString LanguageCode = TEXT("en");

    UPROPERTY(Config)
    TMap<FName, FString> KeyOverrides;
};
