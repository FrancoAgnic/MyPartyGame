# Diseño de Modo de Juego 2 — "Sillas Musicales Inversas" (título pendiente)
### Documento de brainstorming · v0.1 · 06-ago-2026
**Estado:** IDEA en exploración — nada de esto está decidido ni en producción. Salió de una sesión de brainstorming entre Deivor y Claude (06-ago-2026). Cuando el equipo cierre decisiones, numerarlas como M1, M2… (para no chocar con las D del Modo 1).
**Relación con el Modo 1 (`diseno-modo-sillas.md`):** comparte ~90% del esqueleto técnico ya construido — roles replicados, fases Música/Silencio, la mecánica de sentarse sobre un jugador, máquina de rondas y balance en DataAsset. Un prototipo greybox sería un GameMode hermano + IA simple de huida para bots.

---

## 1. Concepto (idea original de Deivor)

**El juego de la silla clásico, pero las sillas son jugadores.**

- Bandos: CAZADORES (los que se sientan) y SILLAS (jugadores que escapan). Polaridad inversa al Modo 1.
- **Mientras suena la música:** todos corren — las sillas rápidas, los cazadores más lentos (nadie puede sentarse todavía).
- **Cuando la música se corta:** las velocidades se igualan (el cazador queda *apenas* más rápido que la silla) y cada cazador debe alcanzar y sentarse en una silla-jugador. Una silla admite un solo cazador.
- Regla sagrada del juego de la silla: **sillas = cazadores − 1**. Siempre sobra exactamente un cazador sin asiento.
- El match corona al "último en pie", como en el patio de la escuela.

## 2. Los dos problemas detectados (y sus soluciones propuestas)

### Problema A — Hacen falta ~22 jugadores para que se sienta como el juego real

1. **Bots-relleno**: sillas NPC con IA simple de huida ("señuelos que corren"). Con 6-8 humanos + 12-15 bots se logra la densidad del patio. Bonus enorme: si las sillas humanas son visualmente idénticas a los bots, reaparece el **camuflaje conductual** del Modo 1 ("¿esa silla corre como bot o como persona?").
2. **Densidad por espacio**: arena chica o que se achica por ronda — 8 jugadores en un pasillo se sienten como 22 en un gimnasio.
3. Límite técnico actual: el template topea en 10 jugadores (`MaxPlayersAllowed`); 22 humanos en listen server es otra categoría de problemas de red. Los bots son el camino barato.

### Problema B — Los eliminados se aburren espectando

La primera versión del flujo (todos los humanos arrancan cazadores; el que sobra pasa a silla y ahí se queda) tiene un bug matemático: las sillas humanas **se acumulan** y la regla `sillas = cazadores − 1` expulsa gente sin que haya perdido. Trace con 6 humanos + 5 sillas bot:

| Ronda | Cazadores | Sillas (caz−1) | Composición sillas |
|---|---|---|---|
| 1 | 6 humanos | 5 | 5 bots |
| 2 | 5 humanos | 4 | 3 bots + 1 humano |
| 3 | 4 humanos | 3 | 1 bot + 2 humanos |
| 4 | 3 humanos | 2 | **−1 bot: imposible** — sobra un humano sin perder |

**Solución propuesta: ROTACIÓN DOBLE.**
- El cazador que sobra pasa a silla (castigo con rol, no eliminación).
- La silla humana que fue "usada" (se sentaron en ella) **vuelve a ser cazador** la ronda siguiente.
- Neto: los humanos *circulan* entre bandos y nunca se acumulan; el achique de la regla clásica lo absorben **solo los bots** (cada ronda se retira una silla-bot, jamás un humano).
- Resultado: **nadie especta nunca** durante esta fase, con 6 o con 10 humanos.

## 3. Estructura de match propuesta: liga + muerte súbita

**Fase liga (sin eliminación):** rondas cortas con rotación doble y puntos (reciclando la filosofía D7b del Modo 1): sentarse rápido puntúa, sobrevivir como silla sin ser usada puntúa, sobrar resta. Los bots van bajando ronda a ronda.

