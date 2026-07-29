/**
 * Generates supabase/seed.sql from the TypeScript content (single source of
 * truth). Run with:  npm run seed:sql
 *
 * The exercise library, the 3-day routine and the default profile all live in
 * src/lib/data as typed content; this script serializes them to idempotent
 * SQL upserts so the Supabase database matches the app exactly.
 */
import { writeFileSync } from "node:fs";
import { fileURLToPath } from "node:url";
import { dirname, join } from "node:path";
import { EXERCISES } from "../src/lib/data/exercises.ts";
import { TRAINING_DAYS } from "../src/lib/data/routine.ts";

function q(value: string | undefined | null): string {
  if (value == null) return "null";
  return `'${value.replace(/'/g, "''")}'`;
}

function arr(values: string[] | undefined): string {
  if (!values || values.length === 0) return "'{}'";
  const inner = values.map((v) => `"${v.replace(/"/g, '\\"')}"`).join(",");
  return `'{${inner}}'`;
}

const lines: string[] = [];
lines.push("-- =============================================================");
lines.push("-- Seed autogenerado desde src/lib/data (npm run seed:sql).");
lines.push("-- Contenido global: biblioteca de ejercicios y rutina base.");
lines.push("-- =============================================================");
lines.push("");

// -------- Exercises
lines.push("-- Biblioteca de ejercicios");
for (const e of EXERCISES) {
  lines.push(
    `insert into public.exercises (id,name,category,primary_muscles,equipment,level,is_bodyweight,image_emoji,video_url,instructions,breathing,common_mistakes,corrections,regression,progression,planet_fitness_substitutions,contraindications) values (`,
  );
  lines.push(
    `  ${q(e.id)}, ${q(e.name)}, ${q(e.category)}, ${arr(
      e.primaryMuscles,
    )}, ${arr(e.equipment)}, ${q(e.level)}, ${e.isBodyweight}, ${q(
      e.imageEmoji,
    )}, ${q(e.videoUrl)}, ${arr(e.instructions)}, ${q(e.breathing)}, ${arr(
      e.commonMistakes,
    )}, ${arr(e.corrections)}, ${q(e.regression)}, ${q(e.progression)}, ${arr(
      e.planetFitnessSubstitutions,
    )}, ${q(e.contraindications)}`,
  );
  lines.push(
    `) on conflict (id) do update set name=excluded.name, category=excluded.category, primary_muscles=excluded.primary_muscles, equipment=excluded.equipment, level=excluded.level, is_bodyweight=excluded.is_bodyweight, image_emoji=excluded.image_emoji, instructions=excluded.instructions, breathing=excluded.breathing, common_mistakes=excluded.common_mistakes, corrections=excluded.corrections, regression=excluded.regression, progression=excluded.progression, planet_fitness_substitutions=excluded.planet_fitness_substitutions, contraindications=excluded.contraindications;`,
  );
  lines.push("");
}

// -------- Routine reference (documented as comments so it can be attached to a
// user's plan on signup by the application layer).
lines.push("-- Rutina base de 3 días (referencia). El plan por usuario se crea");
lines.push("-- desde la app; este bloque documenta la prescripción canónica.");
for (const day of TRAINING_DAYS) {
  lines.push(`-- Día ${day.code}: ${day.title} (weekday ${day.weekday})`);
  for (const block of day.blocks) {
    lines.push(`--   [${block.title}]`);
    for (const ex of block.exercises) {
      const range =
        ex.repsMax != null
          ? `${ex.repsMin ?? ""}-${ex.repsMax} reps`
          : `${ex.timeSecondsMin ?? ""}-${ex.timeSecondsMax ?? ""}s`;
      lines.push(
        `--     ${ex.exerciseId}: ${ex.sets} x ${range}, rest ${ex.restSeconds}s`,
      );
    }
  }
}
lines.push("");

const __dirname = dirname(fileURLToPath(import.meta.url));
const out = join(__dirname, "..", "supabase", "seed.sql");
writeFileSync(out, lines.join("\n"), "utf8");
console.log(`Wrote ${out} (${EXERCISES.length} exercises).`);
