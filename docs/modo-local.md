# Modo local (celulares + joystick)

Una sola PC conectada a la TV y **un joystick que se pasan** los jugadores para esculpir. Cada jugador
entra desde el **navegador de su celular**, sin instalar nada:

- quien esculpe ve la palabra (y elige entre 3) en su celular;
- los demás adivinan escribiendo en el suyo;
- la TV muestra la escultura, el marcador y el chat, **nunca** la palabra.

## Cómo se juega

1. Menú principal → **Jugar** → **Modo local (celulares)**. También sirve el comando de consola `PTLocal`.
2. La TV muestra un **QR** y una dirección (`http://192.168.x.x:8787`). Los celulares tienen que estar
   en el **mismo WiFi** que la PC.
3. Cada uno escribe su nombre. El primero que entra es el **anfitrión (★)**: toca *Empezar partida*
   cuando están todos. Mínimo 2 jugadores (`LocalPartyMinPlayers` en `BP_SculptGameMode`).
4. En cada turno, la TV dice *"¡Pásale el joystick a X!"* y el celular de X vibra y muestra las 3 palabras.
5. Al final, el anfitrión elige *Jugar de nuevo* o *Volver al menú* desde su celular.

Si un celular se bloquea o se corta el WiFi, se reconecta solo y recupera el mismo jugador con su
puntaje. Si no vuelve en 90 s (20 s en la sala de espera), sale de la partida.

## Controles del joystick (Xbox)

| Botón | Acción | Equivale a |
|---|---|---|
| Stick izquierdo | Moverse / volar | WASD |
| Stick derecho | Mirar | Mouse |
| RT | Esculpir | Click izquierdo |
| LB / RB | Achicar / agrandar el pincel (mantener = repetir). Con la rueda de color abierta, cambian el brillo. Con el radial de formas abierto, cambian de página | Rueda del mouse |
| Cruceta ↑ → ↓ ← | Agregar / Borrar / Pintar / Ojos | 1 2 3 4 |
| X (mantener) | Rueda de color: el stick elige el color, soltar X confirma | Click derecho |
| Y (mantener) | Radial de formas: el stick elige la forma, soltar Y confirma | Tab |
| A / B (mantener) | Subir / bajar | Espacio / Ctrl |
| LT (mantener) | Pegar el sello a la superficie | Alt |
| L3 (mantener) | Plano vertical | Z |
| R3 (mantener) | Rotar la forma con el stick derecho | Rueda apretada |
| View | Deshacer (toque) / borrar todo (mantener 3 s) | Backspace |
| Menu | Pausa | Esc |

El joystick también funciona en partidas online. La sensibilidad se ajusta en `BP_SculptPlayerController`,
categoría *Input | Gamepad*.

## Arquitectura

| Pieza | Archivo |
|---|---|
| Servidor HTTP + WebSocket mínimo (sockets no bloqueantes, game thread, sin dependencias) | `Source/MyPartyGame/LocalParty/PTLocalPartyServer.*` |
| Subsistema del GameInstance: jugadores del celular, tokens de reconexión y estado por celular | `LocalParty/PTLocalPartySubsystem.*` |
| Overlay de la TV (QR, lista, "pásale el joystick", ayuda de controles), 100% C++ | `LocalParty/PTLocalPartyTVWidget.*` |
| Lector del chat de Twitch / Kick (modo audiencia) | `LocalParty/PTStreamChat.*` |
| Modal "Links de la partida" (audiencia): separa el link PRIVADO del streamer del link para el chat | `LocalParty/PTStreamerLinkModal.*` |
| Generador de QR (modo byte, ECC M, v1–10; verificado contra la librería `qrcode` de Python) | `LocalParty/PTQRCode.*` |
| Página del celular (HTML/JS en un solo archivo, 6 idiomas) | `Content/LocalParty/Web/index.html` |
| Joystick del escultor | `Sculpt/PTSculptPlayerController_Gamepad.cpp` |

- Cada jugador del celular es un **`APTPlayerState` sin controller** (`bIsPhonePlayer`), creado por
  `APTSculptGameMode::LocalParty_AddPlayer`. El GameMode lo trata como a cualquier jugador (turnos,
  puntaje, chat). Lo que antes viajaba por RPC a su controller (las opciones, la palabra, "adivinaste",
  "casi") le llega al celular por el subsistema.
- La PlayerState de la PC queda marcada `bIsLocalPartyTV`: no juega ni aparece en el marcador, y
  `APTSculptGameState::IsActingSculptor` la deja **esculpir en nombre** del escultor de turno. Esto
  vale para las validaciones del servidor y para el cubo de esculpido.
- Protocolo (JSON por WebSocket en `/ws`):
  - Del celular a la PC: `join{name,token,lang}`, `guess{text}`, `choose{i}`, `start`, `again`,
    `menu` y `ping`.
  - De la PC al celular: `welcome`, `error{code}`, `state` (personalizado: solo el escultor recibe
    `choices`/`word`), `chat`, `guessed`, `close` y `buzz`.
- El puerto por defecto es **8787**; si está ocupado prueba 8788…8791.

