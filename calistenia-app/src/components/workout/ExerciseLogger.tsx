"use client";

import { useMemo, useState } from "react";
import { Check, History, RefreshCw, SkipForward, TrendingUp } from "lucide-react";
import type {
  Exercise,
  LoggedSet,
  Units,
  WorkoutExercisePrescription,
} from "@/lib/types";
import { Button } from "@/components/ui/primitives";
import { ExerciseNotes } from "./ExerciseNotes";
import { useAppStore } from "@/lib/store";
import { suggestProgression, type ProgressionAdvice } from "@/lib/progression";
import { cn, repRangeLabel, timeRangeLabel } from "@/lib/utils";

interface Props {
  sessionId: string;
  prescription: WorkoutExercisePrescription;
  exercise: Exercise;
  units: Units;
  history: LoggedSet[]; // last session's sets for this exercise
  onSetCompleted: (restSeconds: number) => void;
  onReplace: () => void;
  onSkip: () => void;
}

interface DraftSet {
  weight: string;
  reps: string;
  time: string;
  rir: string;
}

const ADVICE_STYLE: Record<ProgressionAdvice["action"], string> = {
  increase: "text-success",
  hold: "text-warning",
  reduce: "text-danger",
  "keep-adding-reps": "text-accent",
};

const ADVICE_LABEL: Record<ProgressionAdvice["action"], string> = {
  increase: "Subí el peso",
  hold: "Mantené el peso",
  reduce: "Bajá el peso",
  "keep-adding-reps": "Sumá reps",
};

