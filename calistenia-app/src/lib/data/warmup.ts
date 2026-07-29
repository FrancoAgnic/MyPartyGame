import type { WarmupSection, HandstandLevel } from "@/lib/types";

/** Calentamiento guiado de 10 a 15 minutos, antes de cada entrenamiento. */
export const WARMUP_SECTIONS: WarmupSection[] = [
  {
    id: "munecas",
    title: "Calentamiento de muñecas",
    exercises: [
      {
        id: "circulos-muneca",
        name: "Círculos de muñeca",
        imageEmoji: "🔄",
        prescription: "20s por dirección",
        instructions: ["Rotá las muñecas en ambos sentidos de forma controlada."],
      },
      {
        id: "balanceos-palmas",
        name: "Balanceos con las palmas apoyadas",
        imageEmoji: "🤲",
        prescription: "10 repeticiones",
        instructions: ["Con las palmas en el suelo, balanceá el peso adelante y atrás."],
      },
      {
        id: "dedos-atras",
        name: "Apoyo con dedos orientados hacia atrás",
        imageEmoji: "👇",
        prescription: "20s",
        instructions: ["Apoyá las palmas con los dedos hacia vos y presioná suave."],
      },
      {
        id: "elevaciones-palma",
        name: "Elevaciones de palma manteniendo los dedos apoyados",
        imageEmoji: "✋",
        prescription: "2 × 10",
        instructions: ["Despegá la palma manteniendo los dedos en el suelo."],
      },
    ],
  },
  {
    id: "escapular",
    title: "Activación escapular",
    exercises: [
      {
        id: "scapular-pushups",
        name: "Scapular push-ups",
        imageEmoji: "🔻",
        prescription: "2 × 10",
        instructions: ["En plancha, protraé y retraé las escápulas sin doblar los codos."],
      },
      {
        id: "scapular-pullups",
        name: "Scapular pull-ups o jalón escapular en polea",
        imageEmoji: "⬇️",
        prescription: "2 × 8–10",
        instructions: ["Deprimí las escápulas sin flexionar los codos."],
      },
      {
        id: "band-pull-aparts",
        name: "Band pull-aparts o face pulls ligeros",
        imageEmoji: "🎯",
        prescription: "2 × 15",
        instructions: ["Abrí la banda a la altura del pecho juntando las escápulas."],
      },
    ],
  },
  {
    id: "hombros",
    title: "Activación de hombros",
    exercises: [
      {
        id: "rotaciones-externas",
        name: "Rotaciones externas con polea o banda",
        imageEmoji: "🔁",
        prescription: "2 × 12 por lado",
        instructions: ["Codo pegado al costado, rotá el antebrazo hacia afuera."],
      },
      {
        id: "elevaciones-frontales",
        name: "Elevaciones frontales sin peso",
        imageEmoji: "🙆",
        prescription: "10 repeticiones",
        instructions: ["Elevá los brazos al frente hasta la horizontal."],
      },
      {
        id: "wall-slides",
        name: "Wall slides",
        imageEmoji: "🧱",
        prescription: "2 × 10",
        instructions: ["Deslizá los brazos por la pared manteniendo el contacto."],
      },
    ],
  },
  {
    id: "core",
    title: "Activación de core",
    exercises: [
      {
        id: "hollow-warmup",
        name: "Hollow body hold",
        imageEmoji: "🌙",
        prescription: "2 × 20s",
        instructions: ["Lumbar en el suelo, hombros y piernas elevados."],
      },
      {
        id: "dead-bug",
        name: "Dead bug",
        imageEmoji: "🐞",
        prescription: "2 × 8 por lado",
        instructions: ["Extendé brazo y pierna opuestos sin arquear la lumbar."],
      },
    ],
  },
];

export const WARMUP_NOTES = [
  "No forzar dolor agudo.",
  "Aumentar gradualmente la carga.",
  "Mantener los codos extendidos sin bloquear agresivamente.",
];

/** Sistema de niveles para la progresión de handstand. */
export const HANDSTAND_LEVELS: HandstandLevel[] = [
  {
    id: 1,
    name: "Nivel 1 — Alineación contra la pared",
    goal: [
      "Mantener 30 a 45 segundos cara a la pared.",
      "Mantener codos extendidos.",
      "Controlar la pelvis.",
      "Empujar activamente el suelo.",
    ],
  },
  {
    id: 2,
    name: "Nivel 2 — Cambios de peso",
    goal: [
      "Transferir peso entre manos.",
      "Separar brevemente una mano.",
      "Realizar shoulder taps controlados.",
    ],
  },
  {
    id: 3,
    name: "Nivel 3 — Entradas libres",
    goal: [
      "Mejorar el kick-up.",
      "Reducir la fuerza excesiva de entrada.",
      "Encontrar el punto de equilibrio.",
    ],
  },
  {
    id: 4,
    name: "Nivel 4 — Handstand libre consistente",
    goal: [
      "Mantener 10 segundos en al menos 3 de 10 intentos.",
      "Mantener buena alineación.",
      "Salir de forma segura.",
    ],
  },
  {
    id: 5,
    name: "Nivel 5 — Fuerza avanzada",
    goal: [
      "Negativas de handstand push-up.",
      "Handstand push-up asistido.",
      "Presiones escapulares.",
      "Caminata controlada.",
    ],
  },
];

/** Señales de alarma para detener un ejercicio (pantalla de seguridad). */
export const SAFETY_WARNINGS = [
  "Dolor agudo.",
  "Mareo.",
  "Pérdida repentina de fuerza.",
  "Dolor articular persistente.",
  "Hormigueo.",
  "Dolor en el pecho.",
];
