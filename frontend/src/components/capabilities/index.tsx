import { useState, type FC } from "react";
import type { CapabilityDef, Telemetry } from "../../api/types";
import { Badge, Button, Card, Gauge } from "../ui";

export interface CapProps {
  deviceId: string;
  cap: CapabilityDef;
  data: Telemetry;
  config?: Record<string, unknown>;
  send: (capability: string, params?: Record<string, unknown>) => Promise<void>;
}

const num = (v: unknown) => (typeof v === "number" ? v : typeof v === "string" && v !== "" && !isNaN(+v) ? +v : null);
const truthy = (v: unknown) => v === true || v === 1 || v === "1" || v === "true";

function useAction(send: CapProps["send"], cap: string) {
  const [busy, setBusy] = useState(false);
  const [msg, setMsg] = useState<string | null>(null);
  const run = async (params?: Record<string, unknown>) => {
    setBusy(true);
    setMsg(null);
    try {
      await send(cap, params);
      setMsg("명령 전송됨");
    } catch (e) {
      setMsg((e as Error).message);
    } finally {
      setBusy(false);
      setTimeout(() => setMsg(null), 3000);
    }
  };
  return { busy, msg, run };
}

const Msg = ({ msg }: { msg: string | null }) => (msg ? <p className="mt-2 text-xs text-slate-500">{msg}</p> : null);

// ───────────── 모니터링 ─────────────

const AirQualityCard: FC<CapProps> = ({ cap, data }) => {
  // MQ-135 적응형 기준선 대비 변화량 (펌웨어 SCA_Y/SCA_S 공통: >50 흡연, <30 해소)
  const diff = num(data.gas_diff);
  const level = diff === null ? null : diff > 50 ? ["나쁨", "red"] : diff >= 30 ? ["보통", "amber"] : ["좋음", "green"];
  return (
    <Card title={cap.display_name} icon={cap.icon}
      right={level && <Badge tone={level[1] as "red"}>{level[0]}</Badge>}>
      <Gauge value={diff} max={200} unit="변화량" warnAt={30} dangerAt={51} />
      {data.gas_raw !== undefined && <p className="mt-1 text-xs text-slate-400">원시값 {String(data.gas_raw)}</p>}
    </Card>
  );
};

const TempHumidityCard: FC<CapProps> = ({ cap, data }) => (
  <Card title={cap.display_name} icon={cap.icon}>
    <div className="grid grid-cols-2 gap-4">
      <div><p className="text-xs text-slate-400">온도</p><Gauge value={num(data.temperature)} max={60} unit="°C" warnAt={40} dangerAt={50} /></div>
      <div><p className="text-xs text-slate-400">습도</p><Gauge value={num(data.humidity)} unit="%" /></div>
    </div>
  </Card>
);

const FireDetectCard: FC<CapProps> = ({ cap, data }) => {
  const fire = truthy(data.fire);
  return (
    <Card title={cap.display_name} icon={cap.icon} className={fire ? "border-red-400 bg-red-50" : undefined}>
      <p className={fire ? "animate-pulse text-2xl font-bold text-red-600" : "text-2xl font-bold text-emerald-600"}>
        {fire ? "🔥 화재 감지!" : "정상"}
      </p>
      {data.state && <p className="mt-1 text-xs text-slate-500">상태: {String(data.state)}</p>}
    </Card>
  );
};

const AshLevelCard: FC<CapProps> = ({ cap, data }) => (
  <Card title={cap.display_name} icon={cap.icon}>
    <Gauge value={num(data.ash_level)} warnAt={60} dangerAt={80} />
  </Card>
);

const WaterLevelCard: FC<CapProps> = ({ cap, data }) => {
  const level = num(data.water_level);
  const ok = data.water_ok === undefined ? null : truthy(data.water_ok);
  return (
    <Card title={cap.display_name} icon={cap.icon}
      right={ok !== null && <Badge tone={ok ? "green" : "red"}>{ok ? "충분" : "부족"}</Badge>}>
      {level !== null ? <Gauge value={level} /> : (
        <p className="text-2xl font-bold">{ok === null ? "—" : ok ? "물 충분" : "보충 필요"}</p>
      )}
    </Card>
  );
};

// ───────────── 제어 ─────────────

