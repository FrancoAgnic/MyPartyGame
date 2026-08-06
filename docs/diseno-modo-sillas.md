# Diseño de Modo de Juego — **SEAT HAPPENS** (antes "Sillas + Escondite")
### Documento vivo de decisiones · v0.1 · Agosto 2026
**Regla del documento:** las decisiones las toma el equipo (Deivor + hermano). Cada decisión cerrada queda numerada con la fecha. Lo abierto está marcado como ⏳ PENDIENTE.
_(Documento del equipo con ediciones asistidas por Claude — se conserva el nombre de archivo original porque el plan de producción lo referencia por ruta exacta.)_

---

## 1. Concepto (idea original de Deivor)

Combinación de **las sillas musicales + el escondite (prop hunt)**:

- Un jugador es el **CAZADOR**; el resto de los jugadores son **SILLAS** escondidas en el escenario, mezcladas entre sillas reales (señuelos).
- **Cuando la música SUENA:** el cazador baila obligatoriamente (no puede sentarse ni capturar) — es la ventana en la que las sillas se reposicionan.
- **Cuando la música SE CORTA:** el cazador puede cazar. Captura sentándose sobre la silla sospechosa.
- La inversión de la memoria de la infancia: en las sillas musicales corrías a sentarte cuando cortaba la música — acá, cuando corta la música, **el que se sienta es tu enemigo, y la silla sos vos.**

## 2. Decisiones cerradas

| # | Decisión | Resolución | Fecha |
|---|---|---|---|
| D1 | ¿Qué pasa con el jugador eliminado? | **Se une como segundo cazador.** Modo infección: los cazadores crecen, las sillas se achican. Nadie queda fuera del juego. | 04-ago-2026 |
| D2 | Condición de victoria de la ronda | **Gana la ÚLTIMA silla viva.** Victoria individual, no de equipo — cada silla juega para sí misma (alianzas posibles, todas temporales). | 04-ago-2026 |
| D3 | Control de la música | **Patrón fijo que todos conocen.** Ritmo predecible tipo metrónomo: la tensión no viene de la sorpresa sino de que TODOS saben cuándo llega el silencio y planifican contra reloj. | 04-ago-2026 |
| D4 | Movimiento de las sillas en el silencio | **Libres de moverse SIEMPRE**, pero el cazador puede sentarse en cualquier momento del silencio. El sigilo es conductual, no forzado: moverse está permitido, ser VISTO moviéndose es sentencia. | 04-ago-2026 |
| D5 | Mecánica de captura (LA firma del juego) | **La caminata de cola:** manteniendo clic, el cazador rota 90° y camina en posición de sentarse, con la cola hacia afuera, apuntando a la silla sospechosa. Al llegar a distancia suficiente se sienta y **rompe la silla, eliminando al jugador-silla**. Gesto comprometido, telegrafiado y cómico. | 04-ago-2026 |
| D6 | Castigo por error (sentarse en señuelo) | **1.8 segundos caminando lento y adolorido**: las sillas reales NO se rompen (son sólidas, duelen); solo las sillas-jugador se rompen. La densidad de señuelos del mapa se mantiene constante toda la ronda. | 04-ago-2026 |
| D7 | Estructura del match | **Rondas fijas (ej. 5) con cazador inicial al azar.** | 04-ago-2026 |
| D8 | Cantidad de cazadores iniciales | **Configurable en el lobby por el host** (flexibilidad de party local/online). Definir defaults recomendados por cantidad de jugadores en el prototipo. | 04-ago-2026 |
| D10 | Habilidades de las sillas | **Kit completo:** correr/sprint (con cooldown o resistencia) + saltar + empujar a otras sillas (traición física) + habilidad especial con cooldown (ej. sonido señuelo). | 04-ago-2026 |
| D11 | Reparto de fases | **Música corta, silencio largo (ref. 10s / 20s): domina la caza.** La ventana de reposicionamiento es breve y valiosa; números exactos a afinar en playtest. | 04-ago-2026 |
| D12 | Evolución del patrón | **Con menos sillas vivas: silencios más largos, música más corta.** La presión escala sola hacia el final de ronda. | 04-ago-2026 |
| D13 | Visión del cazador durante el baile | **Visión limitada: la animación de baile le gira la cámara a ratos.** La coreografía crea puntos ciegos; el baile es predecible (coherente con D3), así que las sillas expertas aprenden a leerlo y moverse a sus espaldas. | 04-ago-2026 |
| D7b | Puntaje del match | **Sistema de puntos: sobrevivir por fase + capturas como cazador + bonus al último vivo.** Todos los roles puntúan en todo momento — el eliminado sigue compitiendo por el match desde el rol de cazador. | 04-ago-2026 |
| D9 | Densidad de señuelos | **Media: 15–25 sillas reales por mapa.** Equilibrio entre memoria del cazador y búsqueda activa. | 04-ago-2026 |
| D17 | Herramientas del cazador | **Solo oído fino: escucha respiración/crujidos cerca de sillas-jugador.** Sin sprint ni marcadores visuales — caza con oído y memoria pura. El sound design pasa a ser mecánica central. | 04-ago-2026 |
| D10b | Habilidad especial inicial | **Taunt/burla**: sonido burlón audible por todos, puro generador de clips. El resto del pool (sonido señuelo, intercambio con señuelo, modo rígido) queda en backlog para después del playtest — el framework acepta habilidades nuevas como DataAssets sin código nuevo. | 05-ago-2026 |
| D15 | Estética | **Ragdoll cómico confirmado**: todo el humor del juego es físico (caminata de cola, empujón, rotura). Guía las animaciones y VFX de Fase 6. | 05-ago-2026 |
| D2b | Fin de ronda con RELOJ (revisión de D2) | **La ronda tiene tiempo límite** (config en BalanceData): los cazadores deben cazar a TODAS las sillas antes de que expire. Si el reloj llega a cero, **todas las sillas vivas ganan la ronda** (bonus para cada una); si cazan a todas, la ronda es de los cazadores. La última silla ya no gana al instante: juega perseguida 1 vs N — el "clímax garantizado" del diseño por fin existe y la métrica de supervivencia de la última silla se puede medir. | 06-ago-2026 |
| D14 | Ambientación | **CASAMIENTO**: fiesta de boda — sillas de ceremonia, pista de baile, la música-mecánica es la banda que toca y para. Producción de Fase 6: arte MIXTO (packs de Fab para entorno/props + la silla-personaje y su caminata de cola a medida, donde vive la identidad) y música de bibliotecas/generada con edición (el patrón debe ser musicalmente legible, D3). | 06-ago-2026 |
| D16 | Nombre del juego | **SEAT HAPPENS** — elegido por Deivor de la lista de candidatos (tema casamiento). Verificado el 06-ago-2026: sin colisiones en Steam (lo más cercano es "Is This Seat Taken?", un puzzle cozy sin relación) ni marcas visibles en búsqueda web. Pendiente formal: chequeo USPTO/EUIPO antes del lanzamiento en Steam. | 06-ago-2026 |
| D18 | Aguantar la respiración | **Sí, como recurso TRANSITORIO — implementado APAGADO** (switch en BalanceData para comparar en playtest). Reglas anti-campeo: (1) el jadeo delator solo ocurre si la barra se agota por completo — soltar antes = recuperación silenciosa pero la respiración vuelve YA; (2) recarga lenta y solo mientras se respira audible (no se encadenan aguantes); (3) el escape del campeo lo garantiza la estructura: en cada fase de Música el cazador baila obligado (D13) y la silla asediada se reubica. Disuasivo extra: campear una silla = no puntuar capturas (D7b). | 05-ago-2026 |

