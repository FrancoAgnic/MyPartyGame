import type { TrainingDay } from "@/lib/types";

/**
 * Programa inicial de 3 días por semana. Distribución por defecto:
 * Lunes = Día A, Miércoles = Día B, Viernes = Día C. Los días de la semana
 * pueden reprogramarse desde el calendario (preferencias de usuario).
 */
export const TRAINING_DAYS: TrainingDay[] = [
  {
    id: "day-a",
    code: "A",
    title: "Día A — Empuje, Handstand y Pecho",
    focus: "Empuje vertical · Pecho · Hombros · Handstand fresco",
    weekday: 1, // Lunes
    objective: [
      "Mejorar fuerza de empuje vertical.",
      "Ganar masa muscular en hombros, pecho y tríceps.",
      "Practicar handstand estando fresco.",
    ],
    blocks: [
      {
        id: "day-a-handstand",
        title: "Bloque de handstand",
        exercises: [
          {
            exerciseId: "handstand-libre",
            sets: 10,
            timeSecondsMin: 5,
            timeSecondsMax: 15,
            restSeconds: 40,
            note: "8 a 12 intentos de 5–15s. Descanso 30–45s.",
            isHandstand: true,
          },
          {
            exerciseId: "handstand-pared",
            sets: 3,
            timeSecondsMin: 20,
            timeSecondsMax: 40,
            restSeconds: 75,
            note: "Cara a la pared. Descanso 60–90s.",
            isHandstand: true,
          },
        ],
      },
      {
        id: "day-a-fuerza",
        title: "Fuerza e hipertrofia",
        exercises: [
          {
            exerciseId: "smith-shoulder-press",
            sets: 4,
            repsMin: 6,
            repsMax: 8,
            restSeconds: 120,
          },
          {
            exerciseId: "dips",
            sets: 4,
            repsMin: 5,
            repsMax: 10,
            restSeconds: 120,
            note: "Libres, asistidos o negativas según nivel.",
          },
          {
            exerciseId: "incline-db-press",
            sets: 3,
            repsMin: 8,
            repsMax: 12,
            restSeconds: 90,
          },
          {
            exerciseId: "lateral-raises",
            sets: 3,
            repsMin: 12,
            repsMax: 20,
            restSeconds: 60,
          },
          {
            exerciseId: "pec-deck",
            sets: 3,
            repsMin: 10,
            repsMax: 15,
            restSeconds: 75,
          },
          {
            exerciseId: "triceps-pushdown",
            sets: 3,
            repsMin: 10,
            repsMax: 15,
            restSeconds: 60,
          },
        ],
      },
      {
        id: "day-a-core",
        title: "Core",
        exercises: [
          {
            exerciseId: "hollow-body-hold",
            sets: 3,
            timeSecondsMin: 20,
            timeSecondsMax: 40,
            restSeconds: 60,
          },
        ],
      },
    ],
  },
  {
    id: "day-b",
    code: "B",
    title: "Día B — Espalda, Bíceps y Core",
    focus: "Espalda · Bíceps · Escápulas · Postura",
    weekday: 3, // Miércoles
    objective: [
      "Aumentar masa muscular de espalda.",
      "Mejorar fuerza para dominadas.",
      "Fortalecer escápulas y postura.",
    ],
    blocks: [
      {
        id: "day-b-handstand",
        title: "Handstand técnico ligero",
        exercises: [
          {
            exerciseId: "handstand-tecnico",
            sets: 6,
            timeSecondsMin: 5,
            timeSecondsMax: 15,
            restSeconds: 45,
            note: "5–8 minutos. Intentos suaves, sin buscar fatiga.",
            isHandstand: true,
          },
        ],
      },
      {
        id: "day-b-fuerza",
        title: "Fuerza e hipertrofia",
        exercises: [
          {
            exerciseId: "pull-ups",
            sets: 5,
            repsMin: 2,
            repsMax: 5,
            restSeconds: 150,
            note: "1–2 reps en reserva. Usar asistencia si es necesario.",
          },
          {
            exerciseId: "lat-pulldown",
            sets: 3,
            repsMin: 8,
            repsMax: 12,
            restSeconds: 90,
          },
          {
            exerciseId: "seated-cable-row",
            sets: 4,
            repsMin: 8,
            repsMax: 12,
            restSeconds: 90,
          },
          {
            exerciseId: "chest-supported-row",
            sets: 3,
            repsMin: 10,
            repsMax: 12,
            restSeconds: 90,
          },
          {
            exerciseId: "face-pulls",
            sets: 3,
            repsMin: 12,
            repsMax: 20,
            restSeconds: 60,
          },
          {
            exerciseId: "hammer-curl",
            sets: 3,
            repsMin: 8,
            repsMax: 12,
            restSeconds: 60,
          },
        ],
      },
      {
        id: "day-b-core",
        title: "Core",
        exercises: [
          {
            exerciseId: "hanging-knee-raises",
            sets: 3,
            repsMin: 8,
            repsMax: 15,
            restSeconds: 60,
          },
        ],
      },
    ],
  },
  {
    id: "day-c",
    code: "C",
    title: "Día C — Handstand, Hombros y Tren Superior",
    focus: "Fuerza específica de handstand · Hombros · Tren superior",
    weekday: 5, // Viernes
    objective: [
      "Mejorar fuerza específica para handstand.",
      "Aumentar volumen de hombros.",
      "Reforzar pecho y espalda sin repetir demasiado volumen.",
    ],
    blocks: [
      {
        id: "day-c-handstand",
        title: "Bloque de handstand",
        exercises: [
          {
            exerciseId: "handstand-libre",
            sets: 10,
            timeSecondsMin: 3,
            timeSecondsMax: 12,
            restSeconds: 45,
            note: "10–15 minutos. Intentos cortos de buena calidad.",
            isHandstand: true,
          },
          {
            exerciseId: "pike-pushups",
            sets: 4,
            repsMin: 6,
            repsMax: 10,
            restSeconds: 90,
          },
          {
            exerciseId: "wall-shoulder-taps",
            sets: 3,
            repsMin: 4,
            repsMax: 10,
            restSeconds: 90,
            note: "Por lado.",
          },
          {
            exerciseId: "handstand-scapular-shrugs",
            sets: 3,
            repsMin: 8,
            repsMax: 15,
            restSeconds: 60,
          },
        ],
      },
      {
        id: "day-c-fuerza",
        title: "Hombros y tren superior",
        exercises: [
          {
            exerciseId: "db-shoulder-press",
            sets: 3,
            repsMin: 8,
            repsMax: 12,
            restSeconds: 90,
          },
          {
            exerciseId: "machine-row",
            sets: 3,
            repsMin: 10,
            repsMax: 15,
            restSeconds: 90,
          },
          {
            exerciseId: "machine-chest-press",
            sets: 3,
            repsMin: 8,
            repsMax: 12,
            restSeconds: 90,
          },
          {
            exerciseId: "lateral-raises",
            sets: 3,
            repsMin: 15,
            repsMax: 20,
            restSeconds: 60,
          },
          {
            exerciseId: "farmer-carry",
            sets: 3,
            timeSecondsMin: 30,
            timeSecondsMax: 45,
            restSeconds: 75,
            note: "3 recorridos de 30–45s.",
          },
        ],
      },
    ],
  },
];

export const TRAINING_DAYS_BY_CODE: Record<string, TrainingDay> =
  Object.fromEntries(TRAINING_DAYS.map((d) => [d.code, d]));

/** Total de series prescritas en un día (para estimar volumen/duración). */
export function totalPrescribedSets(day: TrainingDay): number {
  return day.blocks.reduce(
    (sum, b) => sum + b.exercises.reduce((s, e) => s + e.sets, 0),
    0,
  );
}
