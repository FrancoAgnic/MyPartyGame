"use client";

import { useCallback, useEffect, useRef, useState } from "react";
import { Minus, Plus, X, Timer } from "lucide-react";
import { Button } from "@/components/ui/primitives";
import { formatSeconds } from "@/lib/utils";
import { useAppStore } from "@/lib/store";

export interface RestTimerHandle {
  start: (seconds: number) => void;
}

/**
 * Floating rest timer. Auto-starts when a set is completed. Supports adding /
 * removing time and optional sound + vibration on completion. Uses a wall-clock
 * end time so it keeps counting correctly even if the tab is backgrounded.
 */
export function useRestTimer() {
  const [endTime, setEndTime] = useState<number | null>(null);
  const [remaining, setRemaining] = useState(0);
  const rafRef = useRef<number | null>(null);
  const soundEnabled = useAppStore((s) => s.preferences.restTimerSoundEnabled);
  const vibrationEnabled = useAppStore(
    (s) => s.preferences.restTimerVibrationEnabled,
  );
  const firedRef = useRef(false);

  const notify = useCallback(() => {
    if (firedRef.current) return;
    firedRef.current = true;
    if (vibrationEnabled && typeof navigator !== "undefined" && navigator.vibrate) {
      navigator.vibrate([200, 100, 200]);
    }
    if (soundEnabled && typeof window !== "undefined") {
      try {
        const ctx = new (window.AudioContext ||
          (window as unknown as { webkitAudioContext: typeof AudioContext })
            .webkitAudioContext)();
        const osc = ctx.createOscillator();
        const gain = ctx.createGain();
        osc.connect(gain);
        gain.connect(ctx.destination);
        osc.frequency.value = 880;
        gain.gain.setValueAtTime(0.001, ctx.currentTime);
        gain.gain.exponentialRampToValueAtTime(0.2, ctx.currentTime + 0.02);
        gain.gain.exponentialRampToValueAtTime(0.001, ctx.currentTime + 0.4);
        osc.start();
        osc.stop(ctx.currentTime + 0.4);
      } catch {
        /* audio not available */
      }
    }
  }, [soundEnabled, vibrationEnabled]);

  useEffect(() => {
    if (endTime == null) return;
    const tick = () => {
      const rem = Math.max(0, Math.round((endTime - Date.now()) / 1000));
      setRemaining(rem);
      if (rem <= 0) {
        notify();
        return;
      }
      rafRef.current = window.setTimeout(tick, 250) as unknown as number;
    };
    tick();
    return () => {
      if (rafRef.current) clearTimeout(rafRef.current);
    };
  }, [endTime, notify]);

  const start = useCallback((seconds: number) => {
    firedRef.current = false;
    setEndTime(Date.now() + seconds * 1000);
    setRemaining(seconds);
  }, []);

  const adjust = useCallback(
    (delta: number) => {
      setEndTime((prev) => {
        if (prev == null) return prev;
        const next = Math.max(Date.now(), prev + delta * 1000);
        firedRef.current = false;
        return next;
      });
    },
    [],
  );

  const stop = useCallback(() => {
    setEndTime(null);
    setRemaining(0);
  }, []);

  const active = endTime != null && remaining > 0;

  return { active, remaining, start, adjust, stop };
}

export function RestTimerBar({
  remaining,
  onAdjust,
  onStop,
}: {
  remaining: number;
  onAdjust: (delta: number) => void;
  onStop: () => void;
}) {
  return (
    <div className="fixed inset-x-0 bottom-0 z-40 border-t border-border bg-surface/95 backdrop-blur">
      <div className="mx-auto flex max-w-3xl items-center gap-3 px-4 py-3">
        <Timer className="text-accent" size={22} />
        <span className="min-w-[64px] text-2xl font-bold tabular-nums text-accent">
          {formatSeconds(remaining)}
        </span>
        <span className="text-sm text-muted">Descanso</span>
        <div className="ml-auto flex items-center gap-2">
          <Button size="sm" variant="secondary" onClick={() => onAdjust(-15)}>
            <Minus size={16} /> 15s
          </Button>
          <Button size="sm" variant="secondary" onClick={() => onAdjust(15)}>
            <Plus size={16} /> 15s
          </Button>
          <Button size="sm" variant="ghost" onClick={onStop} aria-label="Cerrar temporizador">
            <X size={18} />
          </Button>
        </div>
      </div>
    </div>
  );
}