### Implicaciones de diseño de D1+D2+D3 (derivadas, a validar en prototipo)
- D1 (infección) + D2 (último vivo) crean una curva de tensión natural: el final de ronda es 1 silla vs N cazadores — el clímax está garantizado por estructura.
- D2 vuelve traicioneras a las sillas entre sí: delatar/empujar a otra silla te compra tiempo. La traición entre escondidos es un generador de clips.
- D3 convierte el juego en planificación con información perfecta: el ritmo funciona como reloj de ajedrez. Preguntas derivadas para el prototipo: duración exacta de cada fase (¿música 15 s / silencio 20 s?) y si el patrón se acelera al reducirse las sillas vivas.

### Implicaciones de diseño de D4+D5+D6 (derivadas, a validar en prototipo)
- **D4 redefine las fases:** la música ya no congela a nadie — es la ventana SEGURA (el cazador baila y no puede capturar) y el silencio es la ventana de PELIGRO. Las sillas eligen cuánto arriesgan en cada fase. El mind game es de comportamiento puro: la mejor silla es la que se mueve como mueble.
- **D5 abre la contrajugada estrella:** la caminata de cola es visible y lenta → la silla que la ve venir puede escaparse en el último instante. El "esquive de cola" es el clip defensivo del juego; la rotura de silla con jugador adentro es el clip ofensivo.
- **D5, detalle técnico:** clic mantenido = intención; soltar cancela. La velocidad de la caminata de cola y la distancia de "sentado válido" son los dos números de balance más importantes del juego.
- **D6 mantiene el ritmo alto:** 1.8 s es castigo suficiente para que fallar importe (una silla cercana escapa) sin frenar la ronda. Señuelos indestructibles = el camuflaje no se degrada; la información del cazador (memoria de qué sillas ya probó) es su único progreso.

