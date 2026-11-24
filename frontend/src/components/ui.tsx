import type { ButtonHTMLAttributes, InputHTMLAttributes, ReactNode } from "react";

export function cx(...c: (string | false | null | undefined)[]) {
  return c.filter(Boolean).join(" ");
}

export function Card({ title, icon, right, children, className }: {
  title?: ReactNode; icon?: ReactNode; right?: ReactNode; children: ReactNode; className?: string;
}) {
  return (
    <section className={cx("rounded-2xl border border-slate-200 bg-white p-5 shadow-sm", className)}>
      {(title || right) && (
        <header className="mb-3 flex items-center justify-between gap-2">
          <h3 className="flex items-center gap-2 font-semibold text-slate-700">
            {icon && <span className="text-xl">{icon}</span>}
            {title}
          </h3>
          {right}
        </header>
      )}
      {children}
    </section>
  );
}

type Variant = "primary" | "secondary" | "danger" | "ghost";
const VARIANTS: Record<Variant, string> = {
  primary: "bg-brand-600 text-white hover:bg-brand-700",
  secondary: "bg-slate-100 text-slate-700 hover:bg-slate-200",
  danger: "bg-red-600 text-white hover:bg-red-700",
  ghost: "text-slate-600 hover:bg-slate-100",
};

export function Button({ variant = "primary", className, ...p }: ButtonHTMLAttributes<HTMLButtonElement> & { variant?: Variant }) {
  return (
    <button
      {...p}
      className={cx("rounded-lg px-4 py-2 text-sm font-medium transition disabled:cursor-not-allowed disabled:opacity-50",
        VARIANTS[variant], className)}
    />
  );
}

export function Input({ label, className, ...p }: InputHTMLAttributes<HTMLInputElement> & { label?: string }) {
  return (
    <label className="block">
      {label && <span className="mb-1 block text-sm font-medium text-slate-600">{label}</span>}
      <input
        {...p}
        className={cx("w-full rounded-lg border border-slate-300 px-3 py-2 text-sm outline-none focus:border-brand-500 focus:ring-2 focus:ring-brand-100", className)}
      />
    </label>
  );
}

export function Badge({ tone = "slate", children }: { tone?: "green" | "red" | "amber" | "slate" | "blue"; children: ReactNode }) {
  const tones = {
    green: "bg-emerald-100 text-emerald-700",
    red: "bg-red-100 text-red-700",
    amber: "bg-amber-100 text-amber-700",
    slate: "bg-slate-100 text-slate-600",
    blue: "bg-sky-100 text-sky-700",
  };
  return <span className={cx("inline-flex items-center rounded-full px-2.5 py-0.5 text-xs font-medium", tones[tone])}>{children}</span>;
}

export function Alert({ children, tone = "red" }: { children: ReactNode; tone?: "red" | "green" }) {
  if (!children) return null;
  return (
    <div className={cx("rounded-lg px-3 py-2 text-sm", tone === "red" ? "bg-red-50 text-red-700" : "bg-emerald-50 text-emerald-700")}>
      {children}
    </div>
  );
}

export function Gauge({ value, max = 100, unit = "%", warnAt, dangerAt }: {
  value: number | null | undefined; max?: number; unit?: string; warnAt?: number; dangerAt?: number;
}) {
  const v = typeof value === "number" ? value : null;
  const pct = v === null ? 0 : Math.max(0, Math.min(100, (v / max) * 100));
  const color = v !== null && dangerAt !== undefined && v >= dangerAt ? "bg-red-500"
    : v !== null && warnAt !== undefined && v >= warnAt ? "bg-amber-500" : "bg-brand-500";
  return (
    <div>
      <div className="mb-1 text-3xl font-bold tabular-nums">
        {v === null ? "—" : Math.round(v * 10) / 10}
        <span className="ml-1 text-base font-normal text-slate-400">{unit}</span>
      </div>
      <div className="h-2 rounded-full bg-slate-100">
        <div className={cx("h-2 rounded-full transition-all", color)} style={{ width: `${pct}%` }} />
      </div>
    </div>
  );
}

export function fmtDate(s: string | null | undefined) {
  if (!s) return "—";
  return new Date(s).toLocaleString("ko-KR", { dateStyle: "short", timeStyle: "short" });
}

export function Spinner() {
  return <div className="p-10 text-center text-slate-400">불러오는 중…</div>;
}
