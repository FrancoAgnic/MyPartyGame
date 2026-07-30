-- =============================================================================
-- Calistenia App — Esquema de base de datos (Supabase / PostgreSQL)
-- =============================================================================
-- Ejecutar en el SQL Editor de Supabase (o vía `supabase db push`).
-- Incluye: tablas, claves foráneas, índices, timestamps, soft delete y RLS.
-- La autenticación usa la tabla `auth.users` de Supabase; las tablas de la app
-- referencian `auth.users(id)` mediante la columna `user_id`.
-- =============================================================================

create extension if not exists "pgcrypto";

-- Utilidad: actualizar automáticamente updated_at
create or replace function public.set_updated_at()
returns trigger language plpgsql as $$
begin
  new.updated_at = now();
  return new;
end;
$$;

-- --------------------------------------------------------------------- PERFIL
create table if not exists public.user_profiles (
  user_id uuid primary key references auth.users(id) on delete cascade,
  name text not null default 'Atleta',
  level text not null default 'principiante'
    check (level in ('principiante','intermedio','avanzado')),
  frequency smallint not null default 3 check (frequency between 3 and 5),
  goals text[] not null default '{}',
  strict_pullups int not null default 0,
  free_dips int not null default 0,
  free_handstand_seconds numeric not null default 0,
  location text not null default 'Planet Fitness',
  equipment_available text[] not null default '{}',
  equipment_not_guaranteed text[] not null default '{}',
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

create table if not exists public.user_preferences (
  user_id uuid primary key references auth.users(id) on delete cascade,
  theme text not null default 'dark' check (theme in ('dark','light')),
  units text not null default 'kg' check (units in ('kg','lb')),
  session_duration_minutes int not null default 70,
  prefer_machines boolean not null default false,
  rest_timer_sound boolean not null default true,
  rest_timer_vibration boolean not null default true,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now()
);

-- ---------------------------------------------------- BIBLIOTECA DE EJERCICIOS
-- Contenido global (compartido). Se puede sembrar con seed.sql.
create table if not exists public.exercises (
  id text primary key,
  name text not null,
  category text not null,
  primary_muscles text[] not null default '{}',
  equipment text[] not null default '{}',
  level text not null default 'principiante',
  is_bodyweight boolean not null default false,
  image_emoji text,
  video_url text,
  instructions text[] not null default '{}',
  breathing text,
  common_mistakes text[] not null default '{}',
  corrections text[] not null default '{}',
  regression text,
  progression text,
  planet_fitness_substitutions text[] not null default '{}',
  contraindications text,
  created_at timestamptz not null default now()
);

create table if not exists public.exercise_media (
  id uuid primary key default gen_random_uuid(),
  exercise_id text not null references public.exercises(id) on delete cascade,
  kind text not null check (kind in ('image','video')),
  url text not null,
  created_at timestamptz not null default now()
);
create index if not exists idx_exercise_media_exercise on public.exercise_media(exercise_id);

-- Notas personales del usuario sobre un ejercicio
create table if not exists public.exercise_user_notes (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  exercise_id text not null references public.exercises(id) on delete cascade,
  note text,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  unique (user_id, exercise_id)
);

-- --------------------------------------------------------- PLAN / DÍAS / EJ.
create table if not exists public.training_plans (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  name text not null default 'Programa 3 días',
  frequency smallint not null default 3,
  is_active boolean not null default true,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  deleted_at timestamptz
);
create index if not exists idx_training_plans_user on public.training_plans(user_id);

create table if not exists public.training_days (
  id uuid primary key default gen_random_uuid(),
  plan_id uuid not null references public.training_plans(id) on delete cascade,
  code text not null check (code in ('A','B','C','D','E')),
  title text not null,
  focus text,
  objective text[] not null default '{}',
  weekday smallint not null default 1 check (weekday between 0 and 6),
  sort_order int not null default 0,
  created_at timestamptz not null default now()
);
create index if not exists idx_training_days_plan on public.training_days(plan_id);

create table if not exists public.workout_exercises (
  id uuid primary key default gen_random_uuid(),
  training_day_id uuid not null references public.training_days(id) on delete cascade,
  exercise_id text not null references public.exercises(id),
  block_title text,
  sets int not null default 3,
  reps_min int,
  reps_max int,
  time_seconds_min int,
  time_seconds_max int,
  rest_seconds int not null default 90,
  note text,
  is_handstand boolean not null default false,
  sort_order int not null default 0
);
create index if not exists idx_workout_exercises_day on public.workout_exercises(training_day_id);

-- ------------------------------------------------------------------ SESIONES
create table if not exists public.workout_sessions (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  day_code text not null,
  date date not null default current_date,
  status text not null default 'programado'
    check (status in ('programado','en-progreso','completado','parcial','omitido','reprogramado')),
  started_at timestamptz,
  completed_at timestamptz,
  total_volume numeric,
  -- Cuestionario post-entrenamiento
  feedback_difficulty smallint,
  feedback_had_pain boolean,
  feedback_pain_note text,
  feedback_energy smallint,
  feedback_technique smallint,
  feedback_slept_well boolean,
  feedback_repeat_weight boolean,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  deleted_at timestamptz
);
create index if not exists idx_workout_sessions_user_date on public.workout_sessions(user_id, date);

create table if not exists public.exercise_sets (
  id uuid primary key default gen_random_uuid(),
  session_id uuid not null references public.workout_sessions(id) on delete cascade,
  exercise_id text not null references public.exercises(id),
  set_number int not null,
  reps int,
  weight numeric,
  time_seconds int,
  rir smallint,
  rpe smallint,
  status text not null default 'pending' check (status in ('pending','done')),
  note text,
  created_at timestamptz not null default now()
);
create index if not exists idx_exercise_sets_session on public.exercise_sets(session_id);

-- --------------------------------------------------------------- HANDSTAND
create table if not exists public.handstand_sessions (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  date date not null default current_date,
  attempts int not null default 0,
  successful_attempts int not null default 0,
  best_seconds numeric not null default 0,
  average_seconds numeric not null default 0,
  entry_method text not null default 'pared'
    check (entry_method in ('kick-up','tuck','pared','otro')),
  technique_quality smallint not null default 3,
  level_id smallint not null default 1,
  note text,
  video_url text,
  created_at timestamptz not null default now()
);
create index if not exists idx_handstand_user_date on public.handstand_sessions(user_id, date);

-- --------------------------------------------------------------- MÉTRICAS
create table if not exists public.body_metrics (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  date date not null default current_date,
  body_weight numeric,
  notes text,
  created_at timestamptz not null default now()
);
create index if not exists idx_body_metrics_user_date on public.body_metrics(user_id, date);

create table if not exists public.progress_tests (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  date date not null default current_date,
  max_pullups int,
  max_dips int,
  handstand_best_seconds numeric,
  created_at timestamptz not null default now()
);
create index if not exists idx_progress_tests_user_date on public.progress_tests(user_id, date);

create table if not exists public.pain_reports (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  date date not null default current_date,
  area text not null,
  severity smallint not null check (severity between 1 and 5),
  exercise_id text references public.exercises(id),
  note text,
  created_at timestamptz not null default now()
);
create index if not exists idx_pain_reports_user_date on public.pain_reports(user_id, date);

-- --------------------------------------------------------------- CHECKLIST
create table if not exists public.weekly_checklists (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  week_start date not null,
  items jsonb not null default '{}',
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  unique (user_id, week_start)
);
create index if not exists idx_weekly_checklists_user on public.weekly_checklists(user_id);

-- ------------------------------------------------ EVALUACIÓN DE FRECUENCIA
create table if not exists public.progression_assessments (
  id uuid primary key default gen_random_uuid(),
  user_id uuid not null references auth.users(id) on delete cascade,
  date date not null default current_date,
  current_frequency smallint not null,
  target_frequency smallint not null,
  eligible boolean not null default false,
  completion_rate numeric,
  reasons text[] not null default '{}',
  recommendation text,
  created_at timestamptz not null default now()
);
create index if not exists idx_progression_assessments_user on public.progression_assessments(user_id);

-- --------------------------------------------------------------- TRIGGERS
do $$
declare t text;
begin
  foreach t in array array[
    'user_profiles','user_preferences','training_plans','training_days',
    'workout_sessions','weekly_checklists','exercise_user_notes'
  ] loop
    execute format(
      'drop trigger if exists trg_set_updated_at on public.%I;', t);
    execute format(
      'create trigger trg_set_updated_at before update on public.%I
       for each row execute function public.set_updated_at();', t);
  end loop;
end$$;

-- --------------------------------------------------------------- ver policies.sql
-- Las políticas de Row Level Security se definen en policies.sql.
