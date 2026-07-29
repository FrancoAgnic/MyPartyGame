"use client";

import { useMemo, useState } from "react";
import { CheckCircle2, XCircle } from "lucide-react";
import { useAppStore } from "@/lib/store";
import { useMounted } from "@/lib/hooks";
import { Card, SectionHeading, EmptyState } from "@/components/ui/primitives";
import { TrendChart, type ChartPoint } from "@/components/charts/TrendChart";
import {
  assessFrequencyProgression,
  FIFTH_DAY_OPTIONS,
  FOURTH_DAY_CONTENT,
} from "@/lib/progression";
import { weeksTrained } from "@/lib/selectors";
import { formatDate, weekStartISO } from "@/lib/utils";
import type { WorkoutSession } from "@/lib/types";

type Range = "week" | "month" | "3m" | "6m" | "all";

const RANGE_LABELS: Record<Range, string> = {
  week: "Semana",
  month: "Mes",
  "3m": "3 meses",
  "6m": "6 meses",
  all: "Todo",
};

const RANGE_DAYS: Record<Range, number> = {
  week: 7,
  month: 31,
  "3m": 93,
  "6m": 186,
  all: 100000,
};

function withinRange(dateISO: string, range: Range): boolean {
  const days = RANGE_DAYS[range];
  const d = new Date(dateISO + "T00:00:00").getTime();
  return Date.now() - d <= days * 86400000;
}

/** Max weight lifted for an exercise in each session, ordered by date. */
function exerciseWeightSeries(
  sessions: WorkoutSession[],
  exerciseId: string,
  range: Range,
): ChartPoint[] {
  return sessions
    .filter((s) => withinRange(s.date, range))
    .map((s) => {
      const weights = s.sets
        .filter((set) => set.exerciseId === exerciseId && set.weight != null)
        .map((set) => set.weight as number);
      return { date: s.date, max: weights.length ? Math.max(...weights) : null };
    })
    .filter((p): p is { date: string; max: number } => p.max != null)
    .sort((a, b) => a.date.localeCompare(b.date))
    .map((p) => ({ label: formatDate(p.date), value: p.max }));
}

