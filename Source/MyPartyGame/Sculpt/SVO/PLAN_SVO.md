# Sculpturillo — Esculpido adaptativo tipo SculptrVR (Sparse Voxel Octree)

Objetivo: "grande = pocos triángulos, chico = detalle infinito" + escenarios grandes,
tiempo real y multiplayer. Basado en cómo lo hace SculptrVR (SVO + mallado adaptativo).

Se desarrolla en la rama `svo-sculpt`, POR INCREMENTOS chicos que compilan y se testean
uno por uno, SIN tocar el esculpido actual (uniforme) hasta que el nuevo esté validado.

## Fases

### Fase 1 — Núcleo SVO + mallado (single-player, offline)  ✅ VALIDADA (2026-09-05)
- [x] 1.1  Estructura del octree esparso (`FPTVoxelOctree`): nodos, subdivisión adaptativa,
           sample SDF trilineal, edición CSG de esfera (add/erase).
- [x] 1.2/1.3  Dual Contouring (Surface Nets, hojas de 1 celda) + conectividad por aristas mínimas
           con consulta de vecinos → CRACK-FREE entre niveles distintos. Validado: sólido, sin
           grietas, adaptativo, normales OK.
- [x] 1.4  Actor de prueba (`APTSVOTest`) + comandos `PTSVO.Demo/Clear/Stats/Add`.

### Fase 2 — Paridad de features sobre el SVO
- [x] 2.1  Color/pintura por voxel (vertex colors en el DC).  ✅
- [x] 2.2  Undo por trazo (snapshots por clon; se optimiza a copy-on-write en Fase 3).  ✅
- [x] 2.3  Baking/persistencia: Serialize/LoadFromBytes del octree (blob de bytes).  ✅
- [x] 2.4  Shapes (Box/Elipsoide/Cilindro/Toro/Cono) + escala no-uniforme + rotacion (EditShape).  ✅

### Fase 3 — Integración al juego + multiplayer
- [~] 3.1  Flag bUseSVO en APTSculptVolume: geometria Add/Erase + color por el octree, meshado a la seccion 0. OFF=juego intacto. Paint/Smooth/capas/ojos aun no portados.
- [ ] 3.2  Replicación de OPERACIONES (reusar el patrón `Server/Multicast_ApplyStamp`):
           se manda la operación, cada cliente la aplica a su octree → determinismo.
- [ ] 3.3  Tests multiplayer (2+ máquinas): consistencia, sin desync.

### Fase 4 — UX de escala tipo SculptrVR
- [ ] 4.1  Escalar el mundo / al jugador para editar a distintos niveles del octree
           (achicarse = detalle fino; agrandarse = trabajar en grande).

## Riesgos
- El mallado adaptativo SIN costuras (1.3) es la parte difícil (dual contouring + transiciones).
- Performance del re-mallado incremental (solo re-mallar nodos tocados, async).
- Determinismo en la replicación de operaciones (3.2).

## Regla
Cada casilla = un incremento que COMPILA y se TESTEA antes de seguir. Nada se mergea a `main`
hasta que la Fase 1 esté validada (calidad + perf).
