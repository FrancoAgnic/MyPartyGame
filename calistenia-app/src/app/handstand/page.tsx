"use client";

import { useState } from "react";
import { Activity, Plus, Trophy, Timer, Target } from "lucide-react";
import { useAppStore } from "@/lib/store";
import { useMounted } from "@/lib/hooks";
import {
  Card,
  Button,
  StatTile,
  Field,
  Input,
  EmptyState,
  SectionHeading,
} from "@/components/ui/primitives";
import { Modal } from "@/components/ui/Modal";
import { HANDSTAND_LEVELS } from "@/lib/data/warmup";
import {
  averageHandstandSeconds,
  bestHandstandSeconds,
} from "@/lib/selectors";
import { getExercise } from "@/lib/data/exercises";
import { ExerciseNotes } from "@/components/workout/ExerciseNotes";
import type { HandstandEntryMethod } from "@/lib/types";
import { formatDate, todayISO } from "@/lib/utils";
import { cn } from "@/lib/utils";

const ENTRY_METHODS: { value: HandstandEntryMethod; label: string }[] = [
  { value: "kick-up", label: "Kick-up" },
  { value: "tuck", label: "Tuck" },
  { value: "pared", label: "Pared" },
  { value: "otro", label: "Otro" },
];

export default function HandstandPage() {
  const mounted = useMounted();
  const { handstandSessions, addHandstandSession } = useAppStore();
  const [open, setOpen] = useState(false);

  const best = mounted ? bestHandstandSeconds(handstandSessions) : 0;
  const avg = mounted ? averageHandstandSeconds(handstandSessions) : 0;
  const totalAttempts = mounted
    ? handstandSessions.reduce((n, s) => n + s.attempts, 0)
    : 0;
  const currentLevel = mounted
    ? handstandSessions.reduce((lvl, s) => Math.max(lvl, s.levelId), 1)
    : 1;

  const sorted = [...handstandSessions].sort((a, b) =>
    b.date.localeCompare(a.date),
  );

  const freeExercise = getExercise("handstand-libre");

  return (
    <div className="space-y-6">
      <header className="flex items-start justify-between gap-3">
        <div>
          <h1 className="text-2xl font-bold">Handstand</h1>
          <p className="text-sm text-muted">Progresión, práctica y registro</p>
        </div>
        <Button onClick={() => setOpen(true)}>
          <Plus size={18} /> Registrar
        </Button>
      </header>

      <div className="grid grid-cols-2 gap-3 sm:grid-cols-4">
        <StatTile label="Mejor tiempo" value={best} unit="s" icon={<Trophy size={14} />} />
        <StatTile label="Promedio" value={avg} unit="s" icon={<Timer size={14} />} />
        <StatTile label="Intentos" value={totalAttempts} icon={<Activity size={14} />} />
        <StatTile label="Nivel" value={currentLevel} unit="/5" icon={<Target size={14} />} />
      </div>

      {/* Level system */}
      <section>
        <SectionHeading title="Sistema de niveles" subtitle={`Nivel actual: ${currentLevel}`} />
        <div className="space-y-3">
          {HANDSTAND_LEVELS.map((level) => {
            const isCurrent = level.id === currentLevel;
            const isDone = level.id < currentLevel;
            return (
              <Card
                key={level.id}
                className={cn(
                  isCurrent && "border-primary/50",
                  isDone && "opacity-70",
                )}
              >
                <div className="flex items-center gap-3">
                  <div
                    className={cn(
                      "flex h-9 w-9 shrink-0 items-center justify-center rounded-full text-sm font-bold",
                      isDone
                        ? "bg-success/15 text-success"
                        : isCurrent
                          ? "bg-primary/15 text-primary"
                          : "bg-surface-2 text-muted",
                    )}
                  >
                    {level.id}
                  </div>
                  <h3 className="font-semibold">{level.name}</h3>
                </div>
                <ul className="mt-2 space-y-1 pl-12 text-sm text-muted">
                  {level.goal.map((g, i) => (
                    <li key={i} className="flex gap-2">
                      <span className="text-primary">•</span>
                      {g}
                    </li>
                  ))}
                </ul>
              </Card>
            );
          })}
        </div>
      </section>

      {/* Technique */}
      {freeExercise ? (
        <section>
          <SectionHeading title="Técnica del handstand libre" />
          <Card>
            <ExerciseNotes exercise={freeExercise} />
          </Card>
        </section>
      ) : null}

      {/* History */}
      <section>
        <SectionHeading title="Historial" />
        {!mounted || sorted.length === 0 ? (
          <EmptyState
            icon={<Activity size={28} />}
            title="Todavía no registraste sesiones"
            description="Registrá tus intentos, mejor tiempo y calidad técnica para ver tu evolución."
            action={<Button onClick={() => setOpen(true)}>Registrar sesión</Button>}
          />
        ) : (
          <div className="space-y-2">
            {sorted.map((s) => (
              <Card key={s.id} className="flex items-center gap-3 p-3">
                <div className="flex h-10 w-10 items-center justify-center rounded-xl bg-accent/15 text-sm font-bold text-accent">
                  {s.bestSeconds}s
                </div>
                <div className="min-w-0 flex-1">
                  <div className="text-sm font-medium">{formatDate(s.date)}</div>
                  <div className="text-xs text-muted">
                    {s.successfulAttempts}/{s.attempts} logrados · prom {s.averageSeconds}s
                    · {s.entryMethod} · técnica {s.techniqueQuality}/5
                  </div>
                  {s.note ? (
                    <div className="mt-0.5 text-xs text-muted">{s.note}</div>
                  ) : null}
                </div>
              </Card>
            ))}
          </div>
        )}
      </section>

      <LogHandstandModal
        open={open}
        currentLevel={currentLevel}
        onClose={() => setOpen(false)}
        onSave={(data) => {
          addHandstandSession(data);
          setOpen(false);
        }}
      />
    </div>
  );
}

