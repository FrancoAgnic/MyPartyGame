# Build Battle (prototipo)

Modo de juego nuevo de Sculpturillo, en la rama `build-battle`. El modo clásico (uno esculpe y los demás
adivinan) sigue intacto y es el default.

## Reglas

- **Todos esculpen la misma palabra al mismo tiempo**, cada uno en su propio cubo. Dura el "tiempo de turno"
  del panel de ajustes del host (default 90 s).
- Se acaba el tiempo y **se exhibe cada escultura, de a una, en orden al azar y sin decir de quién es**.
  Todos la votan del **1 al 5** con las teclas 1-5 (Fea / Meh / Bien / ¡Genial! / ¡Obra maestra!). Se puede
  cambiar el voto mientras dure esa escultura. La propia no se vota.
- **Nadie ve los votos hasta que termina la votación de la ronda.** Ahí aparece la tabla (promedio, cantidad
  de votos y puntos) y la ganadora queda en exhibición.
- Puntos de la ronda = promedio / 5 × 100 (5 de promedio = 100 pts). Se juegan las rondas del panel; al final,
  la pantalla de resultados de siempre.

## Cómo activarlo

- **Lobby (host):** Game Settings → botón **"Modo: Clásico / Modo: Build Battle"** (debajo de la Biblioteca).
  Los clientes lo ven en su panelcito ("Rondas: 3 · Modo: Build Battle").
- **Consola (dev, host):** `PTBuildBattle` alterna el modo (en el lobby o en la partida, si está esperando o
  terminó).
- Solo para partidas online/listen server. En modo local/audiencia y en el tutorial siempre es clásico.

### Probar solo / ver una corrida entera

- `PTBBBots 3` agrega 3 bots con cuerpo (flotan alrededor del cubo con su nombre) que esculpen una figura
  cada uno (muñeco de nieve, árbol, hongo, bicho, casita) y votan al azar. Con 1 humano + bots ya arranca.
- `PTBBDemo` (host): tu jugador también esculpe una figura y vota solo. Corrida completa sin tocar nada:
  `-ExecCmds="PTMatchTime 45 3,PTBuildBattle,PTBBDemo,PTBBBots 4"` sobre `Lvl-01?listen` — es lo que hace
  **`SimularBuildBattle.bat`** (doble click).
- `PTMatchTime 30 2` → próxima partida con 30 s de esculpido y 2 rondas.
- `PTBBVote 4` vota por consola; `PTBBScribble` hace unos sellos en tu cubo por la vía normal (para probar en
  red sin mouse).
- Cada máquina loguea su estado en cada cambio de fase (`[BuildBattle][host|cliente] ...`): qué lienzo es el
  suyo, cuál está visible y cuántos vértices tiene cada uno.

## Cómo está armado

- **Un lienzo por jugador**: al empezar la ronda el server spawnea copias de `BP_SculptVolume` **en el mismo
  lugar que el cubo del nivel**, con el controller del jugador como dueño. Cada copia copia en `BeginPlay` la
  config de la instancia del nivel (en server y clientes). Están apiladas: cada máquina muestra solo el que
  corresponde (el tuyo al esculpir, el exhibido al votar) y ninguno tiene colisión.
- El dueño replicado solo resuelve en la máquina de ese jugador → cada uno encuentra su lienzo y la
  exhibición es anónima.
- El esculpido usa **los mismos RPC/multicast de siempre**; el server los redirige al lienzo del que los manda
  (`ResolveServerVolume`). En Build Battle "todos son escultores" mientras dura la fase de esculpir.
- Fases nuevas en `EPTTurnPhase`: `Showcase` y `RoundResults` (se esculpe en `Drawing`).
- Código: `Sculpt/PTSculptGameMode_BuildBattle.cpp` (reglas), `Sculpt/PTSculptPlayerController_BuildBattle.cpp`
  (lienzo local, teclas de voto, comandos dev), `Sculpt/PTBuildBattleWidget.*` (UI, en C++ sin WBP).
  Tiempos y puntos editables en `BP_SculptGameMode` → categoría **Game|BuildBattle**.

## Limitaciones conocidas (para iterar)

- **Costo**: todas las máquinas reciben y mallan los sellos de todos (los lienzos ocultos igual se
  actualizan). Con 4-6 jugadores debería andar; con más habría que mandar cada lienzo solo a su dueño y
  sincronizar el resultado al exhibir.
- Quien entra **a mitad de ronda** no tiene lienzo hasta la próxima (mira y vota) y no recibe lo ya esculpido
  de los demás en esa ronda.
- Votar es solo con teclado (1-5 / numpad); falta joystick.
- Durante la exhibición la cámara es libre (cada uno mira la escultura desde donde está); no hay cámara
  orbital automática.
- La palabra es al azar (nadie elige); podría ofrecerse una votación de tema.
