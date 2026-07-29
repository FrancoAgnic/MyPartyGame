"use client";

import { useMemo, useState } from "react";
import { ChevronLeft, ChevronRight } from "lucide-react";
import { useAppStore, effectiveWeekday } from "@/lib/store";
import { useMounted } from "@/lib/hooks";
import { Card, SectionHeading } from "@/components/ui/primitives";
import { TRAINING_DAYS } from "@/lib/data/routine";
import { CHECKLIST_KEYS, CHECKLIST_LABELS } from "@/lib/data/defaults";
import { orderedDays } from "@/lib/selectors";
import { toISODate, weekStartISO, weekdayLong, weekdayLabel } from "@/lib/utils";
import type { TrainingDay, WorkoutStatus } from "@/lib/types";
import { cn } from "@/lib/utils";

const STATUS_DOT: Record<WorkoutStatus, string> = {
  programado: "bg-muted",
  "en-progreso": "bg-accent",
  completado: "bg-success",
  parcial: "bg-warning",
  omitido: "bg-danger",
  reprogramado: "bg-muted",
};

const WEEKDAY_OPTIONS = [1, 2, 3, 4, 5, 6, 0]; // Mon..Sun

export default function CalendarioPage() {
  const mounted = useMounted();
  const store = useAppStore();
  const [monthOffset, setMonthOffset] = useState(0);

  const base = new Date();
  base.setDate(1);
  base.setMonth(base.getMonth() + monthOffset);
  const year = base.getFullYear();
  const month = base.getMonth();

  const monthLabel = base.toLocaleDateString("es-AR", {
    month: "long",
    year: "numeric",
  });

  // Map ISO date -> planned day code (based on weekday) and session status.
  const plannedByWeekday = useMemo(() => {
    const map = new Map<number, TrainingDay>();
    TRAINING_DAYS.forEach((d) => {
      map.set(effectiveWeekday(d.code, store.weekdayOverrides), d);
    });
    return map;
  }, [store.weekdayOverrides]);

  const sessionByDate = useMemo(() => {
    const map = new Map<string, WorkoutStatus>();
    store.sessions.forEach((s) => {
      // keep the "most complete" status if multiple
      const rank: Record<WorkoutStatus, number> = {
        completado: 5,
        "en-progreso": 4,
        parcial: 3,
        programado: 2,
        reprogramado: 1,
        omitido: 0,
      };
      const prev = map.get(s.date);
      if (!prev || rank[s.status] > rank[prev]) map.set(s.date, s.status);
    });
    return map;
  }, [store.sessions]);

  const firstWeekday = (new Date(year, month, 1).getDay() + 6) % 7; // Mon=0
  const daysInMonth = new Date(year, month + 1, 0).getDate();
  const cells: (number | null)[] = [
    ...Array.from({ length: firstWeekday }, () => null),
    ...Array.from({ length: daysInMonth }, (_, i) => i + 1),
  ];

  const currentWeek = weekStartISO();
  const checklist = mounted
    ? store.checklists.find((c) => c.weekStart === currentWeek)
    : undefined;

  const days = mounted
    ? orderedDays(store.weekdayOverrides)
    : TRAINING_DAYS.map((day) => ({ day, weekday: day.weekday }));

  return (
    <div className="space-y-6">
      <header>
        <h1 className="text-2xl font-bold">Calendario</h1>
        <p className="text-sm text-muted">Rutina, estados y checklist semanal</p>
      </header>

      {/* Month grid */}
      <Card>
        <div className="mb-3 flex items-center justify-between">
          <button
            onClick={() => setMonthOffset((o) => o - 1)}
            className="rounded-lg p-1.5 hover:bg-surface-2"
            aria-label="Mes anterior"
          >
            <ChevronLeft size={18} />
          </button>
          <h2 className="font-semibold capitalize">{monthLabel}</h2>
          <button
            onClick={() => setMonthOffset((o) => o + 1)}
            className="rounded-lg p-1.5 hover:bg-surface-2"
            aria-label="Mes siguiente"
          >
            <ChevronRight size={18} />
          </button>
        </div>
        <div className="grid grid-cols-7 gap-1 text-center text-[11px] text-muted">
          {["L", "M", "M", "J", "V", "S", "D"].map((d, i) => (
            <div key={i} className="py-1 font-semibold">
              {d}
            </div>
          ))}
          {cells.map((day, i) => {
            if (day == null) return <div key={i} />;
            const date = new Date(year, month, day);
            const iso = toISODate(date);
            const weekday = date.getDay();
            const planned = plannedByWeekday.get(weekday);
            const status = sessionByDate.get(iso);
            const isToday = iso === toISODate(new Date());
            return (
              <div
                key={i}
                className={cn(
                  "flex aspect-square flex-col items-center justify-center rounded-lg text-xs",
                  planned ? "bg-surface-2/60" : "",
                  isToday && "ring-1 ring-primary",
                )}
              >
                <span className={cn("font-medium", isToday && "text-primary")}>
                  {day}
                </span>
                {planned ? (
                  <span className="text-[9px] font-bold text-primary">
                    {planned.code}
                  </span>
                ) : null}
                {status ? (
                  <span
                    className={cn("mt-0.5 h-1.5 w-1.5 rounded-full", STATUS_DOT[status])}
                  />
                ) : null}
              </div>
            );
          })}
        </div>
        <div className="mt-3 flex flex-wrap gap-3 text-[11px] text-muted">
          <Legend color="bg-success" label="Completado" />
          <Legend color="bg-warning" label="Parcial" />
          <Legend color="bg-danger" label="Omitido" />
          <Legend color="bg-accent" label="En progreso" />
        </div>
      </Card>

      {/* Reprogram days */}
      <section>
        <SectionHeading title="Reprogramar días" subtitle="Cambiá el día de la semana de cada entrenamiento" />
        <div className="space-y-2">
          {days.map(({ day, weekday }) => (
            <Card key={day.code} className="flex items-center gap-3 p-3">
              <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-primary/15 font-bold text-primary">
                {day.code}
              </div>
              <div className="min-w-0 flex-1">
                <div className="truncate text-sm font-medium">{day.title}</div>
                <div className="text-xs text-muted">{weekdayLong(weekday)}</div>
              </div>
              <select
                value={weekday}
                onChange={(e) => store.setWeekday(day.code, Number(e.target.value))}
                className="h-9 rounded-lg border border-border bg-surface-2 px-2 text-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-primary"
                aria-label={`Día de ${day.title}`}
              >
                {WEEKDAY_OPTIONS.map((wd) => (
                  <option key={wd} value={wd}>
                    {weekdayLabel(wd)}
                  </option>
                ))}
              </select>
            </Card>
          ))}
        </div>
      </section>

      {/* Weekly checklist */}
      <section>
        <SectionHeading title="Checklist semanal" />
        <Card>
          <ul className="divide-y divide-border">
            {CHECKLIST_KEYS.map((key) => {
              const checked = checklist?.items[key] ?? false;
              return (
                <li key={key}>
                  <label className="flex cursor-pointer items-center gap-3 py-2.5">
                    <input
                      type="checkbox"
                      checked={checked}
                      onChange={() => store.toggleChecklist(currentWeek, key)}
                      className="h-5 w-5 accent-primary"
                    />
                    <span
                      className={cn(
                        "text-sm",
                        checked && "text-muted line-through",
                      )}
                    >
                      {CHECKLIST_LABELS[key]}
                    </span>
                  </label>
                </li>
              );
            })}
          </ul>
        </Card>
      </section>
    </div>
  );
}

function Legend({ color, label }: { color: string; label: string }) {
  return (
    <span className="flex items-center gap-1.5">
      <span className={cn("h-2 w-2 rounded-full", color)} />
      {label}
    </span>
  );
}
