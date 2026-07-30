"use client";

import { getSupabaseClient, isSupabaseEnabled } from "./client";

/**
 * Thin auth helpers over Supabase Auth. In demo mode (no Supabase configured)
 * these are no-ops that resolve successfully, so the UI can be exercised
 * without a backend.
 */
export interface AuthResult {
  ok: boolean;
  error?: string;
}

export async function signUp(email: string, password: string): Promise<AuthResult> {
  const supabase = getSupabaseClient();
  if (!supabase) return { ok: true };
  const { error } = await supabase.auth.signUp({ email, password });
  return error ? { ok: false, error: error.message } : { ok: true };
}

export async function signIn(email: string, password: string): Promise<AuthResult> {
  const supabase = getSupabaseClient();
  if (!supabase) return { ok: true };
  const { error } = await supabase.auth.signInWithPassword({ email, password });
  return error ? { ok: false, error: error.message } : { ok: true };
}

export async function signOut(): Promise<void> {
  const supabase = getSupabaseClient();
  if (supabase) await supabase.auth.signOut();
}

export async function resetPassword(email: string): Promise<AuthResult> {
  const supabase = getSupabaseClient();
  if (!supabase) return { ok: true };
  const redirectTo =
    typeof window !== "undefined"
      ? `${window.location.origin}/login`
      : undefined;
  const { error } = await supabase.auth.resetPasswordForEmail(email, {
    redirectTo,
  });
  return error ? { ok: false, error: error.message } : { ok: true };
}

export async function deleteAccount(): Promise<AuthResult> {
  // Account deletion requires a privileged server call (service role). In this
  // MVP we sign the user out and surface guidance; wire an Edge Function /
  // route handler with the service role key to hard-delete the auth user.
  await signOut();
  return { ok: true };
}

export { isSupabaseEnabled };
