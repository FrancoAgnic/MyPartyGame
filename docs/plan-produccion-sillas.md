# PLAN DE PRODUCCIÓN — "Sillas + Escondite" (título pendiente)
### Para desarrollo en C++ · Unreal Engine 5.8 · con Claude Code
**Base de partida:** repositorio template propio con estructura completa de multiplayer Steam (listen server + sessions).
**Documento hermano:** `diseno-modo-sillas.md` (decisiones D1–D17 y preguntas abiertas). Este plan implementa ESE diseño.

> **Nota para Claude Code:** este plan fue escrito con conocimiento de UE hasta enero 2026. UE 5.8 puede haber cambiado APIs (especialmente en Enhanced Input, animación y audio). Ante cualquier duda, verificar contra el código fuente del engine instalado y la documentación local antes de asumir firmas de funciones.

---

## Resumen del juego (contexto mínimo para cada sesión de Claude Code)

Party game 2–8 jugadores, listen server Steam. Un CAZADOR busca a las SILLAS (jugadores camuflados entre 15–25 sillas señuelo). Cuando suena la música (fase corta, ~10s) el cazador baila con cámara limitada y no puede capturar; en el silencio (fase larga, ~20s) caza. Captura = "caminata de cola": clic mantenido, rota 90°, camina de cola y se sienta — la silla-jugador se rompe (eliminada); el señuelo es sólido (cazador adolorido 1.8s). Eliminados se vuelven cazadores (infección). Gana la última silla viva; el match son 5 rondas con puntos (supervivencia por fase + capturas + bonus último vivo). El cazador caza con oído (respiración por proximidad) y memoria. Patrón de música fijo y conocido, que se intensifica al caer sillas.

---

## FASE 0 — Fundación del proyecto (integración del template)
**Objetivo:** proyecto UE 5.8 compilando con el template de multiplayer integrado y un mapa greybox de pruebas.

- [ ] Clonar template multiplayer → proyecto nuevo. Migrar a 5.8 si el template es de una versión anterior; compilar y resolver deprecations.
- [ ] Verificar en 5.8: creación de sesión Steam, join, listen server con 2+ clientes (usar el flujo ya existente del template, no reescribir).
- [ ] Estructura de módulos/carpetas C++ del gameplay nuevo (ej. módulo `SillasCore`): convenciones de nombres, prefijos.
- [ ] Clases esqueleto del modo: `ASillasGameMode`, `ASillasGameState`, `ASillasPlayerState`, `ASillasPlayerController`.
- [ ] Mapa greybox `L_TestArena`: sala rectangular, obstáculos simples, 20 puntos de spawn de sillas.
- **Criterio de hecho:** 3 clientes conectados al listen server caminando por el greybox con un pawn placeholder.
- **Cubre:** infraestructura. **Riesgo:** incompatibilidades del template con 5.8 — resolver acá, no después.

## FASE 1 — Máquina de estados y roles (el esqueleto del modo)
**Objetivo:** el flujo de ronda existe y se replica; hay sillas y cazador aunque nada sea divertido todavía.

- [ ] `ASillasGameState`: máquina de estados replicada — `Lobby → IntroRonda → Musica → Silencio → FinRonda` con timers server-authoritative (D3: patrón fijo; D11: música ~10s / silencio ~20s **como config, no hardcodeado**).
- [ ] **Todos los números de balance en un `UDataAsset` de configuración** (`USillasBalanceData`): duraciones de fase, velocidad caminata de cola, distancia de sentado, 1.8s de dolor, curva de intensificación D12. El playtest vive de tocar estos números sin recompilar.
- [ ] Roles: `ESillasRole {Silla, Cazador}` en PlayerState, replicado. Asignación aleatoria del cazador inicial (D7) y cantidad inicial configurable por el host en el lobby (D8, con default 1 para 2–5 jugadores / 2 para 6–8).
- [ ] Pawn Silla v0 (mesh de silla placeholder, caminar) y Pawn Cazador v0 (mannequin, caminar). Posesión según rol al iniciar ronda.
- [ ] Spawn de 15–25 señuelos (D9) idénticos al mesh de las sillas-jugador, posiciones desde puntos del mapa con jitter.
- **Criterio de hecho:** ronda completa corre sola con fases alternando en los 3 clientes, roles asignados, sillas mezcladas con señuelos indistinguibles a la vista.
- **Cubre:** D3, D7, D8, D9, D11 (estructura).

