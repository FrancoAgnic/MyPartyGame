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

    /** Tutorial de Sculpi: true cuando lo terminó o lo saltó (no se vuelve a lanzar solo; se repite
     *  desde el botón "Tutorial" del menú Jugar). PT.ResetTutorial lo vuelve a false. */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    bool HasDoneTutorial() const { return bTutorialDone; }
    void SetTutorialDone(bool bDone) { bTutorialDone = bDone; SaveSettings(); }

    /** Tutorial RÁPIDO (imitá a Sculpi): obligatorio solo la 1.ª vez que se abre el juego (tras elegir
     *  idioma). Flag aparte del avanzado. PT.ResetTutorial también lo resetea. */
    bool HasDoneQuickTutorial() const { return bQuickTutorialDone; }
    void SetQuickTutorialDone(bool bDone) { bQuickTutorialDone = bDone; SaveSettings(); }

    /** "en" o "es". Cambia la cultura activa de inmediato. */
    UFUNCTION(BlueprintCallable, Category = "Settings")
    void SetLanguageCode(const FString& InLanguageCode);

    /** Cambia el idioma activo SIN guardarlo a disco (uso temporal: espectar el POV de otro jugador en
     *  SU idioma y volver al tuyo al salir, sin pisar tu preferencia guardada). Re-traduce la UI igual. */
    void SetLanguageCodeTransient(const FString& InLanguageCode);

    // ── Resolución ──────────────────────────────────────────────────────────
    // La resolución real (ResolutionSizeX/Y + modo pantalla) la persiste UGameUserSettings base.
    // Acá solo recordamos si el usuario eligió "Automática (recomendada)" para volver a seguir la
    // resolución del escritorio en cada arranque si cambia de monitor.
    UFUNCTION(BlueprintCallable, Category = "Settings") bool IsAutoResolution() const { return bAutoResolution; }
    UFUNCTION(BlueprintCallable, Category = "Settings") void SetAutoResolution(bool b) { bAutoResolution = b; }

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

    // ── Joystick (ver PTGamepad.h y el panel UPTGamepadSettingsWidget) ──────
    float GetGamepadLookSensitivity() const { return GamepadLookSensitivity; }
    void  SetGamepadLookSensitivity(float V) { GamepadLookSensitivity = FMath::Clamp(V, 0.2f, 3.f); }
    bool  GetGamepadInvertY() const { return bGamepadInvertY; }
    void  SetGamepadInvertY(bool b) { bGamepadInvertY = b; }
    float GetGamepadDeadZone() const { return GamepadDeadZone; }
    void  SetGamepadDeadZone(float V) { GamepadDeadZone = FMath::Clamp(V, 0.05f, 0.5f); }
    float GetGamepadMoveSensitivity() const { return GamepadMoveSensitivity; }
    void  SetGamepadMoveSensitivity(float V) { GamepadMoveSensitivity = FMath::Clamp(V, 0.3f, 1.f); }

    // ── Cámara con MOUSE (panel "Controles" → Teclado y ratón) ──
    float GetMouseLookSensitivity() const { return MouseLookSensitivity; }
    void  SetMouseLookSensitivity(float V) { MouseLookSensitivity = FMath::Clamp(V, 0.2f, 3.f); }
    // Botón por acción (Id → nombre de FKey). Solo las que el usuario cambió.
    const TMap<FName, FString>& GetGamepadOverrides() const { return GamepadOverrides; }
    void SetGamepadOverride(FName ActionId, const FString& KeyName) { GamepadOverrides.Add(ActionId, KeyName); }
    void ClearGamepadOverrides() { GamepadOverrides.Reset(); }

    // ── Modo audiencia: canales de Twitch / Kick cuyo chat se lee (se recuerdan entre partidas) ──
    const FString& GetTwitchChannel() const { return TwitchChannel; }
    void SetTwitchChannel(const FString& V) { TwitchChannel = V; }
    const FString& GetKickChannel() const { return KickChannel; }
    void SetKickChannel(const FString& V) { KickChannel = V; }

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
    bool bTutorialDone = false;

    UPROPERTY(Config)
    bool bQuickTutorialDone = false;

    // true = seguir la resolución del escritorio (recomendada). false = el usuario fijó una manual.
    UPROPERTY(Config)
    bool bAutoResolution = true;

    UPROPERTY(Config)
    TMap<FName, FString> KeyOverrides;

    UPROPERTY(Config) float GamepadLookSensitivity = 1.0f; // multiplica la velocidad de cámara del stick
    UPROPERTY(Config) bool  bGamepadInvertY = false;
    UPROPERTY(Config) float GamepadDeadZone = 0.2f;
    UPROPERTY(Config) float GamepadMoveSensitivity = 1.0f; // velocidad máxima al mover con el stick
    UPROPERTY(Config) float MouseLookSensitivity   = 1.0f; // multiplica la velocidad de cámara con el mouse
    UPROPERTY(Config) TMap<FName, FString> GamepadOverrides;
    UPROPERTY(Config) FString TwitchChannel;
    UPROPERTY(Config) FString KickChannel;
};
