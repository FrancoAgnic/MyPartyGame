# FASE 4 — Genera los sonidos placeholder del modo Sillas (sintetizados desde
# cero, sin assets externos), los importa a /Game/Sillas/Audio y enchufa:
#  - DA_HabilidadTaunt (USillasAbilityTaunt) al componente Habilidad de BP_SillasPawnSilla
# Correr headless despues de compilar:
#   UnrealEditor-Cmd.exe MyPartyGame.uproject -run=pythonscript
#       -script="Tools/generar_audio_sillas.py" -stdout -unattended
# Idempotente: re-importa los WAV (pisa el audio placeholder) pero no toca
# assets que Davor haya editado a mano (el DA solo se crea si no existe).
# El sound design REAL llega en Fase 6 — esto es greybox sonoro.

import math
import os
import random
import struct
import wave

import unreal

SR = 44100
TMP = os.path.join(unreal.Paths.project_saved_dir(), "Temp", "SillasAudio")
os.makedirs(TMP, exist_ok=True)
random.seed(42)  # placeholders deterministas (mismo resultado en cada corrida)

RESULT = os.path.join(TMP, "audio_result.txt")
lines = []


def write_wav(nombre, samples):
    path = os.path.join(TMP, nombre + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(SR)
        w.writeframes(b"".join(
            struct.pack("<h", max(-32767, min(32767, int(s * 32767)))) for s in samples))
    return path


def bump(t, inicio, dur):
    """Envolvente sin^2 entre inicio y inicio+dur (0 fuera)."""
    if t < inicio or t > inicio + dur:
        return 0.0
    return math.sin(math.pi * (t - inicio) / dur) ** 2


# --- Respiración (loop 4s): dos soplidos suaves de ruido filtrado (D17) ---
def gen_respiracion():
    n = int(SR * 4.0)
    out, y = [], 0.0
    for i in range(n):
        t = i / SR
        x = random.uniform(-1, 1)
        y += 0.12 * (x - y)  # lowpass: hiss de aire, no estática
        env = 0.9 * bump(t, 0.15, 1.3) + 1.0 * bump(t, 2.05, 1.5)
        out.append(y * env * 0.5)
    return out


# --- Crujido de madera (loop 1.8s): eventos cortos de wobble grave + ruido ---
def gen_crujido():
    n = int(SR * 1.8)
    out = [0.0] * n
    y = 0.0
    eventos = [(0.10, 0.14, 95), (0.42, 0.10, 130), (0.72, 0.16, 80),
               (1.05, 0.09, 145), (1.33, 0.13, 105), (1.60, 0.10, 120)]
    for inicio, dur, f in eventos:
        fase = random.uniform(0, math.tau)
        for i in range(int(inicio * SR), min(n, int((inicio + dur) * SR))):
            t = i / SR
            env = bump(t, inicio, dur)
            x = random.uniform(-1, 1)
            y += 0.3 * (x - y)
            wob = math.sin(math.tau * f * t + 3.0 * math.sin(math.tau * 7 * t) + fase)
            out[i] += (0.55 * wob + 0.35 * y) * env * 0.55
    return out


# --- Música (loop 2s, 120 BPM): metrónomo musical legible (D3: patrón fijo) ---
def gen_musica():
    n = int(SR * 2.0)
    out = [0.0] * n
    notas = [(0.00, 523.25), (0.50, 523.25), (1.00, 523.25), (1.50, 784.0)]  # C5 C5 C5 G5
    for inicio, f in notas:
        for i in range(int(inicio * SR), min(n, int((inicio + 0.22) * SR))):
            t = i / SR
            rel = t - inicio
            env = math.exp(-rel * 14.0)
            s = math.sin(math.tau * f * t) + 0.35 * math.sin(math.tau * 2 * f * t)
            out[i] += s * env * 0.5
    return out


# --- Jadeo (one-shot 1.8s): 3 bocanadas fuertes y rápidas — el delator de D18 ---
def gen_jadeo():
    n = int(SR * 1.8)
    out, y = [], 0.0
    for i in range(n):
        t = i / SR
        x = random.uniform(-1, 1)
        y += 0.2 * (x - y)
        env = bump(t, 0.02, 0.38) + bump(t, 0.55, 0.38) + bump(t, 1.10, 0.45)
        out.append(y * env * 0.95)
    return out


# --- Taunt (1.2s): pedorreta de dos bocinazos burlones (D10b) ---
def gen_taunt():
    n = int(SR * 1.2)
    out = []
    for i in range(n):
        t = i / SR
        s = 0.0
        if t < 0.45:
            f = 260 - 90 * (t / 0.45) + 12 * math.sin(math.tau * 9 * t)  # bocinazo cayendo
            s = (2.0 * ((f * t) % 1.0) - 1.0) * bump(t, 0.0, 0.45)
        elif 0.55 <= t < 1.15:
            f = 300 - 140 * ((t - 0.55) / 0.6) + 15 * math.sin(math.tau * 11 * t)
            s = (2.0 * ((f * t) % 1.0) - 1.0) * bump(t, 0.55, 0.6)
        out.append(s * 0.6)
    return out


SONIDOS = [
    ("A_Respiracion_Loop", gen_respiracion, True),
    ("A_Crujido_Loop",     gen_crujido,     True),
    ("A_Musica_Loop",      gen_musica,      True),
    ("A_Jadeo",            gen_jadeo,       False),
    ("A_Taunt",            gen_taunt,       False),
]

# 1) Sintetizar + importar
tareas = []
for nombre, gen, _ in SONIDOS:
    path = write_wav(nombre, gen())
    task = unreal.AssetImportTask()
    task.filename = path
    task.destination_path = "/Game/Sillas/Audio"
    task.automated = True
    task.replace_existing = True
    task.save = True
    tareas.append(task)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tareas)

