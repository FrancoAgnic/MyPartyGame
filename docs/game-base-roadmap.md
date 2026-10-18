# Game Base — roadmap (rama `game-base`)

Objetivo: un **TEMPLATE / base técnica online reutilizable**, forkeada de sculpturillo (`main`). Conserva
TODO el código (workshop, locker, tutorial, modo local, head-sculpt, etc.) pero **arranca limpio** como pura
base online: crear sala, que otros se unan, invitar por el juego y por Steam, lobby diegético (personaje +
escena) con **Listo** → countdown → **arranca la partida en otro nivel** (`GameLevel` vacío). Cada juego que
forkee esta base PRENDE/REEMPLAZA las piezas que necesite.

Enfoque elegido: **B (ocultar por comportamiento, no borrar)**. Las features de sculpturillo quedan en el
proyecto detrás de flags, apagadas por defecto.

## Fases
- [x] **F0** — rama `game-base` desde `main`.
- [x] **F1** — la partida viaja a `GameLevel` en vez del nivel de esculpir (`PTLobbyGameMode::GameMapPath`).
  - [ ] (editor) crear `Content/Template/levels/GameLevel`: piso + PlayerStart, World Settings → GameMode =
        `MyPartyGameGameMode`. Guardar.
- [x] **F2 — Base arranca limpia (flags)**:
  - [x] Menú: `bEnableSculpturilloFeatures=false` (default) oculta Locker/Workshop/Level Creator/Modo local/
        Modo online/Tutorial. Quedan Jugar/Crear/Unirse/Código/Opciones (+Invitar en el lobby).
  - [x] Boot: `bEnableTutorialOnFirstRun=false` (default) → primer arranque va directo al menú (sin tutorial).
  - [x] Compila sin borrar nada (todo el código sigue disponible para reactivar por juego).
- [ ] **F3 — Steam invites nativas**: activar las invitaciones por overlay de Steam (en sculpturillo estaban
      planeadas) + mantener las in-game. (Opcional según el juego.)
- [ ] **F4 — Identidad**: cuando se forkee un juego concreto, renombrar módulo/proyecto e íconos, y prender/
      reemplazar las features (workshop/locker/tutorial/gameplay) con las de ESE juego.

## Cómo forkear un juego nuevo desde esta base
1. Rama nueva desde `game-base`.
2. Poner el gameplay del juego en `GameLevel` (o un nivel nuevo) + su GameMode.
3. Prender las features que reuses: `bEnableSculpturilloFeatures` (menú), `bEnableTutorialOnFirstRun` (boot),
   o reemplazar esas piezas por las del juego.
4. Nunca commitear el juego de vuelta a `game-base`.

## Notas
- Compilar con el editor CERRADO: `Build.bat MyPartyGameEditor Win64 Development -Project=... -WaitMutex`.
- Online base: `Multiplayer/` (sessions + Steam search), `PTGameInstance` (online+invites),
  `UI/PTMainMenuWidget` (menús), `Lobby/` (lobby → travel a GameLevel).
