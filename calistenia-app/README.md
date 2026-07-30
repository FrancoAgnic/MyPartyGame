# Calistenia · Entrenamiento & Handstand

Aplicación web para **planificación y seguimiento de entrenamiento**, enfocada en
calistenia, hipertrofia del tren superior y progresión de handstand. Pensada
para un usuario que entrena en Planet Fitness, empieza con 3 días por semana y
progresa gradualmente a 4 o 5.

Mobile-first, tema oscuro por defecto (con modo claro), navegación inferior en
móvil y lateral en escritorio.

---

## Stack

- **Next.js 14** (App Router) + **React 18** + **TypeScript** estricto
- **Tailwind CSS** (design tokens claro/oscuro por variables CSS)
- **Supabase** (Postgres + Auth) como backend opcional
- **Zustand** (persistencia local) como capa de datos por defecto / modo demo
- **Recharts** para gráficos
- **React Hook Form** + **Zod** para formularios validados
- **lucide-react** para iconos

### Modo demo vs. Supabase

La app **funciona sin backend**: por defecto persiste los datos del usuario en
`localStorage` (modo demo), así se puede probar toda la experiencia sin
configurar nada. Al completar las variables de entorno de Supabase y poner
`NEXT_PUBLIC_USE_SUPABASE=true`, se habilitan autenticación y (como evolución
natural) sincronización en la nube. Los componentes siempre leen/escriben a
través del store (`src/lib/store.ts`), de modo que la fuente de datos es
intercambiable.

---

## Arquitectura

```
calistenia-app/
├── src/
│   ├── app/                      # Rutas (App Router)
│   │   ├── layout.tsx            # Shell raíz + tema
│   │   ├── page.tsx              # Inicio
│   │   ├── entrenar/            # Rutina + modo entrenamiento
│   │   │   ├── page.tsx          # Lista de días + calentamiento
│   │   │   └── [code]/page.tsx   # Modo entrenamiento (logging, timer)
│   │   ├── handstand/page.tsx    # Progresión y registro de handstand
│   │   ├── progreso/page.tsx     # Gráficos + evaluación de frecuencia
│   │   ├── biblioteca/page.tsx   # Biblioteca de ejercicios (búsqueda/filtros)
│   │   ├── calendario/page.tsx   # Calendario + checklist + reprogramar
│   │   ├── perfil/page.tsx       # Perfil, preferencias, métricas, seguridad
│   │   └── login/page.tsx        # Registro / login / recuperación
│   ├── components/
│   │   ├── ui/                   # Primitivos reutilizables (Card, Button…)
│   │   ├── layout/               # AppShell, navegación, tema
│   │   ├── workout/              # RestTimer, ExerciseLogger, diálogos…
│   │   ├── charts/               # TrendChart (Recharts)
│   │   └── profile/              # ProfileForm, MetricsSection
│   └── lib/
│       ├── types.ts              # Modelo de dominio (tipos)
│       ├── store.ts              # Estado global (Zustand + persist)
│       ├── progression.ts        # Progresión doble + evaluación de frecuencia
│       ├── selectors.ts          # Derivaciones (racha, volumen, semana…)
│       ├── schemas.ts            # Validación Zod
│       ├── utils.ts              # Utilidades (fechas, formato, cn…)
│       ├── supabase/             # Cliente + auth (opcional)
│       └── data/                 # Contenido semilla (ejercicios, rutina…)
├── supabase/
│   ├── schema.sql                # Tablas, FKs, índices, timestamps, soft delete
│   ├── policies.sql              # Row Level Security + trigger de alta
│   └── seed.sql                  # Semilla de ejercicios (autogenerada)
└── scripts/
    └── generate-seed.ts          # Genera seed.sql desde el contenido TS
```

**Separación de responsabilidades:** presentación (`components`), lógica de
dominio (`lib/progression`, `lib/selectors`), acceso a datos (`lib/store`,
`lib/supabase`) y contenido (`lib/data`) están desacoplados. Los componentes no
llevan datos hardcodeados: consumen el contenido semilla y el store.

---

## Modelo de datos

Tablas principales (ver `supabase/schema.sql`):

| Tabla | Descripción |
|---|---|
| `user_profiles` | Perfil editable (nivel, objetivos, marcas, equipamiento). |
| `user_preferences` | Tema, unidades, duración de sesión, timer. |
| `exercises` | Biblioteca global de ejercicios (contenido). |
| `exercise_media` / `exercise_user_notes` | Medios y notas personales. |
| `training_plans` / `training_days` / `workout_exercises` | Plan y prescripción. |
| `workout_sessions` / `exercise_sets` | Sesiones y series registradas + feedback. |
| `handstand_sessions` | Registro de práctica de handstand. |
| `body_metrics` / `progress_tests` / `pain_reports` | Métricas y molestias. |
| `weekly_checklists` | Checklist semanal (JSONB). |
| `progression_assessments` | Evaluaciones para aumentar frecuencia. |

