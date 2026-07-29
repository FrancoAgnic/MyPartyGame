"use client";

import { useEffect, useState } from "react";

/**
 * True once the component has mounted on the client. Used to avoid hydration
 * mismatches when reading from localStorage-persisted state.
 */
export function useMounted(): boolean {
  const [mounted, setMounted] = useState(false);
  useEffect(() => setMounted(true), []);
  return mounted;
}
