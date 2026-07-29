"use client";

import { useMemo, useState } from "react";
import { useRouter } from "next/navigation";
import Link from "next/link";
import { ArrowLeft, Flag, Play } from "lucide-react";
import { useAppStore } from "@/lib/store";
import { useMounted } from "@/lib/hooks";
import { Button, Card, ProgressBar, EmptyState } from "@/components/ui/primitives";
import { WarmupList } from "@/components/workout/WarmupList";
import { ExerciseLogger } from "@/components/workout/ExerciseLogger";
import {
  RestTimerBar,
  useRestTimer,
} from "@/components/workout/RestTimer";
import { FinishWorkoutDialog } from "@/components/workout/FinishWorkoutDialog";
import { Modal } from "@/components/ui/Modal";
import { TRAINING_DAYS_BY_CODE } from "@/lib/data/routine";
import { EXERCISES, getExercise } from "@/lib/data/exercises";
import type { TrainingDay, WorkoutFeedback } from "@/lib/types";
import { todayISO } from "@/lib/utils";

export default function WorkoutDayPage({
  params,
}: {
  params: { code: string };
}) {
  const dayCode = params.code.toUpperCase() as TrainingDay["code"];
  const day = TRAINING_DAYS_BY_CODE[dayCode];
  const mounted = useMounted();
  const router = useRouter();

  const store = useAppStore();
  const timer = useRestTimer();

  const [showFinish, setShowFinish] = useState(false);
  const [replaceFor, setReplaceFor] = useState<string | null>(null);
  const [replacements, setReplacements] = useState<Record<string, string>>({});
  const [skipped, setSkipped] = useState<Record<string, boolean>>({});

  const activeSession = useMemo(() => {
    if (!day) return undefined;
    return store.sessions.find(
      (s) =>
        s.dayCode === dayCode &&
        s.date === todayISO() &&
        (s.status === "en-progreso" || s.status === "completado" || s.status === "parcial"),
    );
  }, [store.sessions, dayCode, day]);

  // Previous session (before the active one) for history display.
  const previousSession = useMemo(
    () =>
      [...store.sessions]
        .filter(
          (s) =>
            s.dayCode === dayCode &&
            s.id !== activeSession?.id &&
            s.sets.length > 0,
        )
        .sort((a, b) => b.date.localeCompare(a.date))[0],
    [store.sessions, dayCode, activeSession],
  );

  if (!day) {
    return (
      <EmptyState
        title="Entrenamiento no encontrado"
        description="Ese día no existe en tu rutina."
        action={
          <Link href="/entrenar">
            <Button>Volver a la rutina</Button>
          </Link>
        }
      />
    );
  }

  const started = activeSession && activeSession.status === "en-progreso";

  const handleStart = () => {
    store.startWorkout(dayCode, day);
  };

  const handleFinish = (feedback: WorkoutFeedback) => {
    if (!activeSession) return;
    store.finishWorkout(activeSession.id, "completado", feedback);
    setShowFinish(false);
    router.push("/");
  };

  const allPrescriptions = day.blocks.flatMap((b) =>
    b.exercises.map((ex) => ({ block: b.title, ex })),
  );
  const totalSets = allPrescriptions.reduce((n, p) => n + p.ex.sets, 0);
  const completedSets = activeSession
    ? activeSession.sets.filter((s) => s.status === "done").length
    : 0;
  const progress = totalSets === 0 ? 0 : completedSets / totalSets;

  return (
    <div className="space-y-5 pb-4">
      <div className="flex items-center gap-3">
        <Link href="/entrenar" aria-label="Volver">
          <Button variant="ghost" size="sm">
            <ArrowLeft size={18} />
          </Button>
        </Link>
        <div className="min-w-0 flex-1">
          <h1 className="truncate text-xl font-bold">{day.title}</h1>
          <p className="truncate text-xs text-muted">{day.focus}</p>
        </div>
      </div>

      {/* Objective */}
      {!started ? (
        <Card>
          <h2 className="text-sm font-bold text-muted">Objetivo</h2>
          <ul className="mt-2 space-y-1 text-sm">
            {day.objective.map((o, i) => (
              <li key={i} className="flex gap-2">
                <span className="text-primary">•</span>
                {o}
              </li>
            ))}
          </ul>
        </Card>
      ) : null}

      {/* Warmup */}
      {!started ? <WarmupList /> : null}

      {/* Progress bar when in session */}
      {started ? (
        <Card className="sticky top-2 z-20">
          <div className="flex items-center justify-between text-sm">
            <span className="font-semibold">
              {completedSets}/{totalSets} series
            </span>
            <span className="text-muted">{Math.round(progress * 100)}%</span>
          </div>
          <ProgressBar value={progress} tone="success" className="mt-2" />
        </Card>
      ) : null}

      {/* Blocks & exercises */}
      {day.blocks.map((block) => (
        <section key={block.id}>
          <h2 className="mb-2 mt-1 text-sm font-bold uppercase tracking-wide text-muted">
            {block.title}
          </h2>
          <div className="space-y-3">
            {block.exercises.map((prescription) => {
              const effectiveId =
                replacements[prescription.exerciseId] ?? prescription.exerciseId;
              const exercise = getExercise(effectiveId);
              if (!exercise) return null;
              if (skipped[prescription.exerciseId]) {
                return (
                  <SkippedRow
                    key={prescription.exerciseId}
                    name={exercise.name}
                    onUndo={() =>
                      setSkipped((s) => ({ ...s, [prescription.exerciseId]: false }))
                    }
                  />
                );
              }
              if (!started) {
                return (
                  <PreviewRow
                    key={prescription.exerciseId}
                    emoji={exercise.imageEmoji}
                    name={exercise.name}
                    detail={prescriptionDetail(prescription)}
                  />
                );
              }
              const history = previousSession
                ? previousSession.sets.filter(
                    (s) => s.exerciseId === effectiveId,
                  )
                : [];
              return (
                <ExerciseLogger
                  key={prescription.exerciseId}
                  sessionId={activeSession!.id}
                  prescription={{ ...prescription, exerciseId: effectiveId }}
                  exercise={exercise}
                  units={store.preferences.units}
                  history={history}
                  onSetCompleted={(rest) => timer.start(rest)}
                  onReplace={() => setReplaceFor(prescription.exerciseId)}
                  onSkip={() =>
                    setSkipped((s) => ({ ...s, [prescription.exerciseId]: true }))
                  }
                />
              );
            })}
          </div>
        </section>
      ))}

      {/* Start / finish CTA */}
      {!started ? (
        <div className="sticky bottom-20 z-20 md:bottom-4">
          <Button size="lg" className="w-full shadow-lg" onClick={handleStart}>
            <Play size={18} /> Comenzar entrenamiento
          </Button>
        </div>
      ) : (
        <div className="sticky bottom-20 z-20 md:bottom-4">
          <Button
            size="lg"
            variant="secondary"
            className="w-full shadow-lg"
            onClick={() => setShowFinish(true)}
          >
            <Flag size={18} /> Finalizar entrenamiento
          </Button>
        </div>
      )}

      {mounted && timer.active ? (
        <RestTimerBar
          remaining={timer.remaining}
          onAdjust={timer.adjust}
          onStop={timer.stop}
        />
      ) : null}

      <FinishWorkoutDialog
        open={showFinish}
        onClose={() => setShowFinish(false)}
        onConfirm={handleFinish}
      />

      <ReplaceExerciseModal
        open={replaceFor != null}
        currentId={
          replaceFor ? replacements[replaceFor] ?? replaceFor : undefined
        }
        onClose={() => setReplaceFor(null)}
        onPick={(newId) => {
          if (replaceFor) setReplacements((r) => ({ ...r, [replaceFor]: newId }));
          setReplaceFor(null);
        }}
      />
    </div>
  );
}

