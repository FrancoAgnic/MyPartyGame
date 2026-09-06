# Optimización del esculpido SVO — 6 de septiembre de 2026

Se revisaron PLAN_SVO.md, el núcleo PTVoxelOctree, la integración en PTSculptVolume,
el guardado y pintado de cabezas y los tres cambios anteriores de mallado por chunks.

## Cambios

- Balance 2:1 incremental: se recorren únicamente subárboles cuyos sondeos de vecinos
  pueden tocar hojas editadas o refinadas. La propagación del refinamiento conserva
  el algoritmo anterior. Cargar y deshacer invalidan el árbol completo. Pintar no
  necesita balancear. Existe un recorrido completo de referencia para las pruebas.
- Un componente de malla por chunk fino ocupado. En Unreal 5.8, CreateMeshSection
  invalida el scene proxy del componente y ese proxy copia todas sus secciones.
  Separar componentes evita subir nuevamente la escultura completa al cambiar un bloque.
  Los componentes vacíos se destruyen; no se crean 1.728 componentes vacíos.
- Se invalidan también las regiones refinadas por balance y los límites completos
  de las hojas editadas. Se usan límites transformados para sellos rotados.
- Las capas de detalle se reconstruyen solo cuando están sucias.
- Pintura y horneado de cabezas incluyen los componentes finos. Limpiar y cargar
  destruyen los componentes anteriores. El formato guardado permanece igual.
- Marcadores de perfil para edición, balance y mallado de chunks.

## Verificación ejecutada

Compilación MyPartyGameEditor Win64 Development con Unreal 5.8 correcta.
Cuatro pruebas de automatización correctas, sin advertencias en los resultados:

1. IncrementalBalanceMatchesFull: 90 ediciones de tamaños y formas distintos,
   rotaciones, recorte, undo y carga; mismo árbol serializado y misma malla que
   el recorrido completo. Pintura sin balance geométrico.
2. LocalEditScaling: 245 sellos finos distribuidos, árbol de 140.232 hojas;
   30 ediciones pequeñas posteriores conservan el árbol completo de referencia.
3. ChunkPartitionMatchesWhole: mismos triángulos orientados, colores y normales
   al dividir la malla en sección gruesa y chunks finos.
4. VolumeIncrementalLifecycle: actor real en mundo de editor; bloques separados,
   conservación del buffer de un bloque lejano, deshacer, capa de detalle,
   pintar, borrar, guardar, cargar y limpiar. La malla incremental coincide
   con una reconstrucción completa después de cargar.

Última medición del balance para las 30 ediciones:

| Medida | Recorrido completo | Incremental |
|---|---:|---:|
| Tiempo acumulado | 2.030,802 ms | 2,753 ms |
| Hojas revisadas | 4.206.330 | 8.513 |

Esta es una medición del balance de CPU, no de FPS del juego ni del renderizado.
Las pruebas corrieron sin renderizado (NullRHI). Reporte detallado en
Saved/Automation/SVOOptimization/index.json y log en Saved/Logs/SVOOptimizationTests.log.

## Límites y siguiente comprobación en juego

El mallado sigue siendo sincrónico pero acotado por bloques. Undo todavía clona el
árbol; esta optimización no implementa copy-on-write ni LOD. No se redujo la resolución
de las brochas. Sigue pendiente medir FPS reales con el nivel, materiales, sombras
y una sesión multiplayer; esas cifras no se pueden deducir del microbenchmark.

Los cambios están en los archivos locales. No se hizo commit, merge, publicación
ni empaquetado. El ejecutable empaquetado anterior no se actualiza al compilar el editor.

El primer intento de compilación restringido quedó bloqueado y fue interrumpido.
Las dos compilaciones autorizadas finalizaron correctamente. No se encontró un evento
de dotnet que permita atribuir con certeza el aviso de excepción reportado por el usuario.