**Fase muerte súbita (cuando se agotan los bots o tras X rondas):** los mejores puntuados pasan a eliminación clásica — el que sobra queda fuera, ronda a ronda, hasta el campeón. Dura 3-4 rondas de ~60 s, así el peor caso de espera son un par de minutos… y ni siquiera es espera pasiva:

- **El DJ**: el primer eliminado decide *cuándo* corta la música, dentro de una ventana que le da el server. Poder inmenso, casi sin código.
- **Sillas-troll**: los siguientes eliminados **poseen sillas-bot** — no pueden ganar, pero esquivan culos, bloquean pasillos y le arruinan la ronda a su verdugo. La venganza como premio consuelo.

## 4. Preguntas abiertas del modo (sin numerar hasta que se decidan)

- ¿La silla puede **esquivar en el último instante** cuando el cazador ya inició el sentado? (sería el "esquive de cola" del Modo 1 con la polaridad invertida — los dos modos compartirían el clip estrella).
- ¿El corte de música es **predecible** (D3 del Modo 1) o **sorpresivo**? La sorpresa parece más fiel al juego de la silla original; el DJ eliminado empuja hacia lo sorpresivo.
- ¿Los cazadores **saben** qué sillas son bots y cuáles humanas? Si no lo saben: ¿sentarse en bot vale menos puntos que cazar humana, o al revés?
- ¿Qué pasa exactamente con la silla usada durante la ronda (queda "aplastada" hasta el fin de ronda, o sigue moviéndose)?
- Números de balance: velocidades (silla rápida / cazador lento en música; cazador +5-10% en silencio), duración de ronda, curva de retiro de bots, economía de puntos.
- Nombre del modo (esperar a P14/P16 del Modo 1 — probablemente compartan tema).

## 4b. Variante C — Inversión directa del Modo 1 (idea de Deivor, 06-ago-2026)

Sin bots y sobre el casting del Modo 1, invirtiendo la infección:

- Arranca **1 SILLA vs. N−1 CAZADORES** (el espejo exacto del 1 cazador vs. N−1 sillas).
- **El primer cazador que logra sentarse** (capturar a la silla) **asciende: pasa a ser silla** la ronda siguiente. Las sillas se ACUMULAN en vez de los cazadores: 1→2→3… sillas mientras los cazadores se achican.
- Con varias sillas vivas, los cazadores compiten entre sí por ser el primero en sentarse cada ronda (solo asciende uno por ronda — o uno por silla capturada, a decidir).
- **Pierde el único cazador que nunca consiguió sentarse** en todo el match (el perdedor único, al revés del ganador único del Modo 1).
- **Las sillas puntúan por sobrevivir**: si en alguna ronda nadie logró sentarse, ganan las sillas con más puntos — el mismo D7b reciclado con la polaridad opuesta.
- Nadie especta: cazador que asciende sigue jugando como silla; sillas juegan hasta el final.
- Atractivo extra: se puede ofrecer como **opción del lobby del mismo Modo 1** (mismo mapa, mismas mecánicas de captura/baile/audio — solo cambia la dirección de la conversión y el casting inicial). Candidato a implementarse como variante del GameMode actual, no como juego aparte.

Preguntas abiertas de la variante: ¿la silla capturada vuelve a cazador (rotación) o las sillas nunca bajan? ¿el baile aplica a todos los cazadores a la vez (mucha ceguera simultánea)? ¿cómo escala la intensificación D12 cuando las sillas CRECEN en vez de caer?

## 5. Nota técnica (para cuando se prototipe)

- GameMode hermano (`ASillasInversasGameMode` o similar) sobre el mismo esqueleto: roles, fases, sentado server-authoritative, BalanceData propio.
- Lo único genuinamente nuevo: IA de huida para sillas-bot (huir del cazador más cercano + ruido), posesión de bots por eliminados, y el rol DJ.
- Prototipable en greybox después de la Fase 5 del Modo 1 (cuando exista el loop de match completo para heredar).