### Implicaciones de diseño de D7+D8+D10 (derivadas, a validar en prototipo)
- **D7 exige un puntaje de match:** si el match son 5 rondas y cada ronda la gana UNA silla, hace falta definir cómo se corona al ganador del match (¿solo victorias de ronda, o puntos por supervivencia y capturas también?) → nueva pregunta P7b en Ronda 5.
- **D8 pide defaults inteligentes:** el host configura, pero el juego debe sugerir (ej. 1 cazador para 2–5 jugadores, 2 para 6–8). El 1v1 (un cazador, una silla) funciona como duelo psicológico puro entre señuelos — vale la pena testearlo como modo propio.
- **D10, el costo del kit:** cada habilidad delata — una silla que salta o corre es OBVIAMENTE un jugador. Todas las habilidades son un trade-off entre movilidad y camuflaje: usarlas frente al cazador es suicidio, usarlas a sus espaldas es jugar bien. El empujón es la traición física que D2 pedía (empujar una silla a la vista del cazador = comprarte tiempo). Definir lista de habilidades especiales candidatas (sonido señuelo, intercambio de posición, quedarse rígida anti-empujón) → P10b.
- **D10 balance del cazador:** con sillas tan ágiles, el cazador necesita su contraparte (¿sprint propio? ¿visión especial breve?) → P17 en Ronda 5.

### Implicaciones de diseño de D11+D12+D13 (derivadas, a validar en prototipo)
- **D11+D12+D1 apilan presión sobre el final:** última silla = más cazadores + silencios más largos + música más corta. El clímax es estructural, pero hay riesgo de que sea INJUGABLE para la última silla → punto de balance crítico: medir en playtest cuánto sobrevive en promedio la última silla; si es <10 s, considerar una compensación (ej. la última silla recupera habilidades más rápido).
- **D13 convierte el baile en coreografía-información:** la cámara del cazador barre en un patrón aprendible. Las sillas leen el baile como los corredores leen al vigilante en luz roja/luz verde. Con 2+ cazadores bailando, sus barridos se complementan y la fase de música deja de ser segura — el lobby con muchos cazadores se equilibra solo.
- **D13 arregla el riesgo del cazador aburrido:** bailar con giros de cámara es cómico también para el que lo sufre — el rol de cazador tiene su propia comedia (el streamer bailando contra su voluntad mientras intenta espiar es un clip en sí mismo).

### Implicaciones de diseño de D7b+D9+D17 (derivadas, a validar en prototipo)
- **D7b cierra el círculo de D1:** ser eliminado temprano no te saca del match — pasás a farmear puntos de captura como cazador. ⚠️ Balance crítico: la supervivencia por fase + el bonus del último vivo deben valer claramente MÁS que las capturas, o algún jugador va a dejarse atrapar a propósito para farmear del otro lado. Regla de referencia inicial: ganar la ronda vivo ≥ mejor cazador posible de esa ronda.
- **D9 fija la matemática del camuflaje:** 15–25 señuelos + hasta 7 sillas-jugador ≈ 1 de cada 3–4 sillas es un jugador. El cazador PUEDE memorizarlas todas pero le cuesta — exactamente la zona donde la memoria es habilidad y no trivia.
- **D17 hace del audio el arma del cazador:** la respiración por proximidad significa que quedarse cerca del cazador es jugar con fuego. Deriva natural → P18: ¿pueden las sillas AGUANTAR la respiración? (recurso limitado, con jadeo delator al soltarlo). Requisito técnico: audio espacial/binaural de calidad desde el prototipo.

## 3. Preguntas abiertas

### ✅ No quedan preguntas abiertas — el diseño está completo

_Resueltas el 05-ago-2026: P10b → D10b (taunt; resto en backlog) · P15 → D15 (ragdoll cómico) · P18 → D18 (aguantar transitorio, implementado apagado)._
_Resueltas el 06-ago-2026: P14 → D14 (casamiento) · revisión D2 → D2b (ronda con reloj, clímax 1 vs N) · P16 → D16 (**Seat Happens**)._

## 4. Riesgos identificados (a testear en prototipo)
- Que ser cazador al inicio (bailar sin poder actuar) se sienta como pérdida de control → mitigar con agencia durante el baile (posicionarse, espiar, memorizar).
- D3 (patrón conocido) podría quitar tensión si las fases son demasiado largas → el prototipo debe encontrar el tempo exacto.
- D2 (victoria individual) + D1 (infección) implican que el primer eliminado juega "de cazador" casi toda la ronda → el rol de cazador tiene que ser genuinamente divertido.
