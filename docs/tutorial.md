# Tutorial de Sculpi

Práctica de esculpido guiada por **Sculpi** (un personaje del juego con la skin *Frank Suit* del Workshop,
empaquetada en `Content/Tutorial/Sculpi.skin`). Se juega sin conexión en el mapa de juego (`Lvl-01`).

## Cuándo aparece

- **Primera vez:** después de elegir el idioma en el boot (`UPTBootWidget`), en lugar del menú.
- **Repetir:** botón **Tutorial** del submenú Jugar (`TutorialButton` en `WBP_MainMenu`; si el WBP no lo trae,
  se clona debajo del botón de audiencia).
- Terminado o saltado (menú de pausa → *Saltar tutorial*, o *Salir*) queda marcado como hecho
  (`UPTGameUserSettings::bTutorialDone`). Consola: `PT.ResetTutorial` lo vuelve a habilitar; `PTTutorial` lo abre.

## Lecciones

Mirar → moverse (aro) → subir → bajar → Agregar (bola fantasma) → tamaño → formas (cubo) → rotar
(cilindro acostado) → aplastar/estirar con Z/X + rueda (pastilla) → trazos rectos con Z/X (poste y viga) →
pegar con Alt (bolita sobre el cubo) → Borrar (mitad de arriba) → deshacer → borrar todo (3 s) → pintar
(elegir y guardar color) → ojos. Después 3 palabras: **iglú** (guía completa), **hongo** (solo el tallo; el
sombrero se mide sin dibujarse) y **perro** (libre, 90 s, Enter o *¡Listo!* en la pausa). Al final, foto del
perro con cuenta 3-2-1, en un marco tipo polaroid ("Mi perro" + logo), guardada en las **capturas de Steam**
(sin Steam: PNG en `Saved/Screenshots`).

## Cómo funciona

| Pieza | Archivo |
|---|---|
| Director (Sculpi, lecciones, guías fantasma, medición, foto) | `Source/MyPartyGame/Tutorial/PTTutorialDirector.*` |
| Interfaz (diálogo con teclas, progreso, pausa, cuenta regresiva, polaroid) | `Tutorial/PTTutorialWidget.*` |
| Entrada / salida | `UPTGameInstance::EnterTutorial / ExitTutorial` (flag `bTutorialMode`) |

- El GameMode normal (`BP_SculptGameMode`) ve `bTutorialMode`, crea el director (`TutorialDirectorClass`) y
  deja al jugador esculpiendo sin turnos ni reloj. El HUD oculta palabra, reloj, marcador y chat.
- **Guías:** las mismas formas del sello (`BuildStampPreview`) y la misma cuenta (`StampSDF`). El relleno se
  mide consultando `SampleWorldDensity` en una grilla de puntos dentro de la guía (5 veces por segundo).
- **Material de las guías:** `GhostMaterial` = `/Game/Tutorial/MI_TutorialGhost` (si no existe, prueba
  `M_TutorialGhost` y si no, `M_SmoothPreview`). Recibe el parámetro **Color** (azul, rojo en Borrar,
  amarillo en los aros) y se va poniendo verde a medida que se rellena.
- **Teclas:** íconos de `Content/UMG/Texture/NewUI/Gameplay_UI/Keyboards/{Keyboard,Joystick}` según la
  tecla asignada ahora (rebindeable) o el joystick si es lo último que se usó; si falta un ícono, texto.
- Ajustes en un BP hijo de `APTTutorialDirector`: sonidos, logo, porcentajes (`LessonFill`, `WordFill`),
  `PerroSeconds`, material.
- Desarrollo: `PTTutStep N` salta a una lección, `PTTutFill` rellena las guías por código.
