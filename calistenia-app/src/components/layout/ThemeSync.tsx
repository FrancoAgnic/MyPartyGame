"use client";

import { useEffect } from "react";
import { useAppStore } from "@/lib/store";

/** Applies the persisted theme preference to <html> on the client. */
export function ThemeSync() {
  const theme = useAppStore((s) => s.preferences.theme);

  useEffect(() => {
    const root = document.documentElement;
    root.classList.remove("dark", "light");
    root.classList.add(theme);
  }, [theme]);

  return null;
}
