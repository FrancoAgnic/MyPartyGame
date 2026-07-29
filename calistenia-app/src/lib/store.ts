"use client";

import { create } from "zustand";
import { persist } from "zustand/middleware";
import type {
  BodyMetric,
  ChecklistKey,
  HandstandSession,
  LoggedSet,
  PainReport,
  ProgressTest,
  TrainingDay,
  UserPreferences,
  UserProfile,
  WeeklyChecklist,
  WorkoutFeedback,
  WorkoutSession,
} from "@/lib/types";
import {
  DEFAULT_PREFERENCES,
  DEFAULT_PROFILE,
  emptyChecklistItems,
} from "@/lib/data/defaults";
import { TRAINING_DAYS_BY_CODE } from "@/lib/data/routine";
import { sessionVolume } from "@/lib/progression";
import { todayISO, uid, weekStartISO } from "@/lib/utils";

/**
 * Single source of truth for user data. Persists to localStorage (demo mode).
 * When Supabase is enabled (see src/lib/supabase), the same shapes are synced
 * to the cloud; components always read/write through this store.
 */
interface AppState {
  profile: UserProfile;
  preferences: UserPreferences;
  /** Override of default weekday for each day code (calendar reprogramming). */
  weekdayOverrides: Partial<Record<TrainingDay["code"], number>>;
  sessions: WorkoutSession[];
  handstandSessions: HandstandSession[];
  bodyMetrics: BodyMetric[];
  progressTests: ProgressTest[];
  painReports: PainReport[];
  checklists: WeeklyChecklist[];
  activeSessionId: string | null;
  hydrated: boolean;

  // Profile / preferences
  updateProfile: (patch: Partial<UserProfile>) => void;
  updatePreferences: (patch: Partial<UserPreferences>) => void;
  toggleTheme: () => void;
  setWeekday: (code: TrainingDay["code"], weekday: number) => void;

  // Workouts
  startWorkout: (dayCode: TrainingDay["code"], day: TrainingDay) => string;
  logSet: (sessionId: string, set: Omit<LoggedSet, "id">) => void;
  updateSet: (sessionId: string, setId: string, patch: Partial<LoggedSet>) => void;
  finishWorkout: (
    sessionId: string,
    status: WorkoutSession["status"],
    feedback?: WorkoutFeedback,
  ) => void;
  skipWorkout: (dayCode: TrainingDay["code"]) => void;
  deleteSession: (sessionId: string) => void;

  // Handstand & metrics
  addHandstandSession: (s: Omit<HandstandSession, "id">) => void;
  addBodyMetric: (m: Omit<BodyMetric, "id">) => void;
  addProgressTest: (t: Omit<ProgressTest, "id">) => void;
  addPainReport: (p: Omit<PainReport, "id">) => void;

  // Checklist
  toggleChecklist: (weekStart: string, key: ChecklistKey) => void;

  resetAll: () => void;
}

function ensureChecklist(
  checklists: WeeklyChecklist[],
  weekStart: string,
): WeeklyChecklist[] {
  if (checklists.some((c) => c.weekStart === weekStart)) return checklists;
  return [...checklists, { weekStart, items: emptyChecklistItems() }];
}