export function ExerciseLogger({
  sessionId,
  prescription,
  exercise,
  units,
  history,
  onSetCompleted,
  onReplace,
  onSkip,
}: Props) {
  const logSet = useAppStore((s) => s.logSet);
  const updateSet = useAppStore((s) => s.updateSet);
  const session = useAppStore((s) =>
    s.sessions.find((ws) => ws.id === sessionId),
  );

  const isTimed = prescription.repsMax == null && prescription.timeSecondsMax != null;

  const loggedSets = useMemo(
    () =>
      (session?.sets ?? []).filter((s) => s.exerciseId === prescription.exerciseId),
    [session, prescription.exerciseId],
  );

  const [drafts, setDrafts] = useState<Record<number, DraftSet>>({});

  const setDraft = (n: number, patch: Partial<DraftSet>) =>
    setDrafts((d) => ({
      ...d,
      [n]: { weight: "", reps: "", time: "", rir: "", ...d[n], ...patch },
    }));

  const completeSet = (setNumber: number) => {
    const draft = drafts[setNumber] ?? { weight: "", reps: "", time: "", rir: "" };
    const existing = loggedSets.find((s) => s.setNumber === setNumber);
    const payload: Omit<LoggedSet, "id"> = {
      exerciseId: prescription.exerciseId,
      setNumber,
      weight: draft.weight ? Number(draft.weight) : undefined,
      reps: !isTimed && draft.reps ? Number(draft.reps) : undefined,
      timeSeconds: isTimed && draft.time ? Number(draft.time) : undefined,
      rir: draft.rir ? Number(draft.rir) : undefined,
      status: "done",
    };
    if (existing) {
      updateSet(sessionId, existing.id, payload);
    } else {
      logSet(sessionId, payload);
    }
    onSetCompleted(prescription.restSeconds);
  };

  const advice = suggestProgression(prescription, loggedSets);
  const completedCount = loggedSets.filter((s) => s.status === "done").length;
  const allDone = completedCount >= prescription.sets;

  const targetLabel = isTimed
    ? timeRangeLabel(prescription.timeSecondsMin, prescription.timeSecondsMax)
    : repRangeLabel(prescription.repsMin, prescription.repsMax);

  return (
    <div
      className={cn(
        "rounded-2xl border border-border bg-surface p-4",
        allDone && "border-success/40",
      )}
    >
      <div className="flex items-start gap-3">
        <div className="text-3xl" aria-hidden>
          {exercise.imageEmoji}
        </div>
        <div className="min-w-0 flex-1">
          <h3 className="font-bold leading-tight">{exercise.name}</h3>
          <p className="text-sm text-muted">
            {prescription.sets} series · {targetLabel} · descanso{" "}
            {prescription.restSeconds}s
          </p>
          {prescription.note ? (
            <p className="mt-0.5 text-xs text-muted">{prescription.note}</p>
          ) : null}
        </div>
        <span className="shrink-0 rounded-full bg-surface-2 px-2 py-1 text-xs font-semibold tabular-nums">
          {completedCount}/{prescription.sets}
        </span>
      </div>

      {history.length > 0 ? (
        <div className="mt-3 flex items-center gap-2 rounded-xl bg-surface-2/60 px-3 py-2 text-xs text-muted">
          <History size={14} />
          <span>
            Última vez:{" "}
            {history
              .slice(0, prescription.sets)
              .map((s) =>
                isTimed
                  ? `${s.timeSeconds ?? "—"}s`
                  : `${s.weight ?? "—"}${units}×${s.reps ?? "—"}`,
              )
              .join(" · ")}
          </span>
        </div>
      ) : null}

      {/* Set rows */}
      <div className="mt-3 space-y-2">
        {Array.from({ length: prescription.sets }).map((_, i) => {
          const setNumber = i + 1;
          const logged = loggedSets.find((s) => s.setNumber === setNumber);
          const done = logged?.status === "done";
          const draft = drafts[setNumber] ?? {
            weight: logged?.weight?.toString() ?? "",
            reps: logged?.reps?.toString() ?? "",
            time: logged?.timeSeconds?.toString() ?? "",
            rir: logged?.rir?.toString() ?? "",
          };
          return (
            <div
              key={setNumber}
              className={cn(
                "flex items-center gap-2 rounded-xl border border-border/60 p-2",
                done && "bg-success/5",
              )}
            >
              <span className="w-6 text-center text-sm font-bold text-muted">
                {setNumber}
              </span>
              {isTimed ? (
                <LogInput
                  label="seg"
                  value={draft.time}
                  onChange={(v) => setDraft(setNumber, { time: v })}
                />
              ) : (
                <>
                  <LogInput
                    label={units}
                    value={draft.weight}
                    onChange={(v) => setDraft(setNumber, { weight: v })}
                  />
                  <LogInput
                    label="reps"
                    value={draft.reps}
                    onChange={(v) => setDraft(setNumber, { reps: v })}
                  />
                </>
              )}
              <LogInput
                label="RIR"
                value={draft.rir}
                width="w-16"
                onChange={(v) => setDraft(setNumber, { rir: v })}
              />
              <Button
                size="sm"
                variant={done ? "secondary" : "primary"}
                className="ml-auto"
                onClick={() => completeSet(setNumber)}
                aria-label={`Completar serie ${setNumber}`}
              >
                <Check size={16} />
              </Button>
            </div>
          );
        })}
      </div>

      {/* Progression advice */}
      {completedCount > 0 ? (
        <div className="mt-3 flex items-center gap-2 text-sm">
          <TrendingUp size={15} className={ADVICE_STYLE[advice.action]} />
          <span className={cn("font-semibold", ADVICE_STYLE[advice.action])}>
            {ADVICE_LABEL[advice.action]}:
          </span>
          <span className="text-muted">{advice.reason}</span>
        </div>
      ) : null}

      <div className="mt-3">
        <ExerciseNotes exercise={exercise} />
      </div>

      <div className="mt-3 flex gap-2">
        <Button size="sm" variant="ghost" onClick={onReplace}>
          <RefreshCw size={14} /> Reemplazar
        </Button>
        <Button size="sm" variant="ghost" onClick={onSkip}>
          <SkipForward size={14} /> Omitir
        </Button>
      </div>
    </div>
  );
}

function LogInput({
  label,
  value,
  onChange,
  width = "w-20",
}: {
  label: string;
  value: string;
  onChange: (v: string) => void;
  width?: string;
}) {
  return (
    <label className={cn("relative", width)}>
      <input
        type="number"
        inputMode="decimal"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        className="h-10 w-full rounded-lg border border-border bg-surface-2 px-2 pt-3 text-center text-sm font-semibold tabular-nums focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-primary"
        aria-label={label}
      />
      <span className="pointer-events-none absolute left-1/2 top-0.5 -translate-x-1/2 text-[9px] uppercase text-muted">
        {label}
      </span>
    </label>
  );
}
