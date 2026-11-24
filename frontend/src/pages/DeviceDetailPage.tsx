import { useCallback, useState } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import { api } from "../api/client";
import type { DeviceDetail, Telemetry } from "../api/types";
import { CAPABILITY_COMPONENTS, GenericCard } from "../components/capabilities";
import { Badge, Button, fmtDate, Spinner } from "../components/ui";
import { useDeviceSocket, type WsMessage } from "../hooks/useDeviceSocket";
import { useFetch } from "../hooks/useFetch";

export function DeviceDetailPage() {
  const { id } = useParams();
  const nav = useNavigate();
  const { data: device, setData, loading, error } = useFetch<DeviceDetail>(`/devices/${id}`);
  const [live, setLive] = useState<Telemetry>({});
  const [lastAt, setLastAt] = useState<string | null>(null);

  const onMessage = useCallback((m: WsMessage) => {
    if (m.type === "telemetry") {
      setLive((p) => ({ ...p, ...m.data }));
      setLastAt(m.ts);
    } else if (m.type === "status") {
      setData((d) => (d ? { ...d, online: ["online", "true", "1"].includes(String(m.data.status ?? m.data.online)) } : d));
    } else if (m.type === "event" && (m.data.to || m.data.state)) {
      setData((d) => (d ? { ...d, state: String(m.data.to ?? m.data.state) } : d));
    }
  }, [setData]);
  const connected = useDeviceSocket(id, onMessage);

  const send = useCallback(async (capability: string, params: Record<string, unknown> = {}) => {
    await api(`/devices/${id}/command`, { method: "POST", json: { capability, params } });
  }, [id]);

  if (loading) return <Spinner />;
  if (error || !device) return <p className="text-red-600">{error ?? "기기를 찾을 수 없습니다."}</p>;

  const data: Telemetry = { ...(device.latest ?? {}), ...live };

  const rename = async () => {
    const name = prompt("기기 이름", device.name);
    if (!name) return;
    await api(`/devices/${id}`, { method: "PATCH", json: { name } });
    setData({ ...device, name });
  };
  const unlink = async () => {
    if (!confirm("이 기기를 계정에서 해제할까요?")) return;
    await api(`/devices/${id}`, { method: "DELETE" });
    nav("/");
  };

  return (
    <div>
      <div className="mb-6 flex flex-wrap items-start justify-between gap-3">
        <div>
          <h1 className="flex items-center gap-2 text-2xl font-bold">
            {device.name}
            <button onClick={rename} className="text-sm font-normal text-slate-400 hover:text-slate-600">✏️</button>
          </h1>
          <p className="mt-1 flex flex-wrap items-center gap-2 text-sm text-slate-500">
            <Badge tone={device.online ? "green" : "slate"}>{device.online ? "온라인" : "오프라인"}</Badge>
            <Badge tone={connected ? "blue" : "slate"}>{connected ? "실시간 연결" : "실시간 끊김"}</Badge>
            {device.type_name} · 상태 {device.state ?? "—"} · FW {device.fw_version ?? "—"} · 수신 {fmtDate(lastAt ?? device.latest_at)}
          </p>
        </div>
        <div className="flex gap-2">
          <Link to={`/devices/${id}/history`}><Button variant="secondary">📈 히스토리</Button></Link>
          <Button variant="ghost" onClick={unlink}>해제</Button>
        </div>
      </div>
      {/* 설계서 6-2: Capability 동적 렌더링 */}
      <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
        {device.capabilities.map((cap) => {
          const Component = CAPABILITY_COMPONENTS[cap.component_name] ?? GenericCard;
          return <Component key={cap.cap_id} deviceId={device.id} cap={cap} data={data} config={device.config[cap.cap_id]} send={send} />;
        })}
      </div>
    </div>
  );
}
