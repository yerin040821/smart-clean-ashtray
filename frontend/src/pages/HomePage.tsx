import { Link } from "react-router-dom";
import type { DeviceSummary } from "../api/types";
import { Badge, Button, Card, fmtDate, Spinner } from "../components/ui";
import { useFetch } from "../hooks/useFetch";

function Metric({ label, value, unit }: { label: string; value: unknown; unit?: string }) {
  return (
    <div className="rounded-lg bg-slate-50 px-3 py-2">
      <p className="text-xs text-slate-400">{label}</p>
      <p className="font-semibold tabular-nums">{value === undefined || value === null ? "—" : `${value}${unit ?? ""}`}</p>
    </div>
  );
}

export function HomePage() {
  const { data, loading, error } = useFetch<DeviceSummary[]>("/devices");
  if (loading) return <Spinner />;
  return (
    <div>
      <div className="mb-6 flex items-center justify-between">
        <h1 className="text-2xl font-bold">내 기기</h1>
        <Link to="/devices/setup"><Button>+ 기기 등록</Button></Link>
      </div>
      {error && <p className="text-red-600">{error}</p>}
      {data?.length === 0 && (
        <Card><p className="py-10 text-center text-slate-500">등록된 기기가 없습니다. 기기를 등록해 주세요.</p></Card>
      )}
      <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
        {data?.map((d) => {
          const fire = d.latest?.fire === 1 || d.latest?.fire === true;
          return (
            <Link key={d.id} to={`/devices/${d.id}`}>
              <Card className={fire ? "border-red-400 bg-red-50 transition hover:shadow-md" : "transition hover:shadow-md"}
                title={d.name} right={<Badge tone={d.online ? "green" : "slate"}>{d.online ? "온라인" : "오프라인"}</Badge>}>
                <p className="mb-3 text-xs text-slate-400">{d.type_name} · {d.id} · {d.state ?? "—"}</p>
                {fire && <p className="mb-2 animate-pulse font-bold text-red-600">🔥 화재 감지</p>}
                <div className="grid grid-cols-3 gap-2 text-sm">
                  <Metric label="공기질 Δ" value={d.latest?.gas_diff} />
                  <Metric label="재 잔량" value={d.latest?.ash_level} unit="%" />
                  {d.capabilities.includes("temperature_humidity")
                    ? <Metric label="온도" value={d.latest?.temperature} unit="°C" />
                    : <Metric label="수위" value={d.latest?.water_level} unit="%" />}
                </div>
                <p className="mt-3 text-right text-xs text-slate-400">마지막 수신 {fmtDate(d.last_seen)}</p>
              </Card>
            </Link>
          );
        })}
      </div>
    </div>
  );
}