export default function ProgresoPage() {
  const mounted = useMounted();
  const store = useAppStore();
  const [range, setRange] = useState<Range>("3m");

  const {
    sessions,
    handstandSessions,
    bodyMetrics,
    profile,
  } = store;

  const bodyWeightSeries: ChartPoint[] = useMemo(
    () =>
      bodyMetrics
        .filter((m) => m.bodyWeight != null && withinRange(m.date, range))
        .sort((a, b) => a.date.localeCompare(b.date))
        .map((m) => ({ label: formatDate(m.date), value: m.bodyWeight as number })),
    [bodyMetrics, range],
  );

  const handstandSeries: ChartPoint[] = useMemo(
    () =>
      handstandSessions
        .filter((s) => withinRange(s.date, range))
        .sort((a, b) => a.date.localeCompare(b.date))
        .map((s) => ({ label: formatDate(s.date), value: s.bestSeconds })),
    [handstandSessions, range],
  );

  const volumeByWeek: ChartPoint[] = useMemo(() => {
    const map = new Map<string, number>();
    sessions
      .filter((s) => s.status === "completado" && withinRange(s.date, range))
      .forEach((s) => {
        const wk = weekStartISO(new Date(s.date + "T00:00:00"));
        map.set(wk, (map.get(wk) ?? 0) + (s.totalVolume ?? 0));
      });
    return [...map.entries()]
      .sort((a, b) => a[0].localeCompare(b[0]))
      .map(([wk, v]) => ({ label: formatDate(wk), value: Math.round(v) }));
  }, [sessions, range]);

  const completedByWeek: ChartPoint[] = useMemo(() => {
    const map = new Map<string, number>();
    sessions
      .filter((s) => s.status === "completado" && withinRange(s.date, range))
      .forEach((s) => {
        const wk = weekStartISO(new Date(s.date + "T00:00:00"));
        map.set(wk, (map.get(wk) ?? 0) + 1);
      });
    return [...map.entries()]
      .sort((a, b) => a[0].localeCompare(b[0]))
      .map(([wk, v]) => ({ label: formatDate(wk), value: v }));
  }, [sessions, range]);

  const assessment = useMemo(
    () =>
      assessFrequencyProgression({
        currentFrequency: profile.frequency,
        sessions,
        weeksTrained: weeksTrained(sessions),
      }),
    [profile.frequency, sessions],
  );

  if (!mounted) {
    return <div className="h-64 animate-pulse rounded-2xl bg-surface-2" />;
  }

  return (
    <div className="space-y-6">
      <header>
        <h1 className="text-2xl font-bold">Progreso</h1>
        <p className="text-sm text-muted">Fuerza, masa muscular y skills</p>
      </header>

      {/* Range filter */}
      <div className="flex flex-wrap gap-2">
        {(Object.keys(RANGE_LABELS) as Range[]).map((r) => (
          <button
            key={r}
            onClick={() => setRange(r)}
            className={
              "rounded-full px-3 py-1.5 text-sm font-medium transition-colors " +
              (range === r
                ? "bg-primary text-primary-fg"
                : "bg-surface-2 text-muted hover:text-fg")
            }
          >
            {RANGE_LABELS[r]}
          </button>
        ))}
      </div>

      <ChartCard title="Peso corporal" unit={store.preferences.units} data={bodyWeightSeries} />
      <ChartCard title="Tiempo máx. de handstand" unit="s" data={handstandSeries} color="rgb(34 211 238)" />
      <ChartCard title="Entrenamientos completados por semana" data={completedByWeek} type="bar" color="rgb(34 197 94)" />
      <ChartCard title="Volumen semanal" unit={store.preferences.units} data={volumeByWeek} type="bar" />

      <ChartCard
        title="Press de hombros (Smith)"
        unit={store.preferences.units}
        data={exerciseWeightSeries(sessions, "smith-shoulder-press", range)}
      />
      <ChartCard
        title="Press inclinado con mancuernas"
        unit={store.preferences.units}
        data={exerciseWeightSeries(sessions, "incline-db-press", range)}
      />
      <ChartCard
        title="Jalón al pecho"
        unit={store.preferences.units}
        data={exerciseWeightSeries(sessions, "lat-pulldown", range)}
      />
      <ChartCard
        title="Remo sentado en polea"
        unit={store.preferences.units}
        data={exerciseWeightSeries(sessions, "seated-cable-row", range)}
      />

      {/* Frequency assessment */}
      <section id="frecuencia" className="scroll-mt-4">
        <SectionHeading
          title="Evaluación de frecuencia"
          subtitle={`${profile.frequency} días → ${assessment.targetFrequency} días`}
        />
        <Card>
          <div
            className={
              "mb-3 inline-flex items-center gap-2 rounded-full px-3 py-1 text-sm font-semibold " +
              (assessment.eligible
                ? "bg-success/15 text-success"
                : "bg-warning/15 text-warning")
            }
          >
            {assessment.eligible ? (
              <CheckCircle2 size={16} />
            ) : (
              <XCircle size={16} />
            )}
            {assessment.eligible
              ? "Listo para aumentar"
              : "Todavía no conviene aumentar"}
          </div>
          <p className="text-sm text-muted">{assessment.recommendation}</p>
          {assessment.reasons.length > 0 ? (
            <ul className="mt-3 space-y-1 text-sm">
              {assessment.reasons.map((r, i) => (
                <li key={i} className="flex gap-2 text-muted">
                  <span className="text-warning">•</span>
                  {r}
                </li>
              ))}
            </ul>
          ) : null}

          <div className="mt-4 grid gap-3 sm:grid-cols-2">
            <div className="rounded-xl bg-surface-2/60 p-3">
              <h4 className="text-sm font-bold">4.º día (ligero)</h4>
              <ul className="mt-1 space-y-0.5 text-xs text-muted">
                {FOURTH_DAY_CONTENT.map((c) => (
                  <li key={c}>• {c}</li>
                ))}
              </ul>
            </div>
            <div className="rounded-xl bg-surface-2/60 p-3">
              <h4 className="text-sm font-bold">Opciones para el 5.º día</h4>
              <ul className="mt-1 space-y-1 text-xs text-muted">
                {FIFTH_DAY_OPTIONS.map((o) => (
                  <li key={o.id}>
                    <span className="font-semibold text-fg">{o.title}:</span>{" "}
                    {o.items.join(", ")}
                  </li>
                ))}
              </ul>
            </div>
          </div>
        </Card>
      </section>
    </div>
  );
}

function ChartCard({
  title,
  unit,
  data,
  type = "line",
  color,
}: {
  title: string;
  unit?: string;
  data: ChartPoint[];
  type?: "line" | "bar";
  color?: string;
}) {
  return (
    <Card>
      <h3 className="mb-2 font-semibold">{title}</h3>
      <TrendChart data={data} unit={unit} type={type} color={color} />
    </Card>
  );
}
