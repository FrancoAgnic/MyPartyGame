"use client";

import { useState } from "react";
import { ChevronDown, Info } from "lucide-react";
import type { Exercise } from "@/lib/types";
import { cn } from "@/lib/utils";

/** Collapsible technical notes: instructions, mistakes, corrections, subs. */
export function ExerciseNotes({ exercise }: { exercise: Exercise }) {
  const [open, setOpen] = useState(false);
  return (
    <div className="rounded-xl bg-surface-2/60">
      <button
        onClick={() => setOpen((v) => !v)}
        className="flex w-full items-center justify-between px-3 py-2 text-left text-sm font-medium text-muted"
        aria-expanded={open}
      >
        <span className="flex items-center gap-2">
          <Info size={15} /> Notas técnicas
        </span>
        <ChevronDown
          size={16}
          className={cn("transition-transform", open && "rotate-180")}
        />
      </button>
      {open ? (
        <div className="space-y-3 px-3 pb-3 text-sm">
          <NoteBlock title="Instrucciones" items={exercise.instructions} />
          {exercise.breathing ? (
            <p className="text-muted">
              <span className="font-semibold text-fg">Respiración: </span>
              {exercise.breathing}
            </p>
          ) : null}
          <NoteBlock title="Errores comunes" items={exercise.commonMistakes} tone="danger" />
          <NoteBlock title="Correcciones" items={exercise.corrections} tone="success" />
          {exercise.regression ? (
            <p className="text-muted">
              <span className="font-semibold text-fg">Regresión: </span>
              {exercise.regression}
            </p>
          ) : null}
          {exercise.progression ? (
            <p className="text-muted">
              <span className="font-semibold text-fg">Progresión: </span>
              {exercise.progression}
            </p>
          ) : null}
          <NoteBlock
            title="Sustituciones en Planet Fitness"
            items={exercise.planetFitnessSubstitutions}
          />
          {exercise.contraindications ? (
            <p className="text-xs text-warning">
              ⚠ {exercise.contraindications}
            </p>
          ) : null}
        </div>
      ) : null}
    </div>
  );
}

function NoteBlock({
  title,
  items,
  tone,
}: {
  title: string;
  items: string[];
  tone?: "danger" | "success";
}) {
  if (items.length === 0) return null;
  const dot =
    tone === "danger"
      ? "text-danger"
      : tone === "success"
        ? "text-success"
        : "text-primary";
  return (
    <div>
      <h5 className="mb-1 text-xs font-bold uppercase tracking-wide text-muted">
        {title}
      </h5>
      <ul className="space-y-0.5">
        {items.map((it, i) => (
          <li key={i} className="flex gap-2 text-muted">
            <span className={dot}>•</span>
            <span>{it}</span>
          </li>
        ))}
      </ul>
    </div>
  );
}