## FASE 2 — LA CAMINATA DE COLA (la firma del juego — máxima prioridad de calidad)
**Objetivo:** la mecánica de captura completa y satisfactoria. Esta fase decide si el juego funciona.

- [ ] Input clic mantenido (Enhanced Input): el cazador entra en modo captura — rotación 90°, caminata de cola (velocidad desde BalanceData), soltar cancela. Solo disponible en fase Silencio (D5 + D11).
- [ ] Detección de "sentado válido": distancia/ángulo al asiento objetivo (server-authoritative con predicción básica en cliente; la validación de captura SIEMPRE en el servidor — listen server, el host no debe tener ventaja injusta perceptible).
- [ ] Resolución: objetivo es silla-jugador → **rotura de silla** (evento de eliminación, física de pedazos placeholder) · objetivo es señuelo → **1.8s adolorido** (slow + animación placeholder) (D5, D6).
- [ ] Conversión del eliminado en cazador (D1): re-posesión de pawn cazador tras la rotura, sin cortar el flujo de la ronda.
- [ ] Fin de ronda: queda 1 silla viva → gana la ronda (D2).
- **Criterio de hecho (test de diversión, no solo técnico):** en un playtest interno de 4 jugadores, la caminata de cola provoca risa y el esquive en el último segundo ocurre naturalmente. Si no pasa, iterar velocidad/distancia ANTES de avanzar.
- **Cubre:** D1, D2, D5, D6.

## FASE 3 — Movilidad y habilidades de las sillas
**Objetivo:** las sillas se sienten ágiles y traicioneras; el cazador baila de verdad.

- [ ] Sprint con stamina + salto para sillas (D10), parámetros en BalanceData.
- [ ] Empujón físico silla→silla (D10): impulso con física, la traición estrella de D2.
- [ ] **Sistema genérico de habilidades con cooldown** (`USillasAbilityComponent`): implementar el framework ahora; el contenido concreto espera la decisión **⏳ P10b** (candidatas: sonido señuelo, intercambio con señuelo, modo rígido, taunt). Diseñar el componente para que cada habilidad sea un DataAsset enchufable.
- [ ] Baile del cazador (D13): durante fase Música, control bloqueado a la coreografía; la animación gira la cámara en patrón fijo y aprendible (placeholder: secuencia de rotaciones de cámara; animación real en Fase 6).
- **Criterio de hecho:** una silla puede sprintar a esconderse, empujar a otra frente al cazador, y el cazador tiene ventanas ciegas reales durante el baile que las sillas explotan.
- **Cubre:** D10, D13. **Bloqueado parcialmente por:** ⏳ P10b (solo el contenido de habilidades, no el sistema).

## FASE 4 — Audio como mecánica + intensificación
**Objetivo:** el oído del cazador funciona y el tempo aprieta hacia el final.

- [ ] Audio espacial (verificar en 5.8 el estado de MetaSounds/spatializer del template de audio): **respiración procedural de sillas-jugador audible por proximidad SOLO para cazadores** (D17) — atenuación corta, sin indicador visual.
- [ ] Crujidos de movimiento: moverse emite sonido físico localizable (a más velocidad, más ruido — el sprint es ruidoso por diseño).
- [ ] Música por fases sincronizada en red + intensificación D12: al caer sillas, silencios más largos y música más corta (curva en BalanceData).
- [ ] Sistema preparado para **⏳ P18** (aguantar respiración): dejar el hook en el componente de respiración; activar cuando se decida.
- **Criterio de hecho:** con auriculares, un cazador puede encontrar una silla SOLO por la respiración; el final de ronda con 2 sillas vivas se siente notablemente más tenso que el inicio.
- **Cubre:** D12, D17. **Bloqueado parcialmente por:** ⏳ P18.

## FASE 5 — Match completo y puntaje
**Objetivo:** el loop de sesión completo: lobby → 5 rondas → podio.

