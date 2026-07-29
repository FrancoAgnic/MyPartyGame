import {
  Home,
  Dumbbell,
  Activity,
  TrendingUp,
  BookOpen,
  Calendar,
  User,
  type LucideIcon,
} from "lucide-react";

export interface NavItem {
  href: string;
  label: string;
  icon: LucideIcon;
  /** Shown in the mobile bottom bar. */
  mobile: boolean;
}

export const NAV_ITEMS: NavItem[] = [
  { href: "/", label: "Inicio", icon: Home, mobile: true },
  { href: "/entrenar", label: "Entrenar", icon: Dumbbell, mobile: true },
  { href: "/handstand", label: "Handstand", icon: Activity, mobile: true },
  { href: "/progreso", label: "Progreso", icon: TrendingUp, mobile: true },
  { href: "/biblioteca", label: "Biblioteca", icon: BookOpen, mobile: false },
  { href: "/calendario", label: "Calendario", icon: Calendar, mobile: false },
  { href: "/perfil", label: "Perfil", icon: User, mobile: true },
];
