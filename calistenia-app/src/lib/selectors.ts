import type {
  HandstandSession,
  TrainingDay,
  WorkoutSession,
} from "@/lib/types";
import { TRAINING_DAYS } from "@/lib/data/routine";
import { effectiveWeekday } from "@/lib/store";
import { toISODate, weekStartISO } from "@/lib/utils";

/** Ordered list of training days for the week, honoring weekday overrides. */
export function orderedDays(
  overrides: Partial<Record<TrainingDay["code"], number>>,
): { day: TrainingDay; weekday: number }[] {
  return TRAINING_DAYS.map((day) => ({
    day,
    weekday: effectiveWeekday(day.code, overrides),
  })).sort((a, b) => a.weekday - b.weekday);
}

/** The training day scheduled for today, if any. */
export function todaysDay(
  overrides: Partial<Record<TrainingDay["code"], number>>,
): TrainingDay | null {
  const wd = new Date().getDay();
  const match = orderedDays(overrides).find((d) => d.weekday === wd);
  return match?.day ?? null;
}

/** The next upcoming training day (today counts if not completed). */
export function nextDay(
  overrides: Partial<Record<TrainingDay["code"], number>>,
): { day: TrainingDay; weekday: number } | null {
  const today = new Date().getDay();
  const days = orderedDays(overrides);
  if (days.length === 0) return null;
  const upcoming =
    days.find((d) => d.weekday >= today) ?? days[0];
  return upcoming ?? null;
}

export function sessionsThisWeek(sessions: WorkoutSession[]): WorkoutSession[] {
  const wk = weekStartISO();
  return sessions.filter((s) => weekStartISO(new Date(s.date + "T00:00:00")) === wk);
}

export function completedThisWeek(sessions: WorkoutSession[]): number {
  return sessionsThisWeek(sessions).filter((s) => s.status === "completado")
    .length;
}

/** Consecutive-week streak with at least one completed session. */
export function weeklyStreak(sessions: WorkoutSession[]): number {
  const completed = sessions.filter((s) => s.status === "completado");
  if (completed.length === 0) return 0;
  const weeks = new Set(
    completed.map((s) => weekStartISO(new Date(s.date + "T00:00:00"))),
  );
  let streak = 0;
  const cursor = new Date();
  // walk back week by week while each week has a completed session
  for (let i = 0; i < 104; i++) {
    const wk = weekStartISO(cursor);
    if (weeks.has(wk)) {
      streak++;
      cursor.setDate(cursor.getDate() - 7);
    } else if (i === 0) {
      // allow current (in-progress) week to be empty without breaking streak
      cursor.setDate(cursor.getDate() - 7);
    } else {
      break;
    }
  }
  return streak;
}

export function bestHandstandSeconds(sessions: HandstandSession[]): number {
  return sessions.reduce((m, s) => Math.max(m, s.bestSeconds), 0);
}

export function averageHandstandSeconds(sessions: HandstandSession[]): number {
  if (sessions.length === 0) return 0;
  const sum = sessions.reduce((s, h) => s + h.averageSeconds, 0);
  return Math.round((sum / sessions.length) * 10) / 10;
}

export function totalWeeklyVolume(sessions: WorkoutSession[]): number {
  return sessionsThisWeek(sessions)
    .filter((s) => s.status === "completado")
    .reduce((sum, s) => sum + (s.totalVolume ?? 0), 0);
}

/** Count of distinct weeks that contain any (non-reprogrammed) session. */
export function weeksTrained(sessions: WorkoutSession[]): number {
  const weeks = new Set(
    sessions
      .filter((s) => s.status !== "reprogramado")
      .map((s) => weekStartISO(new Date(s.date + "T00:00:00"))),
  );
  return weeks.size;
}

export function last(dateAscending: { date: string }[]): string | null {
  if (dateAscending.length === 0) return null;
  const sorted = [...dateAscending].sort((a, b) => a.date.localeCompare(b.date));
  return sorted[sorted.length - 1]?.date ?? null;
}

export { toISODate };