- [ ] Match de 5 rondas con cazador inicial aleatorio sin repetir hasta agotar (D7).
- [ ] Sistema de puntos (D7b) en PlayerState: supervivencia por fase completada + capturas como cazador + bonus último vivo. **Regla de balance de referencia: ganar la ronda vivo ≥ mejor puntaje posible como cazador de esa ronda** (anti dejarse-atrapar).
- [ ] UI mínima funcional: HUD de fase (música/silencio + countdown), feed de eliminaciones, scoreboard entre rondas, podio final.
- [ ] Config de lobby del host (D8): cazadores iniciales, cantidad de rondas.
- **Criterio de hecho:** una sesión de 8 jugadores juega un match completo de principio a fin sin intervención externa y el ganador por puntos se percibe justo.
- **Cubre:** D7, D7b, D8 (completos).

## FASE 6 — Tema, arte y animación (bloqueada por ⏳ P14/P15/P16)
**Objetivo:** el juego deja de ser greybox y se convierte en producto con identidad.

- [ ] Decidir tema (⏳ P14), estética (⏳ P15) y nombre (⏳ P16) — bloquean esta fase, no las anteriores.
- [ ] Mapa 1 real tematizado (layout del greybox validado en Fases 2–5, re-vestido con el tema).
- [ ] Animaciones definitivas: caminata de cola (el asset MÁS importante del juego — presupuestar iteración), baile del cazador con coreografía final, rotura de silla con VFX y pedazos, dolor de 1.8s.
- [ ] Sonido final: música original por fases (la música ES gameplay — el patrón debe ser musicalmente legible), respiraciones, crujidos con carácter.
- [ ] Menús, flujo de lobby con estilo, settings.
- **Criterio de hecho:** un tráiler de 30 segundos grabado del juego se entiende sin explicación (Test del Clip aplicado al producto).

## FASE 7 — Red, pulido y beta
**Objetivo:** sólido en condiciones reales y listo para manos externas.

- [ ] Auditoría de replicación del listen server: latencia de la captura con ping real (la caminata de cola contra jugadores con 80–120ms — decidir tolerancias), relevancia de audio, host migration si el template la soporta.
- [ ] Pase de balance con los puntos marcados en el diseño: velocidad de caminata de cola y distancia de sentado (los 2 números más importantes), supervivencia media de la última silla (si <10s, compensar), economía de puntos D7b.
- [ ] Optimización (20+ sillas con física en pantalla), settings de accesibilidad de audio (D17 depende del oído: subtítulos de sonido como opción).
- [ ] Build de beta cerrada para playtests externos + captura de métricas básicas (duración de ronda real, % de capturas erradas, supervivencia por fase).
- **Criterio de hecho:** 3 sesiones de playtest externas consecutivas sin bugs bloqueantes y con risas registradas en los momentos diseñados (caminata de cola, esquive, traición por empujón).

---

## Orden de dependencias (resumen)

```
FASE 0 → FASE 1 → FASE 2 → FASE 3 → FASE 4 → FASE 5 → FASE 7
                                  (P10b)   (P18)         ↑
                              FASE 6 (P14/P15/P16) ──────┘
```

- Las Fases 0–5 no dependen de ninguna decisión pendiente (los sistemas se construyen; el contenido se enchufa).
- La Fase 6 puede correr en paralelo a la 5 si las decisiones de tema se cierran antes.
- **Regla de oro del plan:** ningún avance a la fase siguiente si el "criterio de hecho" (especialmente los de diversión de F2 y F4) no se cumple. La Fase 2 es la apuesta: si la caminata de cola no da risa en greybox, se itera ahí — es 10× más barato que descubrirlo en la Fase 6.

## Convenciones para trabajar con Claude Code

1. Al inicio de cada sesión, dar como contexto: este plan + `diseno-modo-sillas.md` + la fase activa.
2. Todos los valores de gameplay salen de `USillasBalanceData` — si un número aparece hardcodeado en una PR, es un bug.
3. Gameplay server-authoritative siempre (captura, eliminación, puntos, fases); cosmético puede ser cliente.
4. Commits por tarea del checklist, no por fase entera.
5. No modificar el código del template de multiplayer salvo necesidad justificada — extenderlo desde el módulo nuevo.
6. Verificar APIs contra el engine 5.8 instalado; no asumir firmas de versiones anteriores.
