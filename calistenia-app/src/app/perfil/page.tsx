"use client";

import Link from "next/link";
import { useState } from "react";
import {
  Moon,
  Sun,
  ShieldAlert,
  LogOut,
  Trash2,
  Settings2,
} from "lucide-react";
import { useAppStore } from "@/lib/store";
import { useMounted } from "@/lib/hooks";
import { Card, Button, SectionHeading, inputClass } from "@/components/ui/primitives";
import { ProfileForm } from "@/components/profile/ProfileForm";
import { MetricsSection } from "@/components/profile/MetricsSection";
import { SAFETY_WARNINGS } from "@/lib/data/warmup";
import { signOut, deleteAccount, isSupabaseEnabled } from "@/lib/supabase/auth";

export default function PerfilPage() {
  const mounted = useMounted();
  const store = useAppStore();
  const [confirmReset, setConfirmReset] = useState(false);

  if (!mounted) {
    return <div className="h-64 animate-pulse rounded-2xl bg-surface-2" />;
  }

  const { preferences, profile, updatePreferences, toggleTheme } = store;

  return (
    <div className="space-y-6">
      <header>
        <h1 className="text-2xl font-bold">Perfil y configuración</h1>
        <p className="text-sm text-muted">Editá tu perfil, preferencias y métricas</p>
      </header>

      {/* Profile */}
      <section>
        <SectionHeading title="Perfil" />
        <Card>
          <ProfileForm />
        </Card>
        <div className="mt-3 rounded-xl bg-surface-2/50 p-3 text-xs text-muted">
          <p className="font-semibold text-fg">Objetivos</p>
          <ul className="mt-1 space-y-0.5">
            {profile.goals.map((g) => (
              <li key={g}>• {g}</li>
            ))}
          </ul>
          <p className="mt-2 font-semibold text-fg">Equipamiento no garantizado</p>
          <p className="mt-0.5">{profile.equipmentNotGuaranteed.join(", ")}</p>
        </div>
      </section>

      {/* Preferences */}
      <section>
        <SectionHeading title="Preferencias" />
        <Card className="space-y-4">
          <div className="flex items-center justify-between">
            <span className="flex items-center gap-2 text-sm font-medium">
              {preferences.theme === "dark" ? <Moon size={16} /> : <Sun size={16} />}
              Tema
            </span>
            <Button size="sm" variant="secondary" onClick={toggleTheme}>
              {preferences.theme === "dark" ? "Oscuro" : "Claro"}
            </Button>
          </div>

          <div className="flex items-center justify-between">
            <span className="text-sm font-medium">Unidades</span>
            <div className="flex gap-2">
              {(["kg", "lb"] as const).map((u) => (
                <Button
                  key={u}
                  size="sm"
                  variant={preferences.units === u ? "primary" : "secondary"}
                  onClick={() => updatePreferences({ units: u })}
                >
                  {u}
                </Button>
              ))}
            </div>
          </div>

          <div className="flex items-center justify-between gap-3">
            <span className="text-sm font-medium">Duración de sesión (min)</span>
            <input
              type="number"
              value={preferences.sessionDurationMinutes}
              onChange={(e) =>
                updatePreferences({ sessionDurationMinutes: Number(e.target.value) })
              }
              className={inputClass + " w-24 text-center"}
            />
          </div>

          <ToggleRow
            label="Preferir máquinas sobre calistenia"
            value={preferences.preferMachinesOverCalisthenics}
            onChange={(v) => updatePreferences({ preferMachinesOverCalisthenics: v })}
          />
          <ToggleRow
            label="Sonido del temporizador"
            value={preferences.restTimerSoundEnabled}
            onChange={(v) => updatePreferences({ restTimerSoundEnabled: v })}
          />
          <ToggleRow
            label="Vibración del temporizador"
            value={preferences.restTimerVibrationEnabled}
            onChange={(v) => updatePreferences({ restTimerVibrationEnabled: v })}
          />
        </Card>
      </section>

      {/* Metrics */}
      <section>
        <SectionHeading title="Métricas" subtitle="Registrá tu progreso" />
        <MetricsSection />
      </section>

      {/* Safety */}
      <section>
        <SectionHeading title="Seguridad" />
        <Card className="border-danger/30 bg-danger/5">
          <div className="flex items-start gap-3">
            <ShieldAlert className="mt-0.5 shrink-0 text-danger" size={20} />
            <div className="text-sm">
              <p className="font-semibold">Detené el ejercicio si aparece:</p>
              <ul className="mt-1 space-y-0.5 text-muted">
                {SAFETY_WARNINGS.map((w) => (
                  <li key={w}>• {w}</li>
                ))}
              </ul>
              <p className="mt-2 text-xs text-muted">
                Esta aplicación no reemplaza a un médico ni a un fisioterapeuta y
                no diagnostica lesiones. Ante síntomas persistentes, consultá a un
                profesional.
              </p>
            </div>
          </div>
        </Card>
      </section>

      {/* Account */}
      <section>
        <SectionHeading title="Cuenta" />
        <Card className="space-y-3">
          <p className="text-sm text-muted">
            {isSupabaseEnabled()
              ? "Autenticación con Supabase habilitada."
              : "Modo demo: tus datos se guardan en este dispositivo (localStorage). Configurá Supabase para sincronizar en la nube."}
          </p>
          <div className="flex flex-wrap gap-2">
            <Link href="/login">
              <Button variant="secondary" size="sm">
                <Settings2 size={14} /> Iniciar sesión / Registrarse
              </Button>
            </Link>
            <Button variant="ghost" size="sm" onClick={() => signOut()}>
              <LogOut size={14} /> Cerrar sesión
            </Button>
            <Button
              variant="ghost"
              size="sm"
              onClick={() => {
                if (confirm("¿Eliminar tu cuenta? Esta acción cierra tu sesión.")) {
                  deleteAccount();
                }
              }}
            >
              <Trash2 size={14} /> Eliminar cuenta
            </Button>
          </div>
        </Card>
      </section>

      {/* Data reset */}
      <section>
        <Card className="border-danger/30">
          <div className="flex items-center justify-between gap-3">
            <div>
              <h3 className="font-semibold">Restablecer datos</h3>
              <p className="text-xs text-muted">
                Borra tus sesiones y métricas de este dispositivo.
              </p>
            </div>
            {confirmReset ? (
              <div className="flex gap-2">
                <Button
                  size="sm"
                  variant="danger"
                  onClick={() => {
                    store.resetAll();
                    setConfirmReset(false);
                  }}
                >
                  Confirmar
                </Button>
                <Button size="sm" variant="ghost" onClick={() => setConfirmReset(false)}>
                  Cancelar
                </Button>
              </div>
            ) : (
              <Button size="sm" variant="outline" onClick={() => setConfirmReset(true)}>
                Restablecer
              </Button>
            )}
          </div>
        </Card>
      </section>
    </div>
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
    <label className="flex cursor-pointer items-center justify-between">
      <span className="text-sm font-medium">{label}</span>
      <input
        type="checkbox"
        checked={value}
        onChange={(e) => onChange(e.target.checked)}
        className="h-5 w-9 cursor-pointer appearance-none rounded-full bg-surface-2 transition-colors checked:bg-primary relative before:absolute before:left-0.5 before:top-0.5 before:h-4 before:w-4 before:rounded-full before:bg-white before:transition-transform checked:before:translate-x-4"
      />
    </label>
  );
}