# 2) Marcar loops
for nombre, _, es_loop in SONIDOS:
    ruta = "/Game/Sillas/Audio/%s" % nombre
    wave_asset = unreal.load_asset(ruta)
    if wave_asset is None:
        lines.append("%s: FALLO el import" % nombre)
        continue
    if es_loop:
        ok = False
        for prop in ("looping", "b_looping"):
            try:
                wave_asset.set_editor_property(prop, True)
                ok = True
                break
            except Exception:
                continue
        unreal.EditorAssetLibrary.save_loaded_asset(wave_asset)
        lines.append("%s: importado, loop=%s" % (nombre, ok))
    else:
        lines.append("%s: importado" % nombre)

# 3) DA_HabilidadTaunt + enchufe en BP_SillasPawnSilla
DA_PATH = "/Game/Sillas/Data/DA_HabilidadTaunt"
if not unreal.EditorAssetLibrary.does_asset_exist(DA_PATH):
    factory = unreal.DataAssetFactory()
    for prop in ("data_asset_class", "data_class"):
        try:
            factory.set_editor_property(prop, unreal.SillasAbilityTaunt)
            break
        except Exception:
            continue
    da = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_HabilidadTaunt", "/Game/Sillas/Data", unreal.SillasAbilityTaunt, factory)
    lines.append("DA_HabilidadTaunt: CREADO")
else:
    da = unreal.load_asset(DA_PATH)
    lines.append("DA_HabilidadTaunt: ya existia")

taunt_snd = unreal.load_asset("/Game/Sillas/Audio/A_Taunt")
if da and taunt_snd and da.get_editor_property("sonido") is None:
    da.set_editor_property("sonido", taunt_snd)
unreal.EditorAssetLibrary.save_asset(DA_PATH)

bp = unreal.load_asset("/Game/Sillas/Blueprints/BP_SillasPawnSilla")
if bp and da:
    cdo = unreal.get_default_object(bp.generated_class())
    comp = cdo.get_editor_property("habilidad")
    if comp and comp.get_editor_property("habilidad") is None:
        comp.set_editor_property("habilidad", da)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_loaded_asset(bp)
        lines.append("BP_SillasPawnSilla.Habilidad.Habilidad = DA_HabilidadTaunt")
    else:
        lines.append("BP_SillasPawnSilla: componente ya tenia habilidad (no tocado)")

with open(RESULT, "w") as f:
    f.write("\n".join(lines) + "\n")
