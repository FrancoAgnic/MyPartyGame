"use client";

import Link from "next/link";
import { Dumbbell, ChevronRight, Clock } from "lucide-react";
import { useAppStore } from "@/lib/store";
import { useMounted } from "@/lib/hooks";
import { Card, Button, StatusBadge, SectionHeading } from "@/components/ui/primitives";
import { WarmupList } from "@/components/workout/WarmupList";
import { orderedDays } from "@/lib/selectors";
import { TRAINING_DAYS, totalPrescribedSets } from "@/lib/data/routine";
import { weekdayLong, todayISO } from "@/lib/utils";

export default function EntrenarPage() {
  const mounted = useMounted();
  const { weekdayOverrides, sessions } = useAppStore();

  const days = mounted ? orderedDays(weekdayOverrides) : TRAINING_DAYS.map((day) => ({ day, weekday: day.weekday }));
  const todayWeekday = new Date().getDay();

  return (
    <div className="space-y-6">
      <header>
        <h1 className="text-2xl font-bold">Entrenamiento</h1>
        <p className="text-sm text-muted">
          Programa inicial de 3 días · calistenia + hipertrofia + handstand
        </p>
      </header>

      <WarmupList />

      <section>
        <SectionHeading title="Rutina semanal" subtitle="Lun · Mié · Vie por defecto" />
        <div className="space-y-3">
          {days.map(({ day, weekday }) => {
            const isToday = weekday === todayWeekday;
            const todaySession = mounted
              ? sessions.find(
                  (s) => s.dayCode === day.code && s.date === todayISO(),
                )
              : undefined;
            const exerciseCount = day.blocks.reduce(
              (n, b) => n + b.exercises.length,
              0,
            );
            return (
              <Link key={day.code} href={`/entrenar/${day.code}`}>
                <Card
                  className={
                    isToday
                      ? "border-primary/40 transition-colors hover:border-primary"
                      : "transition-colors hover:border-border/80"
                  }
                >
                  <div className="flex items-center gap-4">
                    <div className="flex h-12 w-12 shrink-0 items-center justify-center rounded-xl bg-primary/15 text-lg font-bold text-primary">
                      {day.code}
                    </div>
                    <div className="min-w-0 flex-1">
                      <div className="flex items-center gap-2">
                        <span className="text-xs font-medium text-muted">
                          {weekdayLong(weekday)}
                        </span>
                        {isToday ? (
                          <span className="rounded-full bg-primary/15 px-2 py-0.5 text-[10px] font-bold text-primary">
                            HOY
                          </span>
                        ) : null}
                        {todaySession ? (
                          <StatusBadge status={todaySession.status} />
                        ) : null}
                      </div>
                      <h3 className="truncate font-semibold">{day.title}</h3>
                      <div className="mt-1 flex items-center gap-3 text-xs text-muted">
                        <span className="flex items-center gap-1">
                          <Dumbbell size={12} /> {exerciseCount} ejercicios
                        </span>
                        <span className="flex items-center gap-1">
                          <Clock size={12} /> {totalPrescribedSets(day)} series
                        </span>
                      </div>
                    </div>
                    <ChevronRight size={20} className="shrink-0 text-muted" />
                  </div>
                </Card>
              </Link>
            );
          })}
        </div>
      </section>

      <Card className="bg-surface-2/50">
        <p className="text-sm text-muted">
          ¿Querés reprogramar los días o ajustar la rutina? Cambiá los días desde
          el{" "}
          <Link href="/calendario" className="font-semibold text-primary">
            calendario
          </Link>
          .
        </p>
      </Card>
    </div>
  );
}
