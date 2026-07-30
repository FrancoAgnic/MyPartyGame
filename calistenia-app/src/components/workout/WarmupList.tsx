"use client";

import { useState } from "react";
import { CheckCircle2, Circle, ChevronDown, Flame } from "lucide-react";
import { WARMUP_SECTIONS, WARMUP_NOTES } from "@/lib/data/warmup";
import { Card } from "@/components/ui/primitives";
import { cn } from "@/lib/utils";

/** Guided warmup with per-exercise completion checkboxes (local to the view). */
export function WarmupList() {
  const [open, setOpen] = useState(false);
  const [done, setDone] = useState<Record<string, boolean>>({});

  const total = WARMUP_SECTIONS.reduce((n, s) => n + s.exercises.length, 0);
  const completed = Object.values(done).filter(Boolean).length;

  return (
    <Card>
      <button
        onClick={() => setOpen((v) => !v)}
        className="flex w-full items-center justify-between text-left"
        aria-expanded={open}
      >
        <div className="flex items-center gap-3">
          <div className="flex h-10 w-10 items-center justify-center rounded-xl bg-warning/15 text-warning">
            <Flame size={20} />
          </div>
          <div>
            <div className="font-semibold">Calentamiento (10–15 min)</div>
            <div className="text-xs text-muted">
              {completed}/{total} ejercicios · muñecas, escápulas, hombros, core
            </div>
          </div>
        </div>
        <ChevronDown
          size={20}
          className={cn("text-muted transition-transform", open && "rotate-180")}
        />
      </button>

      {open ? (
        <div className="mt-4 space-y-4">
          {WARMUP_SECTIONS.map((section) => (
            <div key={section.id}>
              <h4 className="mb-2 text-sm font-bold text-muted">
                {section.title}
              </h4>
              <ul className="space-y-1.5">
                {section.exercises.map((ex) => {
                  const isDone = !!done[ex.id];
                  return (
                    <li key={ex.id}>
                      <button
                        onClick={() =>
                          setDone((d) => ({ ...d, [ex.id]: !d[ex.id] }))
                        }
                        className="flex w-full items-start gap-3 rounded-xl px-2 py-2 text-left hover:bg-surface-2"
                      >
                        {isDone ? (
                          <CheckCircle2
                            size={20}
                            className="mt-0.5 shrink-0 text-success"
                          />
                        ) : (
                          <Circle
                            size={20}
                            className="mt-0.5 shrink-0 text-muted"
                          />
                        )}
                        <span className="flex-1">
                          <span className="text-lg">{ex.imageEmoji}</span>{" "}
                          <span
                            className={cn(
                              "font-medium",
                              isDone && "text-muted line-through",
                            )}
                          >
                            {ex.name}
                          </span>
                          <span className="ml-2 text-xs text-muted">
                            {ex.prescription}
                          </span>
                          <span className="block text-xs text-muted">
                            {ex.instructions.join(" ")}
                          </span>
                        </span>
                      </button>
                    </li>
                  );
                })}
              </ul>
            </div>
          ))}
          <div className="rounded-xl bg-surface-2 p-3 text-xs text-muted">
            <ul className="list-inside list-disc space-y-0.5">
              {WARMUP_NOTES.map((n) => (
                <li key={n}>{n}</li>
              ))}
            </ul>
          </div>
        </div>
      ) : null}
    </Card>
  );
}
