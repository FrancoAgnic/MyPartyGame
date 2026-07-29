"use client";

import { useMemo, useState } from "react";
import { Search } from "lucide-react";
import { EXERCISES } from "@/lib/data/exercises";
import { Card, Input, EmptyState } from "@/components/ui/primitives";
import { ExerciseNotes } from "@/components/workout/ExerciseNotes";
import type { EquipmentTag, MuscleGroup } from "@/lib/types";
import { cn } from "@/lib/utils";

const MUSCLE_FILTERS: { key: MuscleGroup; label: string }[] = [
  { key: "pecho", label: "Pecho" },
  { key: "espalda", label: "Espalda" },
  { key: "hombros", label: "Hombros" },
  { key: "brazos", label: "Brazos" },
  { key: "piernas", label: "Piernas" },
  { key: "core", label: "Core" },
  { key: "handstand", label: "Handstand" },
  { key: "movilidad", label: "Movilidad" },
];

const EQUIP_FILTERS: { key: EquipmentTag; label: string }[] = [
  { key: "maquina", label: "Máquina" },
  { key: "mancuerna", label: "Mancuerna" },
  { key: "polea", label: "Polea" },
  { key: "peso-corporal", label: "Peso corporal" },
];

export default function BibliotecaPage() {
  const [query, setQuery] = useState("");
  const [muscle, setMuscle] = useState<MuscleGroup | null>(null);
  const [equipment, setEquipment] = useState<EquipmentTag | null>(null);

  const filtered = useMemo(() => {
    const q = query.trim().toLowerCase();
    return EXERCISES.filter((e) => {
      if (q && !e.name.toLowerCase().includes(q)) return false;
      if (muscle && !e.primaryMuscles.includes(muscle)) return false;
      if (equipment && !e.equipment.includes(equipment)) return false;
      return true;
    });
  }, [query, muscle, equipment]);

  return (
    <div className="space-y-5">
      <header>
        <h1 className="text-2xl font-bold">Biblioteca de ejercicios</h1>
        <p className="text-sm text-muted">{EXERCISES.length} ejercicios · técnica y sustituciones</p>
      </header>

      <div className="relative">
        <Search
          size={18}
          className="pointer-events-none absolute left-3 top-1/2 -translate-y-1/2 text-muted"
        />
        <Input
          value={query}
          onChange={(e) => setQuery(e.target.value)}
          placeholder="Buscar ejercicio…"
          className="pl-10"
          aria-label="Buscar ejercicio"
        />
      </div>

      <div className="space-y-2">
        <FilterRow
          items={MUSCLE_FILTERS}
          active={muscle}
          onToggle={(k) => setMuscle((cur) => (cur === k ? null : k))}
        />
        <FilterRow
          items={EQUIP_FILTERS}
          active={equipment}
          onToggle={(k) => setEquipment((cur) => (cur === k ? null : k))}
        />
      </div>

      {filtered.length === 0 ? (
        <EmptyState
          title="Sin resultados"
          description="Probá con otro término o quitá filtros."
        />
      ) : (
        <div className="space-y-3">
          {filtered.map((e) => (
            <Card key={e.id}>
              <div className="flex items-start gap-3">
                <span className="text-3xl">{e.imageEmoji}</span>
                <div className="min-w-0 flex-1">
                  <h3 className="font-bold">{e.name}</h3>
                  <div className="mt-1 flex flex-wrap gap-1.5">
                    {e.primaryMuscles.map((m) => (
                      <span
                        key={m}
                        className="rounded-full bg-primary/10 px-2 py-0.5 text-[11px] font-medium text-primary"
                      >
                        {m}
                      </span>
                    ))}
                    {e.equipment.map((eq) => (
                      <span
                        key={eq}
                        className="rounded-full bg-surface-2 px-2 py-0.5 text-[11px] font-medium text-muted"
                      >
                        {eq}
                      </span>
                    ))}
                    <span className="rounded-full bg-surface-2 px-2 py-0.5 text-[11px] font-medium text-muted">
                      {e.level}
                    </span>
                  </div>
                </div>
              </div>
              <div className="mt-3">
                <ExerciseNotes exercise={e} />
              </div>
            </Card>
          ))}
        </div>
      )}
    </div>
  );
}

function FilterRow<T extends string>({
  items,
  active,
  onToggle,
}: {
  items: { key: T; label: string }[];
  active: T | null;
  onToggle: (key: T) => void;
}) {
  return (
    <div className="flex flex-wrap gap-2">
      {items.map((it) => (
        <button
          key={it.key}
          onClick={() => onToggle(it.key)}
          className={cn(
            "rounded-full px-3 py-1.5 text-sm font-medium transition-colors",
            active === it.key
              ? "bg-primary text-primary-fg"
              : "bg-surface-2 text-muted hover:text-fg",
          )}
        >
          {it.label}
        </button>
      ))}
    </div>
  );
}
