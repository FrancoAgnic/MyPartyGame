/**
 * Domain types shared across the app. These mirror the Supabase schema in
 * `supabase/schema.sql` and the seed content in `src/lib/data`.
 */

export type Units = "kg" | "lb";
export type ThemeMode = "dark" | "light";
export type TrainingFrequency = 3 | 4 | 5;

export type MuscleGroup =
  | "pecho"
  | "espalda"
  | "hombros"
  | "brazos"
  | "piernas"
  | "core"
  | "handstand"
  | "movilidad";

export type EquipmentTag =
  | "maquina"
  | "mancuerna"
  | "polea"
  | "smith"
  | "banco"
  | "barra-dominadas"
  | "peso-corporal"
  | "banda";

export type ExerciseLevel = "principiante" | "intermedio" | "avanzado";

export type ExerciseCategory =
  | "empuje"
  | "traccion"
  | "hombros"
  | "brazos"
  | "core"
  | "handstand"
  | "movilidad"
  | "calentamiento"
  | "piernas"
  | "transporte";

/** Static exercise definition (content, lives in the exercise library). */
export interface Exercise {
  id: string;
  name: string;
  category: ExerciseCategory;
  primaryMuscles: MuscleGroup[];
  equipment: EquipmentTag[];
  level: ExerciseLevel;
  isBodyweight: boolean;
  imageEmoji: string; // lightweight illustration placeholder
  videoUrl?: string;
  instructions: string[];
  breathing?: string;
  commonMistakes: string[];
  corrections: string[];
  regression?: string;
  progression?: string;
  planetFitnessSubstitutions: string[];
  contraindications?: string;
}

/** How an exercise is prescribed inside a training day. */
export interface WorkoutExercisePrescription {
  exerciseId: string;
  sets: number;
  repsMin?: number;
  repsMax?: number;
  timeSecondsMin?: number;
  timeSecondsMax?: number;
  restSeconds: number;
  note?: string;
  isHandstand?: boolean;
}

export interface TrainingDay {
  id: string;
  code: "A" | "B" | "C" | "D" | "E";
  title: string;
  focus: string;
  objective: string[];
  weekday: number; // 0 = Sunday ... 6 = Saturday (default schedule)
  blocks: TrainingBlock[];
}

export interface TrainingBlock {
  id: string;
  title: string;
  exercises: WorkoutExercisePrescription[];
}

export interface WarmupExercise {
  id: string;
  name: string;
  imageEmoji: string;
  prescription: string;
  instructions: string[];
  notes?: string[];
}

export interface WarmupSection {
  id: string;
  title: string;
  exercises: WarmupExercise[];
}

export type SetStatus = "pending" | "done";

/** A single logged set during a workout session (user data). */
export interface LoggedSet {
  id: string;
  exerciseId: string;
  setNumber: number;
  reps?: number;
  weight?: number;
  timeSeconds?: number;
  rir?: number;
  rpe?: number;
  status: SetStatus;
  note?: string;
}

export type WorkoutStatus =
  | "programado"
  | "en-progreso"
  | "completado"
  | "parcial"
  | "omitido"
  | "reprogramado";

/** A workout session: an instance of a training day performed on a date. */
export interface WorkoutSession {
  id: string;
  dayCode: TrainingDay["code"];
  date: string; // ISO date (yyyy-mm-dd)
  status: WorkoutStatus;
  startedAt?: string;
  completedAt?: string;
  sets: LoggedSet[];
  /** Post-workout questionnaire. */
  feedback?: WorkoutFeedback;
  totalVolume?: number;
}

export interface WorkoutFeedback {
  difficulty?: number; // 1-10
  hadPain?: boolean;
  painNote?: string;
  energy?: number; // 1-5
  technique?: number; // 1-5
  sleptWell?: boolean;
  repeatWeightNextTime?: boolean;
}

export type HandstandEntryMethod = "kick-up" | "tuck" | "pared" | "otro";

export interface HandstandSession {
  id: string;
  date: string;
  attempts: number;
  successfulAttempts: number;
  bestSeconds: number;
  averageSeconds: number;
  entryMethod: HandstandEntryMethod;
  techniqueQuality: number; // 1-5
  levelId: number;
  note?: string;
  videoUrl?: string;
}

export interface HandstandLevel {
  id: number;
  name: string;
  goal: string[];
}

export interface BodyMetric {
  id: string;
  date: string;
  bodyWeight?: number; // in user's units
  notes?: string;
}

export interface ProgressTest {
  id: string;
  date: string;
  maxPullups?: number;
  maxDips?: number;
  handstandBestSeconds?: number;
}

export interface PainReport {
  id: string;
  date: string;
  area: string;
  severity: number; // 1-5
  exerciseId?: string;
  note?: string;
}

export type ChecklistKey =
  | "workoutA"
  | "workoutB"
  | "workoutC"
  | "mobility"
  | "handstand"
  | "protein"
  | "sleep"
  | "bodyWeight"
  | "progress";

export interface WeeklyChecklist {
  weekStart: string; // ISO date of Monday
  items: Record<ChecklistKey, boolean>;
}

export interface UserProfile {
  name: string;
  level: ExerciseLevel;
  frequency: TrainingFrequency;
  goals: string[];
  strictPullups: number;
  freeDips: number;
  freeHandstandSeconds: number;
  location: string;
  equipmentAvailable: EquipmentTag[];
  equipmentNotGuaranteed: string[];
}

export interface UserPreferences {
  theme: ThemeMode;
  units: Units;
  sessionDurationMinutes: number;
  preferMachinesOverCalisthenics: boolean;
  restTimerSoundEnabled: boolean;
  restTimerVibrationEnabled: boolean;
}

export interface FrequencyAssessmentResult {
  eligible: boolean;
  targetFrequency: TrainingFrequency;
  completionRate: number;
  weeksTrained: number;
  reasons: string[];
  recommendation: string;
}
