"use client";

import { useState } from "react";
import { Scale, Trophy, AlertTriangle } from "lucide-react";
import { useAppStore } from "@/lib/store";
import { Card, Button, Field, Input, inputClass } from "@/components/ui/primitives";
import { todayISO, formatDate } from "@/lib/utils";

/** Quick logging for body weight, progress tests and pain reports. */
export function MetricsSection() {
  const store = useAppStore();
  const [weight, setWeight] = useState("");
  const [pullups, setPullups] = useState("");
  const [dips, setDips] = useState("");
  const [hsSeconds, setHsSeconds] = useState("");
  const [painArea, setPainArea] = useState("");
  const [painSeverity, setPainSeverity] = useState("2");
  const [painNote, setPainNote] = useState("");

  const latestWeight = [...store.bodyMetrics]
    .filter((m) => m.bodyWeight != null)
    .sort((a, b) => a.date.localeCompare(b.date))
    .at(-1);

  return (
    <div className="space-y-4">
      {/* Body weight */}
      <Card>
        <div className="mb-3 flex items-center gap-2">
          <Scale size={18} className="text-primary" />
          <h3 className="font-semibold">Peso corporal</h3>
          {latestWeight ? (
            <span className="ml-auto text-xs text-muted">
              Último: {latestWeight.bodyWeight} {store.preferences.units} ·{" "}
              {formatDate(latestWeight.date)}
            </span>
          ) : null}
        </div>
        <div className="flex items-end gap-2">
          <Field label={`Peso (${store.preferences.units})`}>
            <Input
              type="number"
              value={weight}
              onChange={(e) => setWeight(e.target.value)}
              placeholder="ej. 72"
            />
          </Field>
          <Button
            onClick={() => {
              const v = Number(weight);
              if (!v) return;
              store.addBodyMetric({ date: todayISO(), bodyWeight: v });
              setWeight("");
            }}
          >
            Registrar
          </Button>
        </div>
      </Card>

      {/* Progress test */}
      <Card>
        <div className="mb-3 flex items-center gap-2">
          <Trophy size={18} className="text-accent" />
          <h3 className="font-semibold">Test de progreso</h3>
        </div>
        <div className="grid grid-cols-3 gap-2">
          <Field label="Dominadas">
            <Input type="number" value={pullups} onChange={(e) => setPullups(e.target.value)} />
          </Field>
          <Field label="Fondos">
            <Input type="number" value={dips} onChange={(e) => setDips(e.target.value)} />
          </Field>
          <Field label="Handstand (s)">
            <Input type="number" value={hsSeconds} onChange={(e) => setHsSeconds(e.target.value)} />
          </Field>
        </div>
        <Button
          className="mt-3"
          onClick={() => {
            if (!pullups && !dips && !hsSeconds) return;
            store.addProgressTest({
              date: todayISO(),
              maxPullups: pullups ? Number(pullups) : undefined,
              maxDips: dips ? Number(dips) : undefined,
              handstandBestSeconds: hsSeconds ? Number(hsSeconds) : undefined,
            });
            setPullups("");
            setDips("");
            setHsSeconds("");
          }}
        >
          Guardar test
        </Button>
      </Card>

      {/* Pain report */}
      <Card className="border-warning/30">
        <div className="mb-3 flex items-center gap-2">
          <AlertTriangle size={18} className="text-warning" />
          <h3 className="font-semibold">Registrar molestia o dolor</h3>
        </div>
        <div className="space-y-2">
          <div className="grid grid-cols-2 gap-2">
            <Field label="Zona">
              <Input
                value={painArea}
                onChange={(e) => setPainArea(e.target.value)}
                placeholder="ej. hombro derecho"
              />
            </Field>
            <Field label="Intensidad (1-5)">
              <select
                className={inputClass}
                value={painSeverity}
                onChange={(e) => setPainSeverity(e.target.value)}
              >
                {[1, 2, 3, 4, 5].map((n) => (
                  <option key={n} value={n}>
                    {n}
                  </option>
                ))}
              </select>
            </Field>
          </div>
          <Field label="Nota (opcional)">
            <Input value={painNote} onChange={(e) => setPainNote(e.target.value)} />
          </Field>
          <Button
            variant="outline"
            onClick={() => {
              if (!painArea) return;
              store.addPainReport({
                date: todayISO(),
                area: painArea,
                severity: Number(painSeverity),
                note: painNote || undefined,
              });
              setPainArea("");
              setPainNote("");
              setPainSeverity("2");
            }}
          >
            Registrar molestia
          </Button>
        </div>
        {store.painReports.length > 0 ? (
          <ul className="mt-3 space-y-1 text-xs text-muted">
            {[...store.painReports]
              .sort((a, b) => b.date.localeCompare(a.date))
              .slice(0, 4)
              .map((p) => (
                <li key={p.id}>
                  {formatDate(p.date)} · {p.area} · intensidad {p.severity}/5
                  {p.note ? ` · ${p.note}` : ""}
                </li>
              ))}
          </ul>
        ) : null}
      </Card>
    </div>
  );
}
