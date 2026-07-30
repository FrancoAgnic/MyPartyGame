"use client";

import { useState } from "react";
import { useRouter } from "next/navigation";
import Link from "next/link";
import { Activity, ArrowLeft } from "lucide-react";
import { Card, Button, Field, Input } from "@/components/ui/primitives";
import { signIn, signUp, resetPassword, isSupabaseEnabled } from "@/lib/supabase/auth";

type Mode = "signin" | "signup" | "reset";

export default function LoginPage() {
  const router = useRouter();
  const [mode, setMode] = useState<Mode>("signin");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [message, setMessage] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);

  const enabled = isSupabaseEnabled();

  const submit = async (e: React.FormEvent) => {
    e.preventDefault();
    setError(null);
    setMessage(null);
    setLoading(true);
    try {
      if (!enabled) {
        setMessage(
          "Modo demo activo: no se requiere cuenta. Configurá Supabase para habilitar la autenticación.",
        );
        return;
      }
      if (mode === "signin") {
        const r = await signIn(email, password);
        if (!r.ok) setError(r.error ?? "No se pudo iniciar sesión.");
        else router.push("/");
      } else if (mode === "signup") {
        const r = await signUp(email, password);
        if (!r.ok) setError(r.error ?? "No se pudo registrar.");
        else setMessage("Revisá tu correo para confirmar la cuenta.");
      } else {
        const r = await resetPassword(email);
        if (!r.ok) setError(r.error ?? "No se pudo enviar el correo.");
        else setMessage("Te enviamos un correo para restablecer la contraseña.");
      }
    } finally {
      setLoading(false);
    }
  };

  return (
    <div className="mx-auto max-w-sm space-y-5 py-6">
      <Link href="/" className="inline-flex items-center gap-1 text-sm text-muted">
        <ArrowLeft size={16} /> Volver
      </Link>

      <div className="flex items-center gap-3">
        <div className="flex h-11 w-11 items-center justify-center rounded-xl bg-primary text-primary-fg">
          <Activity size={22} />
        </div>
        <div>
          <h1 className="text-xl font-bold">
            {mode === "signup"
              ? "Crear cuenta"
              : mode === "reset"
                ? "Recuperar contraseña"
                : "Iniciar sesión"}
          </h1>
          <p className="text-xs text-muted">Calistenia · Handstand & Fuerza</p>
        </div>
      </div>

      {!enabled ? (
        <Card className="border-accent/30 bg-accent/5 text-sm text-muted">
          Estás en <strong className="text-fg">modo demo</strong>. Podés usar toda
          la app sin cuenta; los datos se guardan en este dispositivo. Para
          autenticación y sincronización, completá las variables de Supabase.
        </Card>
      ) : null}

      <Card>
        <form onSubmit={submit} className="space-y-4">
          <Field label="Email">
            <Input
              type="email"
              value={email}
              onChange={(e) => setEmail(e.target.value)}
              autoComplete="email"
              required
            />
          </Field>
          {mode !== "reset" ? (
            <Field label="Contraseña">
              <Input
                type="password"
                value={password}
                onChange={(e) => setPassword(e.target.value)}
                autoComplete={mode === "signup" ? "new-password" : "current-password"}
                required
                minLength={6}
              />
            </Field>
          ) : null}

          {error ? <p className="text-sm text-danger">{error}</p> : null}
          {message ? <p className="text-sm text-success">{message}</p> : null}

          <Button type="submit" className="w-full" disabled={loading}>
            {loading
              ? "Procesando…"
              : mode === "signup"
                ? "Registrarme"
                : mode === "reset"
                  ? "Enviar correo"
                  : "Entrar"}
          </Button>
        </form>
      </Card>

      <div className="space-y-1 text-center text-sm text-muted">
        {mode !== "signin" ? (
          <button className="text-primary" onClick={() => setMode("signin")}>
            ¿Ya tenés cuenta? Iniciá sesión
          </button>
        ) : (
          <>
            <button className="block w-full text-primary" onClick={() => setMode("signup")}>
              Crear una cuenta nueva
            </button>
            <button className="block w-full text-muted" onClick={() => setMode("reset")}>
              Olvidé mi contraseña
            </button>
          </>
        )}
      </div>
    </div>
  );
}
