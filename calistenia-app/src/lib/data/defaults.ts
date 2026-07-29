import type {
  ChecklistKey,
  UserPreferences,
  UserProfile,
} from "@/lib/types";

/** Perfil inicial de Franco (editable desde Perfil). */
export const DEFAULT_PROFILE: UserProfile = {
  name: "Franco",
  level: "principiante",
  frequency: 3,
  goals: [
    "Ganar masa muscular.",
    "Aumentar volumen de hombros, pecho y espalda.",
    "Mejorar fuerza relativa.",
    "Mejorar el handstand.",
    "Progresar gradualmente hacia 4 o 5 días por semana.",
  ],
  strictPullups: 4,
  freeDips: 5,
  freeHandstandSeconds: 4,
  location: "Planet Fitness",
  equipmentAvailable: [
    "maquina",
    "polea",
    "mancuerna",
    "smith",
    "banco",
    "barra-dominadas",
    "peso-corporal",
  ],
  equipmentNotGuaranteed: ["Anillas", "Barra olímpica libre", "Paralelas profesionales"],
};

export const DEFAULT_PREFERENCES: UserPreferences = {
  theme: "dark",
  units: "kg",
  sessionDurationMinutes: 70,
  preferMachinesOverCalisthenics: false,
  restTimerSoundEnabled: true,
  restTimerVibrationEnabled: true,
};

export const CHECKLIST_LABELS: Record<ChecklistKey, string> = {
  workoutA: "Entrenamiento A completado",
  workoutB: "Entrenamiento B completado",
  workoutC: "Entrenamiento C completado",
  mobility: "Sesión de movilidad",
  handstand: "Práctica de handstand",
  protein: "Consumo adecuado de proteína",
  sleep: "Sueño suficiente",
  bodyWeight: "Registro de peso corporal",
  progress: "Registro de progreso",
};

export const CHECKLIST_KEYS: ChecklistKey[] = [
  "workoutA",
  "workoutB",
  "workoutC",
  "mobility",
  "handstand",
  "protein",
  "sleep",
  "bodyWeight",
  "progress",
];

export function emptyChecklistItems(): Record<ChecklistKey, boolean> {
  return CHECKLIST_KEYS.reduce(
    (acc, k) => {
      acc[k] = false;
      return acc;
    },
    {} as Record<ChecklistKey, boolean>,
  );
}
