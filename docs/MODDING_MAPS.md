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

## Empaquetado — M3 (DLC cook con el plugin MapKit)
Método elegido: **cook de DLC** vía UAT usando el plugin de contenido **`MapKit`**. El `.pak` sale limpio
(solo tu mapa, con deps y mount point `/MapKit/` resueltos).

**Requisitos del creador:** el **proyecto/kit** + **Unreal Engine 5.8** + la carpeta `Releases/Sculpturillo1`
(viene en el kit; la genera el dev con `Kit_PrepararBase.bat`, se corre una sola vez por versión).

**Flujo del creador:**
1. Abrí el proyecto en UE 5.8. En el Content Browser, entrá al plugin **MapKit** (`/MapKit/`).
2. **Duplicá el mapa plantilla** ahí (trae 1× `APTSculptVolume` + PlayerStarts + luz). Ponele tu nombre y
   agregá tu escenografía. Guardalo en `/MapKit/TuMapa`.
3. Corré **`Kit_CocinarMapa.bat`** → cocina el mapa a `.pak` (DLC) y lo deja en `MapMods_Output\map.pak`
   + un `mod.json` de plantilla.
4. Editá `mod.json`: `MapName` = `/MapKit/TuMapa` (el nombre real de tu `.umap`), `Title`, `Author`.
5. **Probar local:** copiá `map.pak` + `mod.json` a `<Proyecto>\MapMods\TuMod\` y abrí el **juego empaquetado**
   (en el editor el montaje de paks no está disponible). El loader (M1) lo detecta y lo monta.
6. **Publicar:** subirlo por el Workshop (M4, próximamente).

**`mod.json`:**
```json
{ "MapName": "/MapKit/TuMapa", "Title": "Mi Mapa", "Author": "TuNombre" }
```
El loader (M1, `UPTMapModSubsystem`) monta el `.pak` y el host viaja a `MapName` con el GameMode forzado
(`APTLobbyGameMode::TravelToModMap`).

> **Estado M3:** plugin `MapKit` + scripts `Kit_PrepararBase.bat` / `Kit_CocinarMapa.bat` creados como
> primera versión. Falta: (a) crear el **`.umap` plantilla** dentro de `/MapKit/`, y (b) una **corrida de
> validación** del cook (ajustar flags/ruta de salida del `.pak` si hace falta) para probar M1 de punta a punta.
