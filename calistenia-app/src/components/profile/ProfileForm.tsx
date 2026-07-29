"use client";

import { useForm } from "react-hook-form";
import { zodResolver } from "@hookform/resolvers/zod";
import { Check } from "lucide-react";
import { useState } from "react";
import { profileSchema, type ProfileFormValues } from "@/lib/schemas";
import { useAppStore } from "@/lib/store";
import { Button, Field, Input, inputClass } from "@/components/ui/primitives";

export function ProfileForm() {
  const profile = useAppStore((s) => s.profile);
  const updateProfile = useAppStore((s) => s.updateProfile);
  const [saved, setSaved] = useState(false);

  const {
    register,
    handleSubmit,
    formState: { errors, isDirty },
    reset,
  } = useForm<ProfileFormValues>({
    resolver: zodResolver(profileSchema),
    defaultValues: {
      name: profile.name,
      level: profile.level,
      frequency: profile.frequency,
      strictPullups: profile.strictPullups,
      freeDips: profile.freeDips,
      freeHandstandSeconds: profile.freeHandstandSeconds,
      location: profile.location,
    },
  });

  const onSubmit = (values: ProfileFormValues) => {
    updateProfile({
      name: values.name,
      level: values.level,
      frequency: values.frequency as 3 | 4 | 5,
      strictPullups: values.strictPullups,
      freeDips: values.freeDips,
      freeHandstandSeconds: values.freeHandstandSeconds,
      location: values.location,
    });
    reset(values);
    setSaved(true);
    setTimeout(() => setSaved(false), 2000);
  };

  return (
    <form onSubmit={handleSubmit(onSubmit)} className="space-y-4">
      <Field label="Nombre" error={errors.name?.message}>
        <Input {...register("name")} />
      </Field>

      <div className="grid grid-cols-2 gap-3">
        <Field label="Nivel">
          <select className={inputClass} {...register("level")}>
            <option value="principiante">Principiante</option>
            <option value="intermedio">Intermedio</option>
            <option value="avanzado">Avanzado</option>
          </select>
        </Field>
        <Field label="Frecuencia (días/sem)" error={errors.frequency?.message}>
          <select className={inputClass} {...register("frequency")}>
            <option value={3}>3 días</option>
            <option value={4}>4 días</option>
            <option value={5}>5 días</option>
          </select>
        </Field>
      </div>

      <div className="grid grid-cols-3 gap-3">
        <Field label="Dominadas" error={errors.strictPullups?.message}>
          <Input type="number" {...register("strictPullups")} />
        </Field>
        <Field label="Fondos" error={errors.freeDips?.message}>
          <Input type="number" {...register("freeDips")} />
        </Field>
        <Field label="Handstand (s)" error={errors.freeHandstandSeconds?.message}>
          <Input type="number" {...register("freeHandstandSeconds")} />
        </Field>
      </div>

      <Field label="Lugar de entrenamiento" error={errors.location?.message}>
        <Input {...register("location")} />
      </Field>

      <Button type="submit" disabled={!isDirty}>
        {saved ? (
          <>
            <Check size={16} /> Guardado
          </>
        ) : (
          "Guardar perfil"
        )}
      </Button>
    </form>
  );
}
