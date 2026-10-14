# Modo clásico con chat del stream

Rama `clasico-con-chat`. El streamer juega el modo clásico con sus amigos (sala online de siempre) y, si prende
el switch, **su chat de Twitch / Kick también juega**: escriben `!unirse` y adivinan desde el chat.

## Cómo se usa

- **Lobby (host):** Game Settings → botón **"Chat del stream: ON/OFF"** (debajo de la Biblioteca) → popup con el
  switch y los canales de Twitch / Kick (los mismos que usa el modo audiencia; se guardan). Muestra si está
  leyendo el chat y cuántos se unieron. Los amigos ven "Chat del stream: ON" en su panelcito del lobby.
- **Chat:** `!unirse` (o `!join`, `!jugar`, ... en cada idioma) para entrar, `!salir` para salir. Se pueden unir
  ya en el lobby: entran al arrancar la partida. Los que no se unieron no entran al juego.
- **En la partida:** **Shift+1** = chat del juego (amigos) · **Shift+2** = chat del stream. Las pestañas están
  arriba del chat y la que no estás mirando cuenta los mensajes sin leer. Al abrir el chat con Enter vuelve al
  del juego (el del stream es solo lectura). Con Shift apretado, los números no cambian de herramienta.
- **Dev:** `PTStreamChat micanal` (prende leyendo ese canal de Twitch), `PTStreamChat off`, `PTStreamChatSetup`
  (abre el popup), `PTChatTab 1|2`. Ojo: `PTStreamChat` guarda el canal en GameUserSettings.

## Reglas

- Los del chat **solo adivinan** (nunca esculpen) y no cuentan para arrancar la partida.
- Sus aciertos suman puntos con la misma fórmula que los amigos, pero **no le dan puntos al escultor**
  (con muchos espectadores se dispararía; se puede cambiar en `BP_SculptGameMode` → `bStreamViewersGiveSculptorPoints`).
- El turno se corta antes cuando adivinan **todos los amigos** (el chat no frena ni apura). Si el streamer juega
  solo con su chat, se corta cuando adivinan todos los del chat.
- Lo que escriben va a la pestaña del stream con la palabra tapada; sus aciertos aparecen ahí ("adivinó +N") y
  no en el chat del juego. Hay tope de líneas por segundo (`StreamFeedMaxLinesPerSec`) para chats muy activos.
- Marcador: los amigos como siempre y abajo "Chat del stream (N)" con los 5 mejores. Al final se anuncia el mejor
  del chat además del ganador.

## Cómo está armado

- `LocalParty/PTStreamChatSubsystem.*`: en el GameInstance del host. Lee el chat con el mismo `FPTStreamChat` del
  modo audiencia (sin cuentas), lleva la lista de los que se unieron y los engancha a la partida.
- `Sculpt/PTSculptGameMode_StreamChat.cpp`: crea / saca sus PlayerStates (sin controller, `StreamPlatform` 1 = Twitch,
  2 = Kick) y manda las líneas de su pestaña (`Multicast_StreamChatLine`).
- `Sculpt/PTChatTabsWidget.*` (pestañas) y `UI/PTStreamChatSetupWidget.*` (popup): UI en C++, sin WBP.
- Los comandos `!unirse` / `!salir` ahora están en `FPTStreamChat` y los usan los dos modos.

## YouTube (pendiente, se puede)

- Oficial: YouTube Data API con una **API key** (Google Cloud, gratis). Con el link del directo se saca el
  `liveChatId` (`videos.list`, 1 unidad) y se abre `liveChatMessages.streamList`, que **empuja** los mensajes por una
  conexión abierta (5 unidades por conexión). La cuota por defecto es 10.000 unidades/día, compartida por todos los
  streamers que usen el juego: alcanza para cientos de directos por día si no se reconecta todo el tiempo.
- La key no debería ir dentro del juego (cualquiera la saca y gasta la cuota): el lector iría en el **relay de la
  VPS** (`Tools/Relay`), que ya tiene el modo audiencia, y le pasaría los mensajes al juego.
- Alternativa sin key: la API interna de youtube.com (como `pytchat` / `youtube-chat`). Sin cuota, pero no es
  oficial y se puede romper en cualquier momento; no la recomiendo para un juego en Steam.
