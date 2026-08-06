# Diseño de Modo de Juego 3 — "El juego de la silla sin bots" (título pendiente)
### Documento de brainstorming · v0.1 · 06-ago-2026
**Estado:** IDEAS en exploración — nada decidido ni en producción. Sesión de brainstorming Deivor + Claude (06-ago-2026). Cuando el equipo cierre decisiones, numerarlas como S1, S2… (las D son del Modo 1, las M del Modo 2).
**Premisas de Deivor:** juego de la silla clásico viable **100% con jugadores (sin bots)**, 6-10 personas, y con una vuelta de tuerca real — no el Modo 1 invertido (crítica válida al Modo 2, `diseno-modo-sillas-inversas_generado-por-Claude.md`).

---

## 1. El problema a resolver

El juego de la silla elimina 1 jugador por ronda y la gracia depende de la multitud. Con 6-10 humanos y sin bots hay que responder dos cosas a la vez:
1. ¿De dónde sale la **escasez** (la regla `asientos = jugadores − 1`) sin relleno artificial?
2. ¿Qué hace el **eliminado** para no espectar?

Las dos ideas siguientes responden ambas, por caminos opuestos: una convierte al eliminado en **terreno**, la otra lo convierte en **pasajero**.

---

## 2. Idea A — "EL PARLANTE" (semilla de Deivor: el perdedor se vuelve parlante musical)

### Concepto
- Roles: CAZADOR(es), SILLAS (jugadores) y PARLANTES (los eliminados).
- El eliminado se convierte en **parlante ambulante**: la única fuente de música y de refugio del mapa. Camina lento, se posiciona donde quiere.
- **La vuelta de tuerca matemática:** las "sillas" del juego clásico pasan a ser los **cupos de protección de los parlantes**. Cada parlante protege un número limitado de sillas (1-2 cupos). El server reparte los cupos entre los parlantes vivos de modo que `cupos totales = sillas vivas − 1` — la escasez clásica, garantizada con cualquier cantidad de humanos. La crean los propios jugadores, no un casting fijo ni bots.

### Flujo de ronda
1. **Música** (espacial: emana de los parlantes): las sillas se reposicionan; el cazador no puede capturar.
2. **Corte**: dos carreras SIMULTÁNEAS —
   - Las sillas corren a **reclamar un cupo** de parlante (esta es la silla musical clásica: N sillas, N−1 cupos).
   - El cazador caza a la silla que quedó sin cupo (mecánica de captura estilo Modo 1 o persecución).
3. La cazada se convierte en parlante → más refugios pero menos sillas: los cupos se recalculan (siempre sillas−1).

### El contra-juego del cazador
- Puede **DESENCHUFAR un parlante**: interacción mantenida, lenta y telegrafiada (el equivalente dramático de la caminata de cola). El parlante muteado deja de proteger por X segundos → sus protegidas quedan expuestas de golpe.
- El parlante puede huir (lento) o gritar/avisar. Decidir si desenchufar tiene cooldown o costo.

### Por qué los eliminados no se aburren
- Los parlantes **eligen a quién proteger**: negocian, traicionan, cobran favores — gameplay social puro.
- Puntúan (filosofía D7b): proteger a la ganadora paga, apostar al protegido correcto paga. Siguen compitiendo por el match.
- El mapa de zonas seguras lo **dibujan los perdedores** cada ronda: cuantas más rondas pasan, más denso y político se vuelve el tablero.

### Matemática con 8 jugadores (ejemplo)
| Ronda | Cazadores | Sillas | Parlantes | Cupos (sillas−1) |
|---|---|---|---|---|
| 1 | 1 | 7 | 0 → usar 1 "parlante de arranque" (objeto fijo del mapa, no bot) | 6 |
| 2 | 1 | 6 | 1 | 5 |
| 3 | 1 | 5 | 2 | 4 |
| … | 1 | 2 | 5 | 1 |
- La última silla con cupo gana la ronda/match. Ajustes abiertos: ¿el cazador también rota? ¿2 cazadores con 9-10 jugadores?

---

## 3. Idea B — "LA SILLA SIAMESA" (semilla de Deivor: compartir la silla / el control)

### Concepto
- Juego de la silla clásico con sillas-prop (acá los jugadores son todos "sentadores", como en el patio).
- **La vuelta de tuerca: perder no elimina — FUSIONA.** El que queda sin silla se sube a hombros de otro jugador y desde entonces **comparten el control del personaje**.
- Esquemas de control compartido (a decidir, ambos probados en party games):
  - **Ejes repartidos**: uno maneja adelante/atrás, el otro izquierda/derecha (el clásico "dos personas, un control").
  - **Suma vectorial**: el movimiento es la suma de los inputs; si no acuerdan, el personaje tiembla y no avanza. Máximo caos, máxima risa.
- Cada ronda se retira una silla-prop → una fusión nueva por ronda.

### Matemática con 8 jugadores (perfecta sin bots, nadie sale nunca)
| Ronda | Entidades | Sillas-prop | Resultado |
|---|---|---|---|
| 1 | 8 solos | 7 | 1 fusión |
| 2 | 7 (una dual) | 6 | … |
| … | … | … | … |
| final | 2 torres (p.ej. 4+4 pilotos) | 1 | la torre ganadora |
- `asientos = entidades − 1` se cumple siempre; los 8 humanos juegan hasta el último segundo.
- **Ganador real**: el que llega a la final sin haberse fusionado nunca (puntos por rondas sobrevividas en soledad); la torre ganadora reparte puntos menores entre pilotos.

### Decisiones abiertas propias
- ¿El fusionado elige a quién subirse, o le toca el que le ganó la silla (venganza incorporada)?
- ¿Las torres más altas tienen hitbox mayor / se ven de lejos (contrapeso natural)?
- ¿Habilidad extra del pasajero (empujar, taunt) para que el de arriba tenga agencia propia?

---

## 4. Híbrido posible (una tuerca más)

En "El Parlante", cuando el cazador desenchufa un parlante, sus protegidas no mueren: **se fusionan con el parlante** (idea B dentro de idea A). Compatible porque atacan cosas distintas: el parlante redefine EL ESPACIO, la fusión redefine EL CUERPO.

---

## 5. Comparación rápida de los 3 modos

| | Modo 1 (en producción) | Modo 2 (inversas) | Modo 3A Parlante | Modo 3B Siamesa |
|---|---|---|---|---|
| Escasez | señuelos estáticos | sillas = caz−1 (bots absorben) | cupos de parlante = sillas−1 | sillas-prop = entidades−1 |
| Eliminado | se infecta (cazador) | rota / trollea bots | se vuelve terreno (refugio) | se vuelve pasajero (co-control) |
| Bots | no | sí (relleno) | **no** | **no** |
| Jugadores | 2-8 | ideal 22 (bots) | 6-10 | 6-10 |

## 6. Nota técnica
- Ambas ideas reutilizan el esqueleto del Modo 1 (roles, fases, sentado server-authoritative, BalanceData).
- Lo nuevo de A: sistema de cupos/protección + interacción de desenchufe + música espacial por actor (ya existe la base de audio de Fase 4).
- Lo nuevo de B: posesión compartida de un pawn (input de 2+ controllers sobre un Character) — el desafío técnico más interesante; prototipable con suma vectorial en ~un día sobre el pawn actual.
- Prototipar recién después de la Fase 5 del Modo 1.
