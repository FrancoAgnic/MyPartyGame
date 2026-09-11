# Sculpturillo — Custom Maps (spec, M2)

Requisitos que debe cumplir un **mapa de mod** para que la partida funcione igual que `Lvl-01`.
El GameMode lo **fuerza el juego** al viajar (`?game=BP_SculptGameMode`), así que el creador NO configura
GameMode: solo arma el mapa con lo siguiente.

## Qué debe tener el mapa (obligatorio)
1. **Un `APTSculptVolume`** (exactamente uno). Es la zona de arcilla; el juego lo encuentra con
   `GetActorOfClass(APTSculptVolume)`. Copiarlo tal cual de `Lvl-01` (misma escala/config del BoundsBox).
   Sin esto, la partida arranca pero no se puede esculpir.
2. **PlayerStarts** — al menos tantos como jugadores máximos (Lvl-01 tiene ~10–12). Los jugadores vuelan
   (vuelo forzado), así que la ubicación exacta no es crítica, pero conviene distribuirlos alrededor del volumen.
3. **Iluminación** — el mapa es un espacio propio; poner al menos una luz + SkyLight/HDRI o un color de fondo,
   si no se ve negro.

## Opcional (estética)
- Escenografía / piso / fondo a gusto del creador (es la gracia del mapa custom).
- Música/ambiente propio si querés.

## Lo que NO hace falta
- **GameMode**: lo fuerza el juego por la URL de travel (`?game=/Game/Template/Character/BP_SculptGameMode...`).
- HUD, reglas, turnos, palabras: todo lo maneja el GameMode forzado.

## Mapa plantilla (para duplicar) — a crear en el editor (M3/kit)
Un `.umap` base ya listo con: 1× `APTSculptVolume` (config de Lvl-01) + ~12 PlayerStarts + una luz + SkyLight.
El creador lo **duplica**, le agrega su escenografía, y lo cocina a `.pak` (ver M3).

## Empaquetado (adelanto M3)
El mapa se cociná a un **`.pak`** con **Unreal Engine 5.8** (misma versión) y se entrega junto a un
`mod.json`:
```json
{ "MapName": "/Game/MapMods/MiMapa/MiMapa", "Title": "Mi Mapa", "Author": "TuNombre" }
```
`MapName` = ruta de paquete del `.umap` dentro del pak (a la que el juego hace ServerTravel).
El loader (M1, `UPTMapModSubsystem`) monta el `.pak` y viaja a `MapName` con el GameMode forzado.