Incluye relaciones con claves foráneas, índices por `(user_id, date)`,
`timestamps` (`created_at` / `updated_at` con trigger) y `deleted_at` para
soft delete donde corresponde.

---

## Lógica de progresión

- **Progresión doble de cargas** (`suggestProgression`): mantener el peso hasta
  llegar al tope del rango de reps en todas las series con reps en reserva;
  recién ahí aumentar y volver a la zona baja. Bloquea el aumento ante dolor,
  fallo en todas las series o RPE 10 repetido.
- **Handstand:** sistema de 5 niveles con objetivos por nivel.
- **Frecuencia 3→4 / 4→5** (`assessFrequencyProgression`): no aumenta por el
  paso del tiempo; exige ≥85% de adherencia, sin dolor persistente, sueño
  estable y sin fallo constante durante varias semanas. Recomienda un 4.º día
  ligero o las opciones A/B/C para el 5.º día.

---

## Ejecutar localmente

Requisitos: Node.js 18.18+ (recomendado 20/22).

```bash
cd calistenia-app
npm install
cp .env.example .env.local     # opcional: sólo para habilitar Supabase
npm run dev                    # http://localhost:3000
```

La app arranca en **modo demo** (sin backend). Todos los datos se guardan en el
navegador.

Scripts útiles:

```bash
npm run build        # build de producción
npm run typecheck    # TypeScript sin emitir
npm run lint         # ESLint (next)
npm run seed:sql     # regenera supabase/seed.sql desde el contenido TS
```

---

## Configurar Supabase (opcional)

1. Creá un proyecto en [supabase.com](https://supabase.com).
2. En **SQL Editor**, ejecutá en orden:
   1. `supabase/schema.sql`
   2. `supabase/policies.sql`
   3. `supabase/seed.sql`
3. En **Project Settings → API**, copiá `Project URL` y `anon public key`.
4. Completá `.env.local`:

   ```env
   NEXT_PUBLIC_SUPABASE_URL=https://xxxx.supabase.co
   NEXT_PUBLIC_SUPABASE_ANON_KEY=eyJ...
   NEXT_PUBLIC_USE_SUPABASE=true
   ```

5. **Auth:** en Supabase → Authentication → Providers, habilitá Email. El
   trigger `handle_new_user` (en `policies.sql`) crea perfil y preferencias al
   registrarse.

> **Nota sobre eliminación de cuenta:** borrar el usuario de `auth.users`
> requiere la *service role key* desde un entorno seguro (Edge Function o Route
> Handler). El MVP cierra la sesión; ver "Pendientes".

---

## Desplegar

Recomendado: **Vercel**.

1. Importá el repo en Vercel y seleccioná el directorio raíz `calistenia-app`.
2. Cargá las variables de entorno (`NEXT_PUBLIC_SUPABASE_URL`,
   `NEXT_PUBLIC_SUPABASE_ANON_KEY`, `NEXT_PUBLIC_USE_SUPABASE`).
3. Framework preset: **Next.js**. Deploy.

Cualquier host con soporte Next.js 14 (Netlify, Render, etc.) también funciona.

---

## Seguridad y advertencias

La app **no reemplaza** a un médico ni fisioterapeuta y no diagnostica lesiones.
Muestra señales de alarma para detener el ejercicio (dolor agudo, mareo,
hormigueo, dolor en el pecho, etc.) y permite registrar molestias para ajustar
recomendaciones futuras.

---

## Funcionalidades del MVP

- [x] Inicio con resumen (racha, marcas, volumen, recomendación de frecuencia).
- [x] Rutina de 3 días (A/B/C) con calentamiento guiado.
- [x] Modo entrenamiento: logging de series (peso/reps/tiempo/RIR), historial,
      progresión sugerida, temporizador de descanso (sonido/vibración,
      reloj de pared), reemplazo/omisión de ejercicios y cuestionario final.
- [x] Handstand: 5 niveles, registro de sesiones y técnica.
- [x] Progreso: gráficos (peso, handstand, volumen, cargas por ejercicio) con
      filtros de tiempo + evaluación de frecuencia.
- [x] Biblioteca de ejercicios con búsqueda y filtros.
- [x] Calendario mensual con estados + reprogramar días + checklist semanal.
- [x] Perfil editable (Zod + RHF), preferencias, métricas, seguridad.
- [x] Autenticación Supabase (registro, login, recuperación) — opcional.
- [x] Esquema SQL con RLS, seed y trigger de alta.

## Pendientes (fuera del primer MVP)

- Sincronización bidireccional completa store ↔ Supabase (hoy el backend está
  cableado para auth y esquema; el store persiste local por defecto).
- Eliminación dura de cuenta vía Edge Function con *service role*.
- Subida real de videos/imágenes (hoy se usan placeholders/emoji e `video_url`).
- Wake Lock del temporizador con pantalla bloqueada en todos los navegadores.
- Generación automática del 4.º y 5.º día como planes editables.
- PWA / offline instalable y notificaciones push del temporizador.
- Tests automatizados (unit de `progression.ts` y e2e del modo entrenamiento).
```
