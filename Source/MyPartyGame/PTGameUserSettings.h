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

    // ── Volúmenes separados por Sound Class (Música / Efectos). El almacenamiento vive acá; la
    //    aplicación real (SetSoundMixClassOverride) la hace UPTGameInstance::ApplyAudioMix. ──
    UFUNCTION(BlueprintCallable, Category = "Settings") float GetMusicVolume() const { return MusicVolume; }
    UFUNCTION(BlueprintCallable, Category = "Settings") float GetSFXVolume()   const { return SFXVolume; }
    UFUNCTION(BlueprintCallable, Category = "Settings") void  SetMusicVolume(float V) { MusicVolume = FMath::Clamp(V, 0.f, 1.f); }
    UFUNCTION(BlueprintCallable, Category = "Settings") void  SetSFXVolume(float V)   { SFXVolume   = FMath::Clamp(V, 0.f, 1.f); }

    // Sonido de "máquina de escribir" al tipear en el chat (se puede desactivar).
    UFUNCTION(BlueprintCallable, Category = "Settings") bool  IsTypingSoundEnabled() const { return bTypingSoundEnabled; }
    UFUNCTION(BlueprintCallable, Category = "Settings") void  SetTypingSoundEnabled(bool b) { bTypingSoundEnabled = b; }

    UFUNCTION(BlueprintCallable, Category = "Settings")
    FString GetLanguageCode() const { return LanguageCode; }

    // ── Primer arranque: pantalla de selección de idioma ────────────────────
    // false = todavía no eligió idioma → el MainMenu muestra la pantalla de selección (solo la
    // primera vez). Al elegir se marca true y se guarda; después se cambia desde Configuración.
    UFUNCTION(BlueprintCallable, Category = "Settings")
    bool HasChosenLanguage() const { return bLanguageChosen; }

    UFUNCTION(BlueprintCallable, Category = "Settings")
    void MarkLanguageChosen();

    /** DEV: resetea el flag (bLanguageChosen=false) para volver a ver la pantalla de idioma en el
     *  próximo arranque del boot. Lo llama el comando de consola PT.ResetLanguage. */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void ResetLanguageChosen();

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
    float MusicVolume = 1.0f;
    UPROPERTY(Config)
    float SFXVolume = 1.0f;
    UPROPERTY(Config)
    bool bTypingSoundEnabled = true;

    UPROPERTY(Config)
    FString LanguageCode = TEXT("en");

    UPROPERTY(Config)
    bool bLanguageChosen = false;

    UPROPERTY(Config)
    TMap<FName, FString> KeyOverrides;
};
