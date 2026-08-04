# FASE 0 — Genera el mapa greybox /Game/Sillas/Maps/L_TestArena desde cero.
# Correr con el editor headless:
#   UnrealEditor-Cmd.exe MyPartyGame.uproject -run=pythonscript
#       -script="Tools/generar_greybox_testarena.py" -stdout -unattended
# Idempotente: si el nivel ya existe lo pisa (regenerar = volver a correr).
#
# Layout: sala rectangular 30m x 40m, paredes de 4m, pilares y muretes como
# obstaculos, 20 TargetPoints con tag "SillaSpawn" (los lee la Fase 1 para
# spawnear senuelos y ubicar sillas-jugador) y 8 PlayerStarts.

import unreal

LEVEL_PATH = "/Game/Sillas/Maps/L_TestArena"

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

# new_level pisa el asset si ya existe (con force). Firma 5.x: new_level(asset_path)
if not les.new_level(LEVEL_PATH):
    raise RuntimeError("No se pudo crear el nivel " + LEVEL_PATH)

cube = unreal.load_asset("/Engine/BasicShapes/Cube")  # 100x100x100 cm, con colision


def caja(nombre, x, y, z, sx, sy, sz):
    a = eas.spawn_actor_from_class(
        unreal.StaticMeshActor, unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0))
    a.set_actor_label(nombre)
    a.static_mesh_component.set_static_mesh(cube)
    a.set_actor_scale3d(unreal.Vector(sx, sy, sz))
    return a


# --- Sala: piso 3000x4000, paredes altas 400 ---
caja("Piso", 0, 0, -25, 30.0, 40.0, 0.5)          # cara superior en z=0
caja("Pared_N", 0, 2025, 200, 30.0, 0.5, 4.0)
caja("Pared_S", 0, -2025, 200, 30.0, 0.5, 4.0)
caja("Pared_E", 1525, 0, 200, 0.5, 41.0, 4.0)
caja("Pared_O", -1525, 0, 200, 0.5, 41.0, 4.0)

# --- Obstaculos simples: 4 pilares, 2 muretes, 2 cajones ---
for i, (px, py) in enumerate([(-700, -1200), (700, -1200), (-700, 1200), (700, 1200)]):
    caja("Pilar_%d" % i, px, py, 200, 1.0, 1.0, 4.0)
caja("Murete_A", 0, -600, 75, 6.0, 0.5, 1.5)
caja("Murete_B", 0, 600, 75, 6.0, 0.5, 1.5)
caja("Cajon_A", 1000, 0, 100, 2.0, 2.0, 2.0)
caja("Cajon_B", -1000, 0, 100, 2.0, 2.0, 2.0)

# --- 20 puntos de spawn de sillas (grilla 5x4 con offsets a mano, no random:
#     el jitter real lo aplica el spawner de la Fase 1 en runtime) ---
XS = [-1200, -600, 0, 600, 1200]
YS = [-1500, -500, 500, 1500]
OFFSETS = [(60, -40), (-80, 30), (20, 90), (-40, -70), (90, 10),
           (-30, 60), (70, -90), (0, 40), (-90, -20), (50, 70),
           (30, -60), (-70, 80), (80, 20), (-20, -90), (40, 50),
           (-60, -30), (10, 80), (90, -50), (-50, 20), (60, 60)]
n = 0
for y in YS:
    for x in XS:
        ox, oy = OFFSETS[n]
        tp = eas.spawn_actor_from_class(
            unreal.TargetPoint, unreal.Vector(x + ox, y + oy, 10), unreal.Rotator(0, 0, 0))
        tp.set_actor_label("SillaSpawn_%02d" % n)
        tp.set_editor_property("tags", ["SillaSpawn"])
        n += 1

# --- 8 PlayerStarts (fila central; en Fase 1 el spawn real pasa a ser por rol) ---
for i in range(8):
    ps = eas.spawn_actor_from_class(
        unreal.PlayerStart, unreal.Vector(-700 + i * 200, 0, 100), unreal.Rotator(0, 0, 90))
    ps.set_actor_label("PlayerStart_%d" % i)

# --- Luces (todo dinamico: el proyecto tiene AllowStaticLighting=False) ---
sol = eas.spawn_actor_from_class(
    unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(-50, 30, 0))
sol.set_actor_label("Sol")
sol.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)

sky = eas.spawn_actor_from_class(
    unreal.SkyLight, unreal.Vector(0, 0, 800), unreal.Rotator(0, 0, 0))
sky.set_actor_label("SkyLight")
sky.light_component.set_mobility(unreal.ComponentMobility.MOVABLE)
sky.light_component.set_editor_property("real_time_capture", True)

atm = eas.spawn_actor_from_class(
    unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
atm.set_actor_label("SkyAtmosphere")

# --- GameMode del mapa: ASillasGameMode ---
# OJO: get_all_level_actors() NO devuelve el WorldSettings — llegar via GameplayStatics.
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
arr = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings)
gm = unreal.load_class(None, "/Script/MyPartyGame.SillasGameMode")
if not arr or gm is None:
    raise RuntimeError("No pude setear el GameMode del mapa (ws=%s, gm=%s)" % (arr, gm))
arr[0].set_editor_property("default_game_mode", gm)

if not les.save_current_level():
    raise RuntimeError("No se pudo guardar el nivel")

unreal.log("L_TestArena generado OK: %d spawns de silla, 8 PlayerStarts" % n)
