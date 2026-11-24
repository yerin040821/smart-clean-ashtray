import type { ReactNode } from "react";

export interface Col<T> {
  key: string;
  label: string;
  render?: (row: T) => ReactNode;
}

export function Table<T extends object>({ rows, cols, empty = "데이터가 없습니다." }: { rows: T[] | null; cols: Col<T>[]; empty?: string }) {
  return (
    <div className="overflow-x-auto rounded-2xl border border-slate-200 bg-white shadow-sm">
      <table className="w-full text-left text-sm">
        <thead className="border-b border-slate-200 bg-slate-50 text-xs uppercase text-slate-500">
          <tr>{cols.map((c) => <th key={c.key} className="whitespace-nowrap px-4 py-3 font-medium">{c.label}</th>)}</tr>
        </thead>
        <tbody className="divide-y divide-slate-100">
          {!rows?.length && <tr><td colSpan={cols.length} className="px-4 py-10 text-center text-slate-400">{empty}</td></tr>}
          {rows?.map((r, i) => (
            <tr key={i} className="hover:bg-slate-50">
              {cols.map((c) => (
                <td key={c.key} className="px-4 py-3 align-top">
                  {c.render ? c.render(r) : String((r as Record<string, unknown>)[c.key] ?? "—")}
                </td>
              ))}
            </tr>
          ))}
        </tbody>
      </table>
    </div>
  );
}

export function PageTitle({ children, right }: { children: ReactNode; right?: ReactNode }) {
  return (
    <div className="mb-6 flex items-center justify-between">
      <h1 className="text-2xl font-bold">{children}</h1>
      {right}
    </div>
  );
}