function LogHandstandModal({
  open,
  currentLevel,
  onClose,
  onSave,
}: {
  open: boolean;
  currentLevel: number;
  onClose: () => void;
  onSave: (data: {
    date: string;
    attempts: number;
    successfulAttempts: number;
    bestSeconds: number;
    averageSeconds: number;
    entryMethod: HandstandEntryMethod;
    techniqueQuality: number;
    levelId: number;
    note?: string;
  }) => void;
}) {
  const [attempts, setAttempts] = useState("10");
  const [successful, setSuccessful] = useState("3");
  const [best, setBest] = useState("");
  const [avg, setAvg] = useState("");
  const [entry, setEntry] = useState<HandstandEntryMethod>("pared");
  const [quality, setQuality] = useState(3);
  const [level, setLevel] = useState(currentLevel);
  const [note, setNote] = useState("");

  const save = () => {
    onSave({
      date: todayISO(),
      attempts: Number(attempts) || 0,
      successfulAttempts: Number(successful) || 0,
      bestSeconds: Number(best) || 0,
      averageSeconds: Number(avg) || 0,
      entryMethod: entry,
      techniqueQuality: quality,
      levelId: level,
      note: note || undefined,
    });
    setBest("");
    setAvg("");
    setNote("");
  };

  return (
    <Modal
      open={open}
      onClose={onClose}
      title="Registrar sesión de handstand"
      footer={
        <div className="flex gap-2">
          <Button variant="outline" className="flex-1" onClick={onClose}>
            Cancelar
          </Button>
          <Button className="flex-1" onClick={save}>
            Guardar
          </Button>
        </div>
      }
    >
      <div className="space-y-4">
        <div className="grid grid-cols-2 gap-3">
          <Field label="Intentos">
            <Input
              type="number"
              value={attempts}
              onChange={(e) => setAttempts(e.target.value)}
            />
          </Field>
          <Field label="Exitosos">
            <Input
              type="number"
              value={successful}
              onChange={(e) => setSuccessful(e.target.value)}
            />
          </Field>
          <Field label="Mejor tiempo (s)">
            <Input
              type="number"
              value={best}
              onChange={(e) => setBest(e.target.value)}
            />
          </Field>
          <Field label="Promedio (s)">
            <Input
              type="number"
              value={avg}
              onChange={(e) => setAvg(e.target.value)}
            />
          </Field>
        </div>

        <Field label="Método de entrada">
          <div className="flex flex-wrap gap-2">
            {ENTRY_METHODS.map((m) => (
              <Button
                key={m.value}
                size="sm"
                variant={entry === m.value ? "primary" : "secondary"}
                onClick={() => setEntry(m.value)}
              >
                {m.label}
              </Button>
            ))}
          </div>
        </Field>

        <Field label={`Calidad técnica (${quality}/5)`}>
          <div className="flex gap-1">
            {[1, 2, 3, 4, 5].map((n) => (
              <button
                key={n}
                onClick={() => setQuality(n)}
                className={cn(
                  "h-9 flex-1 rounded-lg text-sm font-semibold",
                  n <= quality
                    ? "bg-primary text-primary-fg"
                    : "bg-surface-2 text-muted",
                )}
              >
                {n}
              </button>
            ))}
          </div>
        </Field>

        <Field label="Nivel trabajado">
          <div className="flex gap-1">
            {HANDSTAND_LEVELS.map((l) => (
              <button
                key={l.id}
                onClick={() => setLevel(l.id)}
                className={cn(
                  "h-9 flex-1 rounded-lg text-sm font-semibold",
                  l.id === level
                    ? "bg-accent text-white"
                    : "bg-surface-2 text-muted",
                )}
              >
                {l.id}
              </button>
            ))}
          </div>
        </Field>

        <Field label="Notas (opcional)">
          <Input
            value={note}
            onChange={(e) => setNote(e.target.value)}
            placeholder="Ej. mejor control con kick-up suave"
          />
        </Field>
      </div>
    </Modal>
  );
}
