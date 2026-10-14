# Modo clásico con chat del stream

Rama `clasico-con-chat`. El streamer juega el modo clásico con sus amigos (sala online de siempre) y, si prende
el switch, **su chat de Twitch / Kick también participa**: escriben `!unirse` y adivinan desde el chat. La
partida sigue siendo la clásica entre los jugadores; los del chat participan **como espectadores**.

## Cómo se usa

- **Lobby (host):** Game Settings → botón **"Chat del stream: ON/OFF"** (debajo de la Biblioteca) → popup con el
  switch y los canales de Twitch / Kick (los mismos que usa el modo audiencia; se guardan). Muestra si está
  leyendo el chat, cuántos se unieron y la recomendación de la cámara. Los amigos ven "Chat del stream: ON" en su
  panelcito del lobby.
- **Chat:** `!unirse` (o `!join`, `!jugar`, ... en cada idioma) para entrar, `!salir` para salir. Se pueden unir
  ya en el lobby: entran al arrancar la partida. Los que no se unieron no entran al juego.
- **En la partida:** **Shift+1** = chat y tabla de posiciones del juego · **Shift+2** = chat y tabla del stream.
  Las pestañas están arriba del chat (la que no estás mirando cuenta los mensajes sin leer) y la tabla de la
  izquierda muestra una sola tabla a la vez, con su nombre y la tecla para pasar a la otra. La pantalla final
  también: "Resultados" (jugadores) o "Resultados del chat". Al abrir el chat con Enter vuelve al del juego
  (el del stream es solo lectura). Con Shift apretado, los números no cambian de herramienta.
- **Dev:** `PTStreamChat micanal` (prende leyendo ese canal de Twitch), `PTStreamChat off`, `PTStreamChatSetup`
  (abre el popup), `PTChatTab 1|2`. Ojo: `PTStreamChat` guarda el canal en GameUserSettings.

## Reglas

- La partida la mueven **solo los jugadores**: el turno se corta cuando adivinan todos los jugadores (o se acaba
  el tiempo). Los del chat que no llegaron a adivinar esa palabra ya no pueden. Un acierto del chat nunca corta
  ni alarga el turno, y los del chat no cuentan para arrancar la partida.
- Los del chat **solo adivinan** (nunca esculpen). Suman puntos con la misma fórmula que los jugadores, pero **no
  le dan puntos al escultor** (`BP_SculptGameMode` → `bStreamViewersGiveSculptorPoints` para cambiarlo).
- Lo que escriben va a la pestaña del stream con la palabra tapada; sus aciertos aparecen ahí ("adivinó +N").
  Hay tope de líneas por segundo (`StreamFeedMaxLinesPerSec`) para chats muy activos.
- Al final se anuncia al ganador (jugadores) y al mejor del chat.

## La palabra y la cámara del streamer

El chat ve la pantalla del streamer, así que en su pantalla (el host) la palabra nunca aparece donde se ve:

- Arriba a la derecha hay una **zona fija para su cámara** ("Tu cámara va acá"; reemplaza al logo). Cuando le
  toca esculpir, **ahí elige** entre las 3 palabras y **ahí ve la palabra** elegida; arriba al centro solo ve los
  guiones, como cualquiera que adivina. Si adivina la palabra de otro, también la ve solo ahí.
- Al adivinar, el cartel grande dice "¡Adivinaste!" sin la palabra.
- Lo que escribe en el chat sale con puntitos en la caja y en texto normal dentro de su zona.
- Recomendación (está en el popup del lobby): **poner la cámara arriba a la derecha**, tapando esa zona.

## Cómo está armado

- `LocalParty/PTStreamChatSubsystem.*`: en el GameInstance del host. Lee el chat con el mismo `FPTStreamChat` del
  modo audiencia (sin cuentas), lleva la lista de los que se unieron y los engancha a la partida (nunca durante
  un viaje entre mapas).
- `Sculpt/PTSculptGameMode_StreamChat.cpp`: crea / saca sus PlayerStates (sin controller, `StreamPlatform` 1 = Twitch,
  2 = Kick), manda las líneas de su pestaña (`Multicast_StreamChatLine`) y los saca **antes** de cada viaje (el
  motor se lleva todas las PlayerStates: si no, aparecían como jugadores en el lobby).
- `Sculpt/PTChatTabsWidget.*` (pestañas), `Sculpt/PTStreamerZoneWidget.*` (zona de la cámara) y
  `UI/PTStreamChatSetupWidget.*` (popup): UI en C++, sin WBP.
- Los comandos `!unirse` / `!salir` están en `FPTStreamChat` y los usan los dos modos.

## Descartado

- YouTube: se evaluó (API oficial con key desde el relay de la VPS) y se decidió no avanzar.