export const useAppStore = create<AppState>()(
  persist(
    (set, get) => ({
      profile: DEFAULT_PROFILE,
      preferences: DEFAULT_PREFERENCES,
      weekdayOverrides: {},
      sessions: [],
      handstandSessions: [],
      bodyMetrics: [],
      progressTests: [],
      painReports: [],
      checklists: [],
      activeSessionId: null,
      hydrated: false,

      updateProfile: (patch) =>
        set((s) => ({ profile: { ...s.profile, ...patch } })),

      updatePreferences: (patch) =>
        set((s) => ({ preferences: { ...s.preferences, ...patch } })),

      toggleTheme: () =>
        set((s) => ({
          preferences: {
            ...s.preferences,
            theme: s.preferences.theme === "dark" ? "light" : "dark",
          },
        })),

      setWeekday: (code, weekday) =>
        set((s) => ({
          weekdayOverrides: { ...s.weekdayOverrides, [code]: weekday },
        })),

      startWorkout: (dayCode) => {
        const id = uid("ws");
        const session: WorkoutSession = {
          id,
          dayCode,
          date: todayISO(),
          status: "en-progreso",
          startedAt: new Date().toISOString(),
          sets: [],
        };
        set((s) => ({
          sessions: [...s.sessions, session],
          activeSessionId: id,
        }));
        return id;
      },

      logSet: (sessionId, setData) =>
        set((s) => ({
          sessions: s.sessions.map((ws) =>
            ws.id === sessionId
              ? { ...ws, sets: [...ws.sets, { ...setData, id: uid("set") }] }
              : ws,
          ),
        })),

      updateSet: (sessionId, setId, patch) =>
        set((s) => ({
          sessions: s.sessions.map((ws) =>
            ws.id === sessionId
              ? {
                  ...ws,
                  sets: ws.sets.map((st) =>
                    st.id === setId ? { ...st, ...patch } : st,
                  ),
                }
              : ws,
          ),
        })),

      finishWorkout: (sessionId, status, feedback) =>
        set((s) => {
          const sessions = s.sessions.map((ws) => {
            if (ws.id !== sessionId) return ws;
            const updated: WorkoutSession = {
              ...ws,
              status,
              completedAt: new Date().toISOString(),
              feedback,
            };
            updated.totalVolume = sessionVolume(updated);
            return updated;
          });
          // Reflect completion in the weekly checklist.
          const finished = sessions.find((ws) => ws.id === sessionId);
          let checklists = s.checklists;
          if (finished && status === "completado") {
            const wk = weekStartISO(new Date(finished.date + "T00:00:00"));
            checklists = ensureChecklist(checklists, wk).map((c) => {
              if (c.weekStart !== wk) return c;
              const key = `workout${finished.dayCode}` as ChecklistKey;
              if (key in c.items) {
                return { ...c, items: { ...c.items, [key]: true } };
              }
              return c;
            });
          }
          return { sessions, checklists, activeSessionId: null };
        }),

      skipWorkout: (dayCode) =>
        set((s) => ({
          sessions: [
            ...s.sessions,
            {
              id: uid("ws"),
              dayCode,
              date: todayISO(),
              status: "omitido",
              sets: [],
            },
          ],
        })),

      deleteSession: (sessionId) =>
        set((s) => ({
          sessions: s.sessions.filter((ws) => ws.id !== sessionId),
          activeSessionId:
            s.activeSessionId === sessionId ? null : s.activeSessionId,
        })),

      addHandstandSession: (data) =>
        set((s) => ({
          handstandSessions: [
            ...s.handstandSessions,
            { ...data, id: uid("hs") },
          ],
        })),

      addBodyMetric: (m) =>
        set((s) => ({
          bodyMetrics: [...s.bodyMetrics, { ...m, id: uid("bm") }],
        })),

      addProgressTest: (t) =>
        set((s) => ({
          progressTests: [...s.progressTests, { ...t, id: uid("pt") }],
        })),

      addPainReport: (p) =>
        set((s) => ({
          painReports: [...s.painReports, { ...p, id: uid("pr") }],
        })),

      toggleChecklist: (weekStart, key) =>
        set((s) => {
          const checklists = ensureChecklist(s.checklists, weekStart).map((c) =>
            c.weekStart === weekStart
              ? { ...c, items: { ...c.items, [key]: !c.items[key] } }
              : c,
          );
          return { checklists };
        }),

      resetAll: () =>
        set({
          profile: DEFAULT_PROFILE,
          preferences: DEFAULT_PREFERENCES,
          weekdayOverrides: {},
          sessions: [],
          handstandSessions: [],
          bodyMetrics: [],
          progressTests: [],
          painReports: [],
          checklists: [],
          activeSessionId: null,
        }),
    }),
    {
      name: "calistenia-app-v1",
      version: 1,
      onRehydrateStorage: () => (state) => {
        if (state) state.hydrated = true;
      },
    },
  ),
);

/** Resolve the effective weekday for a training day (override or default). */
export function effectiveWeekday(
  code: TrainingDay["code"],
  overrides: Partial<Record<TrainingDay["code"], number>>,
): number {
  const day = TRAINING_DAYS_BY_CODE[code];
  return overrides[code] ?? day?.weekday ?? 1;
}
