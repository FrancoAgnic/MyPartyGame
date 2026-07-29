import { z } from "zod";

/** Validation schema for the editable user profile (React Hook Form + Zod). */
export const profileSchema = z.object({
  name: z.string().min(1, "Ingresá tu nombre").max(40),
  level: z.enum(["principiante", "intermedio", "avanzado"]),
  frequency: z.coerce.number().int().min(3).max(5),
  strictPullups: z.coerce.number().int().min(0).max(100),
  freeDips: z.coerce.number().int().min(0).max(200),
  freeHandstandSeconds: z.coerce.number().min(0).max(600),
  location: z.string().min(1, "Ingresá tu lugar de entrenamiento").max(60),
});

export type ProfileFormValues = z.infer<typeof profileSchema>;

export const bodyMetricSchema = z.object({
  bodyWeight: z.coerce.number().positive("Ingresá un peso válido").max(500),
});

export const progressTestSchema = z.object({
  maxPullups: z.coerce.number().int().min(0).max(100).optional(),
  maxDips: z.coerce.number().int().min(0).max(200).optional(),
  handstandBestSeconds: z.coerce.number().min(0).max(600).optional(),
});

export const painReportSchema = z.object({
  area: z.string().min(1, "Indicá la zona"),
  severity: z.coerce.number().int().min(1).max(5),
  note: z.string().max(200).optional(),
});
