# Game Base — roadmap (rama `game-base`)

Objetivo: una **base técnica online reutilizable**, sacada de sculpturillo (`main`) pero **sin nada de
sculpturillo**. Debe poder: crear sala, que otros se unan, invitar por el juego y por Steam, lobby diegético
(personaje + escena) con **Listo** → countdown → **arranca la partida en otro nivel** (vacío de gameplay).

Regla: forkear juegos NUEVOS desde `game-base`; nunca commitear un juego de vuelta a esta base.

## Fases
- [x] **F0** — rama `game-base` desde `main` (base multiplayer actual).
- [~] **F1 — Funcional primero**: la partida viaja a un `GameLevel` vacío en vez del nivel de esculpir.
  - [x] `PTLobbyGameMode::GameMapPath` → `/Game/Template/levels/GameLevel`.
  - [ ] (editor) crear `Content/Template/levels/GameLevel`: nivel básico con piso + PlayerStart, World
        Settings → GameMode = `MyPartyGameGameMode` (tercera persona del template). Guardar.
  - [ ] probar: crear sala → Listo → arranca en GameLevel con el personaje.
- [ ] **F2 — Pelar el gameplay de esculpir** (`Sculpt/`): reemplazar `PTSculptGameMode` por un GameMode base
      mínimo; borrar SVO, volume, HUD de gameplay de sculpt, y sus referencias.
- [ ] **F3 — Pelar módulos de sculpturillo**: `Tutorial/`, `LocalParty/` (audiencia), `Mods/` (workshop maps),
      `PTWordBank`, locker, head-sculpt del lobby. Desenganchar de `PTGameInstance`.
- [ ] **F4 — Limpiar menús/UI**: dejar crear/unir/código/invitar; sacar Workshop, temáticas, Locker,
      Level Creator, y toda la UI específica de sculpt.
- [ ] **F5 — Invitaciones de Steam nativas**: activar (en sculpturillo estaban planeadas, no terminadas) +
      mantener las in-game.
- [ ] **F6 — Identidad**: nombre del módulo/proyecto, íconos, y limpieza final de `Content`.

## Notas
- Compilar con el editor CERRADO: `Build.bat MyPartyGameEditor Win64 Development -Project=... -WaitMutex`.
- La base online vive en: `Multiplayer/` (MultiplayerSessionsSubsystem + PTSteamWorldwideSearch),
  `PTGameInstance` (parte online), `UI/PTMainMenuWidget` (menús), `Lobby/` (lobby → travel).