## Modo audiencia: chat de Twitch / Kick

En el modo audiencia la gente también puede jugar **desde el chat del stream**, sin celular:

1. En el panel de configuración de la TV, sección *Chat del stream*, el streamer escribe su canal de
   **Twitch** y/o **Kick** (sirve el nombre o el link entero) y aprieta Enter. Se recuerda para la próxima.
2. Quien escribe **`!unirse`** en el chat entra como jugador con su nombre del chat (también `!join`,
   `!entrar`, `!jugar`, `!beitreten`, `!rejoindre`, `!unisciti`… el comando define su idioma).
   **`!salir`** / `!leave` lo saca.
3. Mientras se esculpe, cada mensaje suyo es un intento (máximo uno por segundo). Los mensajes comunes
   no se repiten en el chat de la TV: el stream ya muestra su chat.
4. Comparten ranking (y personajes) con los que juegan por celular. Tope: 150 jugadores del chat.

**Puntaje en audiencia** (celular y chat): por orden de acierto, 100 · 50 · 25 · 20 · 20… (cada uno la
mitad del anterior, con piso de 20) y la **última palabra vale doble**. Así nadie se escapa: el que
copia del chat suma poco, y el final queda abierto. Se ajusta en `BP_SculptGameMode`
(`AudienceFirstGuessPoints`, `AudienceMinGuessPoints`, `AudienceLastWordMultiplier`).

Cómo funciona: `LocalParty/PTStreamChat.*` **solo lee**, sin cuentas ni contraseñas:

- Twitch: IRC por WebSocket (`wss://irc-ws.chat.twitch.tv`) con un usuario anónimo `justinfanNNNNN`.
- Kick: busca el chatroom en `https://kick.com/api/v2/channels/<canal>` y escucha el WebSocket público
  (Pusher) que usa la misma página de kick.com. No es una API oficial: si Kick la cambia, se puede
  apuntar a otra sin recompilar con `KickApiUrl` / `KickPusherUrl` (y `TwitchIrcUrl`) en
  `[LocalParty]` de `DefaultGame.ini`, **entre comillas**.
- Cada persona del chat es un `FPTPhonePlayer` con `ChatPlatform` 1 (Twitch) o 2 (Kick), sin celular.
- Consola: `PTChat twitch <canal>` / `PTChat kick <canal>` (sin canal = apagar).

## Menús con joystick (toda la UI)

`UPTGamepadUINavigator` (`Source/MyPartyGame/UI/`) maneja **todos** los menús con joystick sin tocar
los WBP. Mientras hay un menú en pantalla (cursor visible):

| Botón | Acción |
|---|---|
| Cruceta (o stick izquierdo, si el personaje no puede caminar) | Mover el foco (borde dorado) |
| A | Activar: botón, checkbox o combo. En un cuadro de texto, escribir con el teclado |
| ← / → o LB / RB | Ajustar sliders y combos |
| B | Volver: aprieta el botón *Back / Cerrar / Cancelar* visible o, si no hay, simula Esc |
| Stick derecho | Scrollear listas |
| Y (en Ajustes) | Abrir el panel *Joystick* |
| Menu | Pausa (pasa directo al juego) |

- **Controles tapados:** solo se navega a los que están realmente arriba; un hit-test de Slate
  descarta lo que tapa un popup.
- **Para que B funcione:** un botón de "volver" tiene que llamarse con algo como *Back*, *Close*,
  *Cancel* o *Resume*.

## Panel "Joystick"

`UPTGamepadSettingsWidget` permite ajustar:
- sensibilidad de la cámara;
- velocidad al moverse;
- zona muerta del stick;
- invertir el eje Y;
- **reasignar cada acción**: A sobre la acción y después el botón nuevo; si ese botón ya lo usaba
  otra acción, se intercambian.

Se guarda en `GameUserSettings.ini`, y los cambios valen al instante, incluso en plena partida.
Se abre desde Ajustes (botón `GamepadButton` o Y) o con el comando `PTJoystick`.

**Para probar sin joystick:** lanza con `-PTCmdFile` y escribe comandos en `Saved/PTCommands.txt`,
por ejemplo `PTPadKey Gamepad_DPad_Down`. Esto queda desactivado en Shipping.

## Links en modo audiencia

- El link del streamer (`?h=CLAVE`) es privado: con él cualquiera entra como streamer. Solo aparece en la
  zona privada roja del panel de configuración y NUNCA se muestra escrito.
- "Copiar link" del panel del streamer no copia directo: abre `UPTStreamerLinkModal` (Z 500) con el
  aviso, "Copiar link para el chat" (`GetJoinUrl()`, recibe el foco del joystick) y "Copiar link del
  streamer (privado)" (`GetHostJoinUrl()`). B cierra (botón `CloseLinkModalButton`). Se cierra solo si el
  panel de configuración se oculta (pausa, banco de palabras, Workshop, arranque).
- Debajo del QR público de la TV hay un botón "Copiar link para el chat" (en modo local: "Copiar link").
  El overlay de la TV es todo HitTestInvisible salvo ese botón (y sus contenedores, SelfHitTestInvisible).
