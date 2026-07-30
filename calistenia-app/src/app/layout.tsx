import type { Metadata, Viewport } from "next";
import "./globals.css";
import { AppShell } from "@/components/layout/AppShell";
import { ThemeSync } from "@/components/layout/ThemeSync";

export const metadata: Metadata = {
  title: "Calistenia · Entrenamiento y Handstand",
  description:
    "Planificación y seguimiento de entrenamiento: calistenia, hipertrofia del tren superior y progresión de handstand.",
};

export const viewport: Viewport = {
  themeColor: "#090b10",
  width: "device-width",
  initialScale: 1,
  maximumScale: 1,
};

export default function RootLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    // Default to dark; ThemeSync updates this from persisted preferences.
    <html lang="es" className="dark" suppressHydrationWarning>
      <body
        style={{ ["--font-sans" as string]: "'Inter', system-ui, sans-serif" }}
      >
        <ThemeSync />
        <AppShell>{children}</AppShell>
      </body>
    </html>
  );
}
