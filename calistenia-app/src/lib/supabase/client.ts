"use client";

import { createBrowserClient } from "@supabase/ssr";
import type { SupabaseClient } from "@supabase/supabase-js";

/**
 * Browser Supabase client. Returns `null` in demo mode (no env configured),
 * so the rest of the app can gracefully fall back to localStorage.
 */
let cached: SupabaseClient | null | undefined;

export function isSupabaseEnabled(): boolean {
  return (
    process.env.NEXT_PUBLIC_USE_SUPABASE === "true" &&
    !!process.env.NEXT_PUBLIC_SUPABASE_URL &&
    !!process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY
  );
}

export function getSupabaseClient(): SupabaseClient | null {
  if (cached !== undefined) return cached;
  if (!isSupabaseEnabled()) {
    cached = null;
    return cached;
  }
  cached = createBrowserClient(
    process.env.NEXT_PUBLIC_SUPABASE_URL!,
    process.env.NEXT_PUBLIC_SUPABASE_ANON_KEY!,
  );
  return cached;
}
