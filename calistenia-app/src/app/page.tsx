"use client";

import Link from "next/link";
import {
  Dumbbell,
  Flame,
  Activity,
  Trophy,
  Scale,
  ArrowRight,
  CalendarClock,
  HeartPulse,
} from "lucide-react";
import { useAppStore } from "@/lib/store";
import { useMounted } from "@/lib/hooks";
import {
  Card,
  Button,
  StatTile,
  ProgressBar,
  SectionHeading,
} from "@/components/ui/primitives";
import {
  bestHandstandSeconds,
  completedThisWeek,
  nextDay,
  todaysDay,
  totalWeeklyVolume,
  weeklyStreak,
  weeksTrained,
} from "@/lib/selectors";
import { assessFrequencyProgression } from "@/lib/progression";
import { last } from "@/lib/selectors";

export default function HomePage() {
  const mounted = useMounted();
  const store = useAppStore();

  if (!mounted) return <HomeSkeleton />;

  const {
    profile,
    preferences,
    weekdayOverrides,
    sessions,
    handstandSessions,
    bodyMetrics,
    progressTests,
  } = store;

  const today = todaysDay(weekdayOverrides);
  const upcoming = nextDay(weekdayOverrides);
  const completed = completedThisWeek(sessions);
  const streak = weeklyStreak(sessions);
  const bestHs = bestHandstandSeconds(handstandSessions) || profile.freeHandstandSeconds;
  const latestBodyDate = last(bodyMetrics);
  const latestWeight = bodyMetrics.find((m) => m.date === latestBodyDate)?.bodyWeight;
  const latestTest = progressTests.length
    ? [...progressTests].sort((a, b) => a.date.localeCompare(b.date)).at(-1)
    : undefined;
  const maxPullups = latestTest?.maxPullups ?? profile.strictPullups;
  const maxDips = latestTest?.maxDips ?? profile.freeDips;
  const weeklyVolume = totalWeeklyVolume(sessions);

  const assessment = assessFrequencyProgression({
    currentFrequency: profile.frequency,
    sessions,
    weeksTrained: Math.max(weeksTrained(sessions), 0),
  });

  const target = profile.frequency;
  const progressToTarget = Math.min(1, completed / target);

  return (
    <div className="space-y-6">
      <header>
        <p className="text-sm text-muted">¡Hola de nuevo!</p>
        <h1 className="text-2xl font-bold">{profile.name} 👋</h1>
      </header>

      {/* Today's workout hero */}
      <Card className="bg-gradient-to-br from-primary/15 to-accent/10">
        <div className="flex items-center gap-2 text-sm text-muted">
          <CalendarClock size={16} />
          Entrenamiento de hoy
        </div>
        {today ? (
          <>
            <h2 className="mt-1 text-xl font-bold">{today.title}</h2>
            <p className="mt-1 text-sm text-muted">{today.focus}</p>
            <div className="mt-4 flex flex-wrap gap-2">
              <Link href={`/entrenar/${today.code}`}>
                <Button size="lg">
                  <Dumbbell size={18} /> Comenzar entrenamiento
                </Button>
              </Link>
              <Link href="/entrenar">
                <Button size="lg" variant="outline">
                  Ver rutina
                </Button>
              </Link>
            </div>
          </>
        ) : (
          <>
            <h2 className="mt-1 text-xl font-bold">Día de descanso</h2>
            <p className="mt-1 text-sm text-muted">
              {upcoming
                ? `Próximo: ${upcoming.day.title}.`
                : "Configurá tu rutina en el calendario."}
            </p>
            <div className="mt-4 flex flex-wrap gap-2">
              {upcoming ? (
                <Link href={`/entrenar/${upcoming.day.code}`}>
                  <Button size="lg" variant="secondary">
                    Adelantar próximo entrenamiento
                  </Button>
                </Link>
              ) : null}
              <Link href="/handstand">
                <Button size="lg" variant="outline">
                  <Activity size={18} /> Practicar handstand
                </Button>
              </Link>
            </div>
          </>
        )}
      </Card>

      {/* Weekly stats grid */}
      <section>
        <SectionHeading
          title="Tu semana"
          subtitle={`${completed} de ${target} sesiones completadas`}
        />
        <div className="grid grid-cols-2 gap-3 sm:grid-cols-3">
          <StatTile
            label="Racha semanal"
            value={streak}
            unit="sem"
            icon={<Flame size={14} />}
          />
          <StatTile
            label="Handstand máx."
            value={bestHs}
            unit="s"
            icon={<Activity size={14} />}
          />
          <StatTile
            label="Dominadas máx."
            value={maxPullups}
            icon={<Trophy size={14} />}
          />
          <StatTile
            label="Fondos máx."
            value={maxDips}
            icon={<Dumbbell size={14} />}
          />
          <StatTile
            label="Peso corporal"
            value={latestWeight ?? "—"}
            unit={latestWeight ? preferences.units : undefined}
            icon={<Scale size={14} />}
          />
          <StatTile
            label="Volumen semanal"
            value={Math.round(weeklyVolume)}
            unit={preferences.units}
            icon={<Flame size={14} />}
          />
        </div>
      </section>

      {/* Frequency progression */}
      <Card>
        <SectionHeading
          title={`Progreso hacia ${assessment.targetFrequency} días`}
          subtitle={`Entrenás ${profile.frequency} días por semana`}
        />
        <ProgressBar value={progressToTarget} tone="accent" />
        <p className="mt-3 text-sm text-muted">{assessment.recommendation}</p>
        <Link
          href="/progreso#frecuencia"
          className="mt-3 inline-flex items-center gap-1 text-sm font-semibold text-primary"
        >
          Ver evaluación completa <ArrowRight size={14} />
        </Link>
      </Card>

      {/* Recovery recommendation */}
      <Card className="border-warning/30 bg-warning/5">
        <div className="flex items-start gap-3">
          <HeartPulse className="mt-0.5 shrink-0 text-warning" size={20} />
          <div>
            <h3 className="font-semibold">Recuperación</h3>
            <p className="mt-1 text-sm text-muted">
              Priorizá 7–9 h de sueño y proteína suficiente. Si sentís dolor
              articular persistente, mareo u hormigueo, detené el ejercicio y
              registralo. Esta app no reemplaza a un profesional de la salud.
            </p>
          </div>
        </div>
      </Card>
    </div>
  );
}

function HomeSkeleton() {
  return (
    <div className="space-y-6">
      <div className="h-8 w-40 animate-pulse rounded bg-surface-2" />
      <div className="h-40 animate-pulse rounded-2xl bg-surface-2" />
      <div className="grid grid-cols-2 gap-3 sm:grid-cols-3">
        {Array.from({ length: 6 }).map((_, i) => (
          <div key={i} className="h-20 animate-pulse rounded-2xl bg-surface-2" />
        ))}
      </div>
    </div>
  );
}