function prescriptionDetail(p: {
  sets: number;
  repsMin?: number;
  repsMax?: number;
  timeSecondsMin?: number;
  timeSecondsMax?: number;
  restSeconds: number;
}): string {
  const range =
    p.repsMax != null
      ? `${p.repsMin}–${p.repsMax} reps`
      : `${p.timeSecondsMin}–${p.timeSecondsMax}s`;
  return `${p.sets} × ${range} · descanso ${p.restSeconds}s`;
}

function PreviewRow({
  emoji,
  name,
  detail,
}: {
  emoji: string;
  name: string;
  detail: string;
}) {
  return (
    <Card className="flex items-center gap-3 p-3">
      <span className="text-2xl">{emoji}</span>
      <div className="min-w-0 flex-1">
        <div className="truncate font-medium">{name}</div>
        <div className="text-xs text-muted">{detail}</div>
      </div>
    </Card>
  );
}

function SkippedRow({ name, onUndo }: { name: string; onUndo: () => void }) {
  return (
    <div className="flex items-center gap-3 rounded-2xl border border-dashed border-border p-3 text-muted">
      <span className="flex-1 text-sm line-through">{name}</span>
      <Button size="sm" variant="ghost" onClick={onUndo}>
        Deshacer
      </Button>
    </div>
  );
}

function ReplaceExerciseModal({
  open,
  currentId,
  onClose,
  onPick,
}: {
  open: boolean;
  currentId?: string;
  onClose: () => void;
  onPick: (id: string) => void;
}) {
  const current = currentId ? getExercise(currentId) : undefined;
  const candidates = EXERCISES.filter(
    (e) =>
      current &&
      e.id !== current.id &&
      e.primaryMuscles.some((m) => current.primaryMuscles.includes(m)),
  );
  return (
    <Modal open={open} onClose={onClose} title="Reemplazar ejercicio">
      <p className="mb-3 text-sm text-muted">
        Alternativas para músculos similares
        {current ? ` a ${current.name}` : ""}:
      </p>
      <div className="space-y-2">
        {candidates.map((e) => (
          <button
            key={e.id}
            onClick={() => onPick(e.id)}
            className="flex w-full items-center gap-3 rounded-xl border border-border p-3 text-left hover:bg-surface-2"
          >
            <span className="text-2xl">{e.imageEmoji}</span>
            <div>
              <div className="font-medium">{e.name}</div>
              <div className="text-xs text-muted">
                {e.primaryMuscles.join(" · ")}
              </div>
            </div>
          </button>
        ))}
        {candidates.length === 0 ? (
          <p className="text-sm text-muted">No hay alternativas registradas.</p>
        ) : null}
      </div>
    </Modal>
  );
}