const DurationAction = (label: string, color: "primary" | "danger" = "primary"): FC<CapProps> => ({ cap, send, config }) => {
  const rule = cap.param_schema?.duration_ms;
  const [ms, setMs] = useState<number>(Number(config?.duration_ms ?? rule?.default ?? 1000));
  const { busy, msg, run } = useAction(send, cap.cap_id);
  return (
    <Card title={cap.display_name} icon={cap.icon}>
      {rule && (
        <label className="mb-3 block text-sm text-slate-600">
          동작 시간 <b className="tabular-nums">{(ms / 1000).toFixed(1)}초</b>
          <input type="range" className="mt-1 w-full accent-emerald-600" min={rule.min} max={rule.max} step={100}
            value={ms} onChange={(e) => setMs(+e.target.value)} />
        </label>
      )}
      <Button variant={color} disabled={busy} onClick={() => run(rule ? { duration_ms: ms } : {})} className="w-full">
        {busy ? "전송 중…" : label}
      </Button>
      <Msg msg={msg} />
    </Card>
  );
};

const WaterSprayCard = DurationAction("소화 분사", "danger");
const DeodorizeCard = DurationAction("탈취제 분사");
const AutoCleanCard = DurationAction("청소 시작");
const BuzzerCard = DurationAction("부저 울리기");

const ToggleAction = (param: string, onLabel: string, offLabel: string, stateKey?: string): FC<CapProps> =>
  ({ cap, send, data, config }) => {
    const { busy, msg, run } = useAction(send, cap.cap_id);
    const current = stateKey && data[stateKey] !== undefined ? truthy(data[stateKey]) : truthy(config?.[param]);
    return (
      <Card title={cap.display_name} icon={cap.icon} right={<Badge tone={current ? "green" : "slate"}>{current ? onLabel : offLabel}</Badge>}>
        <div className="grid grid-cols-2 gap-2">
          <Button disabled={busy} onClick={() => run({ [param]: true })}>{onLabel}</Button>
          <Button variant="secondary" disabled={busy} onClick={() => run({ [param]: false })}>{offLabel}</Button>
        </div>
        <Msg msg={msg} />
      </Card>
    );
  };

const DoorControlCard = ToggleAction("open", "열기", "닫기", "door_open");
const SleepModeCard = ToggleAction("enable", "절전", "깨우기", "sleeping");

const RgbLedCard: FC<CapProps> = ({ cap, send, config, data }) => {
  const [color, setColor] = useState<string>(String(config?.color ?? data.led_color ?? "#00ff88"));
  const [brightness, setBrightness] = useState<number>(Number(config?.brightness ?? 128));
  const { busy, msg, run } = useAction(send, cap.cap_id);
  return (
    <Card title={cap.display_name} icon={cap.icon}>
      <div className="flex items-center gap-3">
        <input type="color" value={color} onChange={(e) => setColor(e.target.value)} className="h-10 w-14 cursor-pointer rounded" />
        <input type="range" min={0} max={255} value={brightness} onChange={(e) => setBrightness(+e.target.value)} className="flex-1 accent-emerald-600" />
        <span className="w-10 text-right text-sm tabular-nums">{brightness}</span>
      </div>
      <Button className="mt-3 w-full" disabled={busy} onClick={() => run({ color, brightness })}>적용</Button>
      <Msg msg={msg} />
    </Card>
  );
};

/** 범용 카드 — 관리자가 새 capability를 추가했지만 전용 컴포넌트가 없을 때 */
export const GenericCard: FC<CapProps> = ({ cap, data, send }) => {
  const { busy, msg, run } = useAction(send, cap.cap_id);
  return (
    <Card title={cap.display_name} icon={cap.icon}>
      <dl className="space-y-1 text-sm">
        {(cap.telemetry_keys ?? []).map((k) => (
          <div key={k} className="flex justify-between"><dt className="text-slate-500">{k}</dt><dd className="font-medium">{String(data[k] ?? "—")}</dd></div>
        ))}
      </dl>
      {cap.mqtt_action && <Button className="mt-3 w-full" disabled={busy} onClick={() => run()}>{cap.mqtt_action} 실행</Button>}
      <Msg msg={msg} />
    </Card>
  );
};

/** capability_definitions.component_name → 컴포넌트 (설계서 6-2) */
export const CAPABILITY_COMPONENTS: Record<string, FC<CapProps>> = {
  AirQualityCard, TempHumidityCard, FireDetectCard, WaterSprayCard, DeodorizeCard, AutoCleanCard,
  DoorControlCard, AshLevelCard, WaterLevelCard, SleepModeCard, BuzzerCard, RgbLedCard,
};
