import type {
  FrequencyAssessmentResult,
  LoggedSet,
  TrainingFrequency,
  WorkoutExercisePrescription,
  WorkoutSession,
} from "@/lib/types";

/**
 * Progresión doble de cargas.
 *
 * Regla general: mantener el peso hasta lograr el tope del rango de reps en
 * todas las series con 1–2 reps en reserva; recién ahí subir el peso y volver
 * a la parte baja del rango. No recomendar aumento si hay dolor, se pierde el
 * rango o técnica, se llega al fallo en todas las series o el RPE es 10.
 */
export type ProgressionAdvice =
  | { action: "increase"; reason: string }
  | { action: "hold"; reason: string }
  | { action: "reduce"; reason: string }
  | { action: "keep-adding-reps"; reason: string };

export function suggestProgression(
  prescription: WorkoutExercisePrescription,
  sets: LoggedSet[],
): ProgressionAdvice {
  const done = sets.filter((s) => s.status === "done");
  if (done.length === 0) {
    return { action: "hold", reason: "Sin series registradas todavía." };
  }

  // Señales de alarma que bloquean el aumento.
  const anyPain = done.some((s) => (s.note ?? "").toLowerCase().includes("dolor"));
  const allToFailure = done.every((s) => (s.rir ?? 3) <= 0);
  const repeatedRpe10 = done.filter((s) => (s.rpe ?? 0) >= 10).length >= done.length;

  if (anyPain) {
    return { action: "reduce", reason: "Molestia registrada: no aumentar carga." };
  }
  if (allToFailure || repeatedRpe10) {
    return {
      action: "hold",
      reason: "Llegaste al fallo/RPE 10 en todas las series: consolidá el peso.",
    };
  }

  // Ejercicios por tiempo (handstand, holds): progresar por tiempo.
  if (prescription.repsMax == null && prescription.timeSecondsMax != null) {
    const reachedTop = done.every(
      (s) => (s.timeSeconds ?? 0) >= (prescription.timeSecondsMax ?? Infinity),
    );
    return reachedTop
      ? { action: "increase", reason: "Alcanzaste el tope de tiempo: aumentá dificultad." }
      : { action: "keep-adding-reps", reason: "Seguí sumando segundos dentro del rango." };
  }

  const top = prescription.repsMax;
  if (top == null) {
    return { action: "hold", reason: "Sin rango de reps definido." };
  }

  const allReachedTop = done.every((s) => (s.reps ?? 0) >= top);
  const enoughReserve = done.every((s) => (s.rir ?? 2) >= 1);

  if (allReachedTop && enoughReserve && done.length >= prescription.sets) {
    return {
      action: "increase",
      reason: "Completaste el tope del rango con reps en reserva: subí el peso.",
    };
  }

  const anyBelowBottom =
    prescription.repsMin != null &&
    done.some((s) => (s.reps ?? 0) < (prescription.repsMin ?? 0));
  if (anyBelowBottom) {
    return {
      action: "reduce",
      reason: "Quedaste por debajo del rango: bajá un poco el peso.",
    };
  }

  return {
    action: "keep-adding-reps",
    reason: "Sumá repeticiones hasta llegar al tope del rango.",
  };
}

/** Volumen total de una sesión: sum(reps × peso) de las series completadas. */
export function sessionVolume(session: WorkoutSession): number {
  return session.sets
    .filter((s) => s.status === "done")
    .reduce((sum, s) => sum + (s.reps ?? 0) * (s.weight ?? 0), 0);
}

/**
 * Evaluación para aumentar la frecuencia semanal (3→4 o 4→5).
 * No se aumenta automáticamente por el paso del tiempo: se exige mantener los
 * criterios durante al menos 2 semanas (3→4) / 4 semanas (4→5).
 */
export interface AssessmentInput {
  currentFrequency: TrainingFrequency;
  sessions: WorkoutSession[];
  weeksTrained: number;
}

