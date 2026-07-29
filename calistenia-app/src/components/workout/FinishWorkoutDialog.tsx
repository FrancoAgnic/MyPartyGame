"use client";

import { useState } from "react";
import type { WorkoutFeedback } from "@/lib/types";
import { Modal } from "@/components/ui/Modal";
import { Button, Field } from "@/components/ui/primitives";
import { cn } from "@/lib/utils";

/** Post-workout questionnaire (spec section 12). */
export function FinishWorkoutDialog({
  open,
  onClose,
  onConfirm,
}: {
  open: boolean;
  onClose: () => void;
  onConfirm: (feedback: WorkoutFeedback) => void;
}) {
  const [difficulty, setDifficulty] = useState(6);
  const [hadPain, setHadPain] = useState(false);
  const [painNote, setPainNote] = useState("");
  const [energy, setEnergy] = useState(3);
  const [technique, setTechnique] = useState(4);
  const [sleptWell, setSleptWell] = useState(true);
  const [repeatWeight, setRepeatWeight] = useState(false);

  const submit = () => {
    onConfirm({
      difficulty,
      hadPain,
      painNote: hadPain ? painNote : undefined,
      energy,
      technique,
      sleptWell,
      repeatWeightNextTime: repeatWeight,
    });
  };

  return (
    <Modal
      open={open}
      onClose={onClose}
      title="¿Cómo estuvo el entrenamiento?"
      footer={
        <div className="flex gap-2">
          <Button variant="outline" className="flex-1" onClick={onClose}>
            Cancelar
          </Button>
          <Button className="flex-1" onClick={submit}>
            Finalizar
          </Button>
        </div>
      }
    >
      <div className="space-y-4">
        <Field label={`¿Qué tan difícil fue? (${difficulty}/10)`}>
          <input
            type="range"
            min={1}
            max={10}
            value={difficulty}
            onChange={(e) => setDifficulty(Number(e.target.value))}
            className="w-full accent-primary"
          />
        </Field>

        <div className="grid grid-cols-2 gap-3">
          <Scale label="Energía" value={energy} onChange={setEnergy} max={5} />
          <Scale label="Técnica" value={technique} onChange={setTechnique} max={5} />
        </div>

        <ToggleRow label="¿Tuviste dolor?" value={hadPain} onChange={setHadPain} />
        {hadPain ? (
          <Field label="¿Dónde / qué molestia?">
            <textarea
              value={painNote}
              onChange={(e) => setPainNote(e.target.value)}
              className="min-h-[64px] w-full rounded-xl border border-border bg-surface-2 p-3 text-sm focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-primary"
              placeholder="Ej. molestia leve en hombro derecho"
            />
          </Field>
        ) : null}

        <ToggleRow label="¿Dormiste bien?" value={sleptWell} onChange={setSleptWell} />
        <ToggleRow
          label="¿Querés repetir el peso la próxima sesión?"
          value={repeatWeight}
          onChange={setRepeatWeight}
        />
      </div>
    </Modal>
  );
}

function ToggleRow({
  label,
  value,
  onChange,
}: {
  label: string;
  value: boolean;
  onChange: (v: boolean) => void;
}) {
  return (
    <div className="flex items-center justify-between gap-3">
      <span className="text-sm font-medium">{label}</span>
      <div className="flex gap-2">
        <Button
          size="sm"
          variant={value ? "primary" : "secondary"}
          onClick={() => onChange(true)}
        >
          Sí
        </Button>
        <Button
          size="sm"
          variant={!value ? "primary" : "secondary"}
          onClick={() => onChange(false)}
        >
          No
        </Button>
      </div>
    </div>
  );
}

function Scale({
  label,
  value,
  onChange,
  max,
}: {
  label: string;
  value: number;
  onChange: (v: number) => void;
  max: number;
}) {
  return (
    <Field label={label}>
      <div className="flex gap-1">
        {Array.from({ length: max }).map((_, i) => {
          const n = i + 1;
          return (
            <button
              key={n}
              onClick={() => onChange(n)}
              aria-label={`${label} ${n}`}
              className={cn(
                "h-9 flex-1 rounded-lg text-sm font-semibold",
                n <= value ? "bg-primary text-primary-fg" : "bg-surface-2 text-muted",
              )}
            >
              {n}
            </button>
          );
        })}
      </div>
    </Field>
  );
}
