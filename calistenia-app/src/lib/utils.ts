import { clsx, type ClassValue } from "clsx";
import { twMerge } from "tailwind-merge";

/** Tailwind-aware className combiner. */
export function cn(...inputs: ClassValue[]) {
  return twMerge(clsx(inputs));
}

/** ISO date (yyyy-mm-dd) in local time. */
export function toISODate(date: Date): string {
  const y = date.getFullYear();
  const m = String(date.getMonth() + 1).padStart(2, "0");
  const d = String(date.getDate()).padStart(2, "0");
  return `${y}-${m}-${d}`;
}

export function todayISO(): string {
  return toISODate(new Date());
}

/** Monday of the week containing `date`, as ISO date. */
export function weekStartISO(date: Date = new Date()): string {
  const d = new Date(date);
  const day = d.getDay(); // 0 Sun ... 6 Sat
  const diff = (day + 6) % 7; // days since Monday
  d.setDate(d.getDate() - diff);
  d.setHours(0, 0, 0, 0);
  return toISODate(d);
}

export function addDays(iso: string, days: number): string {
  const d = new Date(iso + "T00:00:00");
  d.setDate(d.getDate() + days);
  return toISODate(d);
}

const WEEKDAY_LABELS = ["Dom", "Lun", "Mar", "Mié", "Jue", "Vie", "Sáb"];
const WEEKDAY_LONG = [
  "Domingo",
  "Lunes",
  "Martes",
  "Miércoles",
  "Jueves",
  "Viernes",
  "Sábado",
];

export function weekdayLabel(weekday: number): string {
  return WEEKDAY_LABELS[weekday % 7] ?? "";
}

export function weekdayLong(weekday: number): string {
  return WEEKDAY_LONG[weekday % 7] ?? "";
}

export function formatDate(iso: string): string {
  const d = new Date(iso + "T00:00:00");
  return d.toLocaleDateString("es-AR", {
    day: "numeric",
    month: "short",
  });
}

export function formatSeconds(total: number): string {
  const m = Math.floor(total / 60);
  const s = total % 60;
  return `${m}:${String(s).padStart(2, "0")}`;
}

/** kg <-> lb conversion for display; storage is always in the user's chosen unit. */
export function repRangeLabel(min?: number, max?: number): string {
  if (min == null && max == null) return "";
  if (min != null && max != null) return `${min}–${max} reps`;
  return `${min ?? max} reps`;
}

export function timeRangeLabel(min?: number, max?: number): string {
  if (min == null && max == null) return "";
  if (min != null && max != null) return `${min}–${max}s`;
  return `${min ?? max}s`;
}

/** Simple unique id (client side). */
export function uid(prefix = "id"): string {
  return `${prefix}_${Date.now().toString(36)}_${Math.random()
    .toString(36)
    .slice(2, 8)}`;
}

export function clamp(n: number, min: number, max: number): number {
  return Math.min(max, Math.max(min, n));
}
