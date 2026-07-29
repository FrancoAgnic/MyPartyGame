"use client";

import {
  ResponsiveContainer,
  LineChart,
  Line,
  XAxis,
  YAxis,
  Tooltip,
  CartesianGrid,
  BarChart,
  Bar,
} from "recharts";
import { EmptyState } from "@/components/ui/primitives";

export interface ChartPoint {
  label: string;
  value: number;
}

/** Shared axis/tooltip styling that reads the theme via CSS variables. */
const AXIS_STYLE = { fontSize: 11, fill: "rgb(148 158 176)" };
const GRID_COLOR = "rgba(148,158,176,0.15)";

function ChartTooltip({
  active,
  payload,
  label,
  unit,
}: {
  active?: boolean;
  payload?: { value: number }[];
  label?: string;
  unit?: string;
}) {
  if (!active || !payload || payload.length === 0) return null;
  return (
    <div className="rounded-lg border border-border bg-surface px-3 py-2 text-xs shadow-lg">
      <div className="font-semibold">{label}</div>
      <div className="text-muted">
        {payload[0]?.value}
        {unit ? ` ${unit}` : ""}
      </div>
    </div>
  );
}

export function TrendChart({
  data,
  unit,
  color = "rgb(99 102 241)",
  type = "line",
  emptyLabel = "Sin datos todavía",
}: {
  data: ChartPoint[];
  unit?: string;
  color?: string;
  type?: "line" | "bar";
  emptyLabel?: string;
}) {
  if (data.length === 0) {
    return (
      <EmptyState title={emptyLabel} description="Registrá datos para ver la evolución." />
    );
  }

  return (
    <div className="h-56 w-full">
      <ResponsiveContainer width="100%" height="100%">
        {type === "line" ? (
          <LineChart data={data} margin={{ top: 8, right: 8, bottom: 0, left: -16 }}>
            <CartesianGrid stroke={GRID_COLOR} vertical={false} />
            <XAxis dataKey="label" tick={AXIS_STYLE} tickLine={false} axisLine={false} />
            <YAxis tick={AXIS_STYLE} tickLine={false} axisLine={false} width={40} />
            <Tooltip content={<ChartTooltip unit={unit} />} />
            <Line
              type="monotone"
              dataKey="value"
              stroke={color}
              strokeWidth={2.5}
              dot={{ r: 3, fill: color }}
              activeDot={{ r: 5 }}
            />
          </LineChart>
        ) : (
          <BarChart data={data} margin={{ top: 8, right: 8, bottom: 0, left: -16 }}>
            <CartesianGrid stroke={GRID_COLOR} vertical={false} />
            <XAxis dataKey="label" tick={AXIS_STYLE} tickLine={false} axisLine={false} />
            <YAxis tick={AXIS_STYLE} tickLine={false} axisLine={false} width={40} />
            <Tooltip content={<ChartTooltip unit={unit} />} cursor={{ fill: GRID_COLOR }} />
            <Bar dataKey="value" fill={color} radius={[4, 4, 0, 0]} />
          </BarChart>
        )}
      </ResponsiveContainer>
    </div>
  );
}
