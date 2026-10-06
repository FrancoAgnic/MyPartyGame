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
