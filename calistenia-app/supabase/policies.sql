-- =============================================================================
-- Row Level Security (RLS) — ejecutar después de schema.sql
-- =============================================================================
-- Regla general: cada usuario sólo puede leer/escribir sus propias filas
-- (user_id = auth.uid()). La biblioteca de ejercicios y sus medios son
-- contenido global de sólo lectura para usuarios autenticados.
-- =============================================================================

-- Habilitar RLS en todas las tablas de usuario
alter table public.user_profiles          enable row level security;
alter table public.user_preferences        enable row level security;
alter table public.training_plans          enable row level security;
alter table public.training_days           enable row level security;
alter table public.workout_exercises       enable row level security;
alter table public.workout_sessions        enable row level security;
alter table public.exercise_sets           enable row level security;
alter table public.handstand_sessions      enable row level security;
alter table public.body_metrics            enable row level security;
alter table public.progress_tests          enable row level security;
alter table public.pain_reports            enable row level security;
alter table public.weekly_checklists       enable row level security;
alter table public.progression_assessments enable row level security;
alter table public.exercise_user_notes     enable row level security;
alter table public.exercises               enable row level security;
alter table public.exercise_media          enable row level security;

-- ---------------------------------------------------------- Tablas por usuario
-- Macro repetida para cada tabla con columna user_id.
do $$
declare t text;
begin
  foreach t in array array[
    'user_profiles','user_preferences','training_plans','handstand_sessions',
    'body_metrics','progress_tests','pain_reports','weekly_checklists',
    'progression_assessments','exercise_user_notes','workout_sessions'
  ] loop
    execute format('drop policy if exists "own_select" on public.%I;', t);
    execute format('drop policy if exists "own_modify" on public.%I;', t);
    execute format(
      'create policy "own_select" on public.%I for select
       using (user_id = auth.uid());', t);
    execute format(
      'create policy "own_modify" on public.%I for all
       using (user_id = auth.uid()) with check (user_id = auth.uid());', t);
  end loop;
end$$;

-- --------------------------------- Tablas hijas (propiedad vía tabla padre)
-- training_days -> training_plans (user_id)
drop policy if exists "days_owner" on public.training_days;
create policy "days_owner" on public.training_days for all
  using (exists (
    select 1 from public.training_plans p
    where p.id = plan_id and p.user_id = auth.uid()))
  with check (exists (
    select 1 from public.training_plans p
    where p.id = plan_id and p.user_id = auth.uid()));

-- workout_exercises -> training_days -> training_plans
drop policy if exists "wex_owner" on public.workout_exercises;
create policy "wex_owner" on public.workout_exercises for all
  using (exists (
    select 1 from public.training_days d
    join public.training_plans p on p.id = d.plan_id
    where d.id = training_day_id and p.user_id = auth.uid()))
  with check (exists (
    select 1 from public.training_days d
    join public.training_plans p on p.id = d.plan_id
    where d.id = training_day_id and p.user_id = auth.uid()));

-- exercise_sets -> workout_sessions (user_id)
drop policy if exists "sets_owner" on public.exercise_sets;
create policy "sets_owner" on public.exercise_sets for all
  using (exists (
    select 1 from public.workout_sessions s
    where s.id = session_id and s.user_id = auth.uid()))
  with check (exists (
    select 1 from public.workout_sessions s
    where s.id = session_id and s.user_id = auth.uid()));

-- --------------------------------------------- Biblioteca global (lectura)
drop policy if exists "exercises_read" on public.exercises;
create policy "exercises_read" on public.exercises for select
  using (auth.role() = 'authenticated');

drop policy if exists "exercise_media_read" on public.exercise_media;
create policy "exercise_media_read" on public.exercise_media for select
  using (auth.role() = 'authenticated');

-- =============================================================================
-- Alta automática de perfil + preferencias al registrarse
-- =============================================================================
create or replace function public.handle_new_user()
returns trigger language plpgsql security definer set search_path = public as $$
begin
  insert into public.user_profiles (user_id, name)
  values (new.id, coalesce(new.raw_user_meta_data->>'name', 'Atleta'))
  on conflict (user_id) do nothing;
  insert into public.user_preferences (user_id)
  values (new.id)
  on conflict (user_id) do nothing;
  return new;
end;
$$;

drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created
  after insert on auth.users
  for each row execute function public.handle_new_user();