export function assessFrequencyProgression(
  input: AssessmentInput,
): FrequencyAssessmentResult {
  const { currentFrequency, sessions, weeksTrained } = input;
  const target = (currentFrequency + 1) as TrainingFrequency;

  const scheduled = sessions.filter(
    (s) => s.status !== "reprogramado",
  ).length;
  const completed = sessions.filter(
    (s) => s.status === "completado",
  ).length;
  const completionRate = scheduled === 0 ? 0 : completed / scheduled;

  const recent = sessions.slice(-12);
  const painFlags = recent.filter((s) => s.feedback?.hadPain).length;
  const poorSleep = recent.filter((s) => s.feedback?.sleptWell === false).length;
  const failureFlags = recent.filter(
    (s) => (s.feedback?.difficulty ?? 0) >= 10,
  ).length;

  const minWeeks = currentFrequency === 3 ? 4 : 4;
  const sustainWeeks = currentFrequency === 3 ? 2 : 4;

  const reasons: string[] = [];
  const okCompletion = completionRate >= 0.85;
  const okPain = painFlags <= 1;
  const okSleep = poorSleep <= Math.ceil(recent.length / 3);
  const okFailure = failureFlags === 0;
  const okWeeks = weeksTrained >= minWeeks;

  if (!okWeeks)
    reasons.push(
      `Entrená al menos ${minWeeks} semanas en ${currentFrequency} días (llevás ${weeksTrained}).`,
    );
  if (!okCompletion)
    reasons.push(
      `Completá al menos el 85% de las sesiones (vas ${Math.round(
        completionRate * 100,
      )}%).`,
    );
  if (!okPain) reasons.push("Reducí el dolor articular persistente antes de subir.");
  if (!okSleep) reasons.push("Estabilizá el sueño antes de aumentar la carga semanal.");
  if (!okFailure) reasons.push("Evitá llegar constantemente al fallo.");

  const eligible = okWeeks && okCompletion && okPain && okSleep && okFailure;

  let recommendation: string;
  if (currentFrequency >= 5) {
    recommendation =
      "Ya entrenás 5 días. Priorizá recuperación y calidad antes que más frecuencia.";
  } else if (eligible) {
    recommendation =
      currentFrequency === 3
        ? `Cumplís los criterios durante ${sustainWeeks}+ semanas. Podés sumar un 4.º día LIGERO (movilidad, handstand técnico, core, laterales suaves, face pulls). No lo conviertas en otra sesión pesada.`
        : "Cumplís los criterios. Podés sumar un 5.º día: elegí handstand/skills, hipertrofia superior moderada o piernas según tu grupo rezagado.";
  } else {
    recommendation = `Mantené ${currentFrequency} días y sostené los criterios ${sustainWeeks} semanas más antes de aumentar.`;
  }

  return {
    eligible,
    targetFrequency: target > 5 ? 5 : target,
    completionRate,
    weeksTrained,
    reasons,
    recommendation,
  };
}

/** Opciones para el 5.º día. */
export const FIFTH_DAY_OPTIONS = [
  {
    id: "skills",
    title: "Opción A — Handstand y skills",
    items: ["Handstand", "L-sit", "Movilidad", "Core", "Técnica escapular"],
  },
  {
    id: "hipertrofia",
    title: "Opción B — Hipertrofia superior",
    items: [
      "Pecho",
      "Espalda",
      "Hombro lateral y posterior",
      "Brazos",
      "Volumen moderado",
    ],
  },
  {
    id: "piernas",
    title: "Opción C — Piernas",
    items: ["Para quienes redujeron demasiado el tren inferior."],
  },
];

/** Contenido sugerido para un 4.º día ligero. */
export const FOURTH_DAY_CONTENT = [
  "Movilidad de muñecas",
  "Movilidad de hombros",
  "Handstand técnico",
  "Core",
  "Elevaciones laterales ligeras",
  "Face pulls",
  "Rotación externa",
  "Trabajo escapular",
  "Cardio suave opcional",
];
