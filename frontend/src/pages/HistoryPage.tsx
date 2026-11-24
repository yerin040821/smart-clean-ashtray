import { useMemo, useState } from "react";
import { Link, useParams } from "react-router-dom";
import { CartesianGrid, Legend, Line, LineChart, ResponsiveContainer, Tooltip, XAxis, YAxis } from "recharts";
import { Card, cx, Spinner } from "../components/ui";
import { useFetch } from "../hooks/useFetch";

type Row = { t: string } & Record<string, unknown>;

const SERIES: { key: string; label: string; color: string; unit: string }[] = [
  { key: "gas_diff", label: "공기질 (가스 변화량)", color: "#6366f1", unit: "Δ" },
  { key: "ash_level", label: "재 잔량", color: "#f59e0b", unit: "%" },
  { key: "water_level", label: "수위", color: "#0ea5e9", unit: "%" },
  { key: "temperature", label: "온도", color: "#ef4444", unit: "°C" },
  { key: "humidity", label: "습도", color: "#10b981", unit: "%" },
];

const RANGES = [
  { h: 1, label: "1시간" }, { h: 24, label: "24시간" }, { h: 24 * 7, label: "7일" }, { h: 24 * 30, label: "30일" },
];

export function HistoryPage() {
  const { id } = useParams();
  const [hours, setHours] = useState(24);
  const { data, loading } = useFetch<Row[]>(`/devices/${id}/history?hours=${hours}&limit=5000`);

  const available = useMemo(() => SERIES.filter((s) => data?.some((r) => typeof r[s.key] === "number")), [data]);
  const fmt = (t: string) => new Date(t).toLocaleString("ko-KR", hours > 24
    ? { month: "numeric", day: "numeric", hour: "2-digit" } : { hour: "2-digit", minute: "2-digit" });

  return (
    <div>
      <div className="mb-6 flex flex-wrap items-center justify-between gap-3">
        <h1 className="text-2xl font-bold"><Link to={`/devices/${id}`} className="text-slate-400 hover:text-slate-600">←</Link> 센서 히스토리</h1>
        <div className="flex gap-1 rounded-lg bg-slate-100 p-1">
          {RANGES.map((r) => (
            <button key={r.h} onClick={() => setHours(r.h)}
              className={cx("rounded-md px-3 py-1 text-sm", hours === r.h ? "bg-white font-semibold shadow-sm" : "text-slate-500")}>
              {r.label}
            </button>
          ))}
        </div>
      </div>
      {loading ? <Spinner /> : !data?.length ? (
        <Card><p className="py-10 text-center text-slate-500">해당 기간의 데이터가 없습니다.</p></Card>
      ) : (
        <div className="grid gap-4 lg:grid-cols-2">
          {available.map((s) => (
            <Card key={s.key} title={`${s.label} (${s.unit})`}>
              <div className="h-64">
                <ResponsiveContainer>
                  <LineChart data={data}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#e2e8f0" />
                    <XAxis dataKey="t" tickFormatter={fmt} minTickGap={40} fontSize={12} />
                    <YAxis fontSize={12} width={40} />
                    <Tooltip labelFormatter={(t) => new Date(String(t)).toLocaleString("ko-KR")} />
                    <Legend />
                    <Line type="monotone" dataKey={s.key} name={s.label} stroke={s.color} dot={false} strokeWidth={2} isAnimationActive={false} />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </Card>
          ))}
        </div>
      )}
    </div>
  );
}
