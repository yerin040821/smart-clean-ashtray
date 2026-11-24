/**
 * 개발용 목업 API — `npm run dev:mock` 에서만 로드됨 (운영 빌드에는 포함되지 않음).
 * 백엔드(MySQL·MQTT·Redis) 없이 화면 검토용. window.fetch / window.WebSocket 을 가로채
 * 실제 API 와 같은 경로·형식의 가짜 데이터를 돌려준다. 텔레메트리 키는 펌웨어(docs/protocol.md)와 동일.
 */
import type { CapabilityDef } from "../api/types";

const API = (import.meta.env.VITE_API_BASE as string | undefined) ?? "/api";
const now = () => new Date().toISOString();
const ago = (min: number) => new Date(Date.now() - min * 60_000).toISOString();
const doc = (text: string) => ({ type: "doc", content: [{ type: "paragraph", content: [{ type: "text", text }] }] });

// ── 시드 데이터 (backend/alembic/versions/0001_initial.py 와 동일) ──
const CAPS: CapabilityDef[] = [
  ["air_quality_monitor", "공기질", "🌫️", "AirQualityCard", null, ["gas_diff", "gas_raw"], null],
  ["temperature_humidity", "온습도", "🌡️", "TempHumidityCard", null, ["temperature", "humidity"], null],
  ["fire_detect", "화재 감지", "🔥", "FireDetectCard", null, ["fire"], null],
  ["water_spray", "소화 분사", "💧", "WaterSprayCard", "spray", ["pump_on"], { duration_ms: { type: "int", min: 100, max: 10000, default: 1000 } }],
  ["deodorize", "탈취", "🌿", "DeodorizeCard", "deodorize", null, { duration_ms: { type: "int", min: 100, max: 10000, default: 1500 } }],
  ["auto_clean", "자동 청소", "🧽", "AutoCleanCard", "clean", null, null],
  ["door_control", "덮개", "🚪", "DoorControlCard", "door", ["door_open"], { open: { type: "bool", default: true } }],
  ["ash_level", "재 잔량", "📏", "AshLevelCard", null, ["ash_level"], null],
  ["water_level", "수위", "🚰", "WaterLevelCard", null, ["water_level", "water_ok"], null],
  ["sleep_mode", "절전 모드", "😴", "SleepModeCard", "sleep", ["sleeping"], { enable: { type: "bool", default: true } }],
  ["buzzer", "부저", "🔔", "BuzzerCard", "buzzer", null, { duration_ms: { type: "int", min: 100, max: 5000, default: 500 } }],
  ["rgb_led", "LED", "💡", "RgbLedCard", "led", ["led_color"], { color: { type: "color", default: "#00ff88" }, brightness: { type: "int", min: 0, max: 255, default: 128 } }],
].map(([cap_id, display_name, icon, component_name, mqtt_action, telemetry_keys, param_schema]) =>
  ({ cap_id, display_name, icon, component_name, mqtt_action, telemetry_keys, param_schema, is_active: true }) as CapabilityDef);

const TYPES = [
  { type_id: "SCA_S", name: "한승찬 재떨이", is_builtin: true, created_at: ago(9000),
    capabilities: ["air_quality_monitor", "fire_detect", "water_spray", "deodorize", "auto_clean", "door_control", "ash_level", "water_level", "sleep_mode", "rgb_led"] },
  { type_id: "SCA_Y", name: "정예린 재떨이", is_builtin: true, created_at: ago(9000),
    capabilities: ["air_quality_monitor", "temperature_humidity", "fire_detect", "water_spray", "ash_level", "water_level", "buzzer", "rgb_led"] },
];

type Tel = Record<string, number | string | boolean>;
const TEL: Record<string, Tel> = {
  SCA_S: { device_id: "SCA_S", state: "IDLE", fire: 0, flame: 0, pump_on: 0, door_open: 0, ash_level: 42, water_level: 78, water_ok: 1,
           sleeping: 0, presence: 0, led_color: "#00ff00", gas_raw: 312, gas_diff: 6, auto_sleep: 1 },
  SCA_Y: { device_id: "SCA_Y", state: "SMOKING", temperature: 27.4, humidity: 48.2, fire: 0, pump_on: 0, ash_level: 83, water_ok: 1,
           led_color: "#ffb400", gas_raw: 402, gas_diff: 64 },
};
const DEVICES = [
  { id: "SCA_S", name: "1층 흡연부스", type_id: "SCA_S", online: true, state: "IDLE", local_ip: "192.168.0.23", fw_version: "1.0.0", registered_at: ago(8000), user: "me@example.com" },
  { id: "SCA_Y", name: "옥상 재떨이", type_id: "SCA_Y", online: true, state: "SMOKING", local_ip: "192.168.0.31", fw_version: "1.1.0", registered_at: ago(7000), user: "me@example.com" },
];

// 목업 로그인은 탭 단위로 유지 (새로고침해도 로그아웃되지 않게). 목업에서는 누구든 관리자로 로그인.
type MockUser = { id: number; email: string; role: "user" | "admin" };
const loadUser = (): MockUser | null => {
  try { return JSON.parse(sessionStorage.getItem("mock_user") ?? "null"); } catch { return null; }
};
const saveUser = (u: MockUser | null) => {
  try { u ? sessionStorage.setItem("mock_user", JSON.stringify(u)) : sessionStorage.removeItem("mock_user"); } catch { /* ignore */ }
};
let user: MockUser | null = loadUser();
const notifications = [
  { id: 3, device_id: "SCA_Y", title: "Smart Clean Ashtray", body: "옥상 재떨이 재 잔량 83% — 비워주세요", is_read: false, created_at: ago(12) },
  { id: 2, device_id: "SCA_Y", title: "Smart Clean Ashtray", body: "옥상 재떨이 공기질 나쁨 (가스 변화량 +64)", is_read: false, created_at: ago(15) },
  { id: 1, device_id: "SCA_S", title: "Smart Clean Ashtray", body: "🔥 1층 흡연부스 화재 감지! 소화 동작 중", is_read: true, created_at: ago(60 * 26) },
];
const inquiries: any[] = [
  { id: 2, user_id: 1, user_email: "me@example.com", title: "덮개가 가끔 안 닫혀요", status: "answered", created_at: ago(60 * 30),
    content: doc("사람이 떠난 뒤에도 덮개가 열려 있는 경우가 있습니다."), replies: [{ id: 1, content: doc("30초 미감지 후 닫히도록 되어 있습니다. PIR 감도를 확인해 주세요."), created_at: ago(60 * 20) }] },
  { id: 1, user_id: 1, user_email: "me@example.com", title: "LED 색상 문의", status: "open", created_at: ago(60 * 50), content: doc("대기 상태 LED 색을 바꿀 수 있나요?"), replies: [] },
];
const users = [
  { id: 1, email: "me@example.com", role: "admin", email_verified: true, created_at: ago(9000), device_count: 2 },
  { id: 2, email: "yerin@example.com", role: "user", email_verified: true, created_at: ago(8000), device_count: 0 },
  { id: 3, email: "new@example.com", role: "user", email_verified: false, created_at: ago(30), device_count: 0 },
];
let rules = [
  { id: 1, capability: "fire_detect", field: "fire", operator: "==", threshold: 1, channels: ["push"], message_tpl: "🔥 {device} 화재 감지! 소화 동작 중", cooldown_min: 1, is_active: true },
  { id: 2, capability: "ash_level", field: "ash_level", operator: ">=", threshold: 80, channels: ["push"], message_tpl: "{device} 재 잔량 {value}% — 비워주세요", cooldown_min: 60, is_active: true },
  { id: 3, capability: "water_level", field: "water_ok", operator: "==", threshold: 0, channels: ["push"], message_tpl: "{device} 소화수 부족 — 물을 보충해 주세요", cooldown_min: 60, is_active: true },
  { id: 4, capability: "air_quality_monitor", field: "gas_diff", operator: ">", threshold: 50, channels: ["push"], message_tpl: "{device} 공기질 나쁨 (가스 변화량 +{value})", cooldown_min: 30, is_active: true },
];
const firmware = [
  { version: "1.1.0", device_type: "SCA_Y", file_url: "https://api.smart-clean-ashtray.com/firmware/SCA_Y/SCA_Y-1.1.0.bin", sha256: "9f2c4e1ab37d".padEnd(64, "0"), changelog: "플랫폼 연동", is_stable: true, released_at: ago(600) },
  { version: "1.0.0", device_type: "SCA_S", file_url: "https://api.smart-clean-ashtray.com/firmware/SCA_S/SCA_S-1.0.0.bin", sha256: "1a7b0c9d55ee".padEnd(64, "0"), changelog: "최초 버전", is_stable: true, released_at: ago(900) },
];
const audit = [
  { id: 2, created_at: ago(600), actor_email: "me@example.com", action: "firmware.upload", target: "SCA_Y@1.1.0", detail: { sha256: "9f2c…" }, ip: "192.168.0.10" },
  { id: 1, created_at: ago(1200), actor_email: "me@example.com", action: "user.role", target: "1", detail: { role: "admin" }, ip: "192.168.0.10" },
];

function history(id: string, hours: number) {
  const n = Math.min(240, hours * 12);
  const step = (hours * 3600_000) / n;
  return Array.from({ length: n }, (_, i) => {
    const t = Date.now() - (n - i) * step;
    const wave = Math.sin(i / 9) * 20 + (i % 37 < 4 ? 70 : 0);
    const row: Record<string, unknown> = { t: new Date(t).toISOString(), gas_diff: Math.round(wave), ash_level: Math.min(100, 20 + Math.floor(i / 6)) };
    if (id === "SCA_Y") { row.temperature = +(25 + Math.sin(i / 20) * 2).toFixed(1); row.humidity = +(48 + Math.cos(i / 15) * 5).toFixed(1); }
    else row.water_level = Math.max(0, 100 - Math.floor(i / 8));
    return row;
  });
}

// ── 명령 → 텔레메트리 반영 (실제 펌웨어 동작 흉내) ──
function applyCommand(id: string, capability: string, params: Record<string, unknown>) {
  const t = TEL[id];
  const cap = CAPS.find((c) => c.cap_id === capability);
  if (!cap?.mqtt_action) throw [400, "제어할 수 없는 기능입니다."];
  if (capability === "door_control") t.door_open = params.open === false ? 0 : 1;
  if (capability === "sleep_mode") { t.sleeping = params.enable === false ? 0 : 1; t.state = t.sleeping ? "SLEEP" : "IDLE"; }
  if (capability === "rgb_led") t.led_color = String(params.color ?? "#00ff88");
  if (capability === "water_spray") { t.pump_on = 1; setTimeout(() => (t.pump_on = 0), Number(params.duration_ms ?? 1000)); }
  if (capability === "auto_clean") { t.state = "CLEANING"; setTimeout(() => (t.state = "IDLE"), 8000); }
  return { req_id: Math.random().toString(16).slice(2, 14), action: cap.mqtt_action, params };
}

// ── 라우터 ──
type Handler = (m: RegExpMatchArray, body: any, q: URLSearchParams) => unknown;
const routes: [string, RegExp, Handler][] = [
  ["GET", /^\/auth\/me$/, () => { if (!user) throw [401, "로그인이 필요합니다."]; return user; }],
  ["POST", /^\/auth\/login$/, (_, b) => {
    if (!b?.email || !b?.password) throw [401, "이메일 또는 비밀번호가 올바르지 않습니다."];
    user = { id: 1, email: b.email, role: "admin" };
    saveUser(user);
    return user;
  }],
  ["POST", /^\/auth\/logout$/, () => { user = null; saveUser(null); return { message: "로그아웃되었습니다." }; }],
  ["POST", /^\/auth\/refresh$/, () => { if (!user) throw [401, "세션 만료"]; return { message: "ok" }; }],
  ["POST", /^\/auth\/register$/, (_, b) => {
    if (String(b?.password ?? "").length < 8) throw [422, "비밀번호는 8자 이상이어야 합니다."];
    return { message: "인증 코드를 이메일로 보냈습니다. (목업: 아무 6자리)" };
  }],
  ["POST", /^\/auth\/verify-email$/, () => ({ message: "이메일 인증이 완료되었습니다." })],
  ["POST", /^\/auth\/password-reset$/, () => ({ message: "가입된 이메일이라면 인증 코드가 발송됩니다." })],
  ["POST", /^\/auth\/password-reset\/confirm$/, () => ({ message: "비밀번호가 변경되었습니다." })],
  ["POST", /^\/auth\/password-change$/, () => ({ message: "비밀번호가 변경되었습니다." })],

  ["GET", /^\/devices$/, () => DEVICES.map((d) => ({ ...d, type_name: TYPES.find((t) => t.type_id === d.type_id)!.name,
    capabilities: TYPES.find((t) => t.type_id === d.type_id)!.capabilities, state: TEL[d.id].state, last_seen: ago(0.1), latest: { ...TEL[d.id] } }))],
  ["POST", /^\/devices\/pairing-token$/, () => ({ pairing_token: "eyJhbGciOiJIUzI1NiJ9.MOCK-PAIRING-TOKEN." + Math.random().toString(36).slice(2), expires_in: 600 })],
  ["GET", /^\/devices\/([^/]+)$/, (m) => {
    const d = DEVICES.find((x) => x.id === m[1]); if (!d) throw [404, "기기를 찾을 수 없습니다."];
    const ty = TYPES.find((t) => t.type_id === d.type_id)!;
    return { ...d, type_name: ty.name, state: TEL[d.id].state, last_seen: ago(0.1), capabilities: ty.capabilities.map((c) => CAPS.find((x) => x.cap_id === c)),
      latest: { ...TEL[d.id] }, latest_at: now(), config: {} };
  }],
  ["PATCH", /^\/devices\/([^/]+)$/, (m, b) => { const d = DEVICES.find((x) => x.id === m[1])!; d.name = b.name; return { ok: true }; }],
  ["DELETE", /^\/devices\/([^/]+)$/, () => ({ ok: true })],
  ["GET", /^\/devices\/([^/]+)\/history$/, (m, _, q) => history(m[1], Number(q.get("hours") ?? 24))],
  ["POST", /^\/devices\/([^/]+)\/command$/, (m, b) => applyCommand(m[1], b.capability, b.params ?? {})],

  ["GET", /^\/notifications$/, () => notifications],
  ["POST", /^\/notifications\/(\d+)\/read$/, (m) => { notifications.find((n) => n.id === +m[1])!.is_read = true; return { ok: true }; }],
  ["POST", /^\/notifications\/read-all$/, () => { notifications.forEach((n) => (n.is_read = true)); return { ok: true }; }],

  ["GET", /^\/inquiry$/, () => inquiries.map(({ id, title, status, created_at }) => ({ id, title, status, created_at }))],
  ["POST", /^\/inquiry$/, (_, b) => { const id = inquiries.length + 1; inquiries.unshift({ id, user_id: 1, user_email: user?.email, title: b.title, status: "open", created_at: now(), content: b.content, replies: [] }); return { id }; }],
  ["GET", /^\/inquiry\/(\d+)$/, (m) => inquiries.find((q) => q.id === +m[1]) ?? (() => { throw [404, "문의를 찾을 수 없습니다."]; })()],

  ["GET", /^\/admin\/stats$/, () => ({ users: users.length, devices: DEVICES.length, online: 2, open_inquiries: inquiries.filter((q) => q.status === "open").length, telemetry_24h: 86_400, alerts_24h: 2 })],
  ["GET", /^\/admin\/devices$/, () => DEVICES.map((d) => ({ ...d, state: TEL[d.id].state, last_seen: ago(0.1), owner_email: d.user }))],
  ["GET", /^\/admin\/users$/, () => users],
  ["PATCH", /^\/admin\/users\/(\d+)\/role$/, (m, b) => { users.find((u) => u.id === +m[1])!.role = b.role; return { ok: true }; }],
  ["GET", /^\/admin\/inquiry$/, () => inquiries],
  ["GET", /^\/admin\/inquiry\/(\d+)$/, (m) => inquiries.find((q) => q.id === +m[1])],
  ["POST", /^\/admin\/inquiry\/(\d+)\/reply$/, (m, b) => { const q = inquiries.find((x) => x.id === +m[1]); q.replies.push({ id: Date.now(), content: b.content, created_at: now() }); q.status = "answered"; return { ok: true }; }],
  ["PATCH", /^\/admin\/inquiry\/(\d+)\/status$/, (m, b) => { inquiries.find((x) => x.id === +m[1]).status = b.status; return { ok: true }; }],
  ["GET", /^\/admin\/device-types$/, () => TYPES],
  ["POST", /^\/admin\/device-types$/, (_, b) => { TYPES.push({ ...b, is_builtin: false, created_at: now() }); return { ok: true }; }],
  ["PUT", /^\/admin\/device-types\/([^/]+)$/, (m, b) => { Object.assign(TYPES.find((t) => t.type_id === m[1])!, b); return { ok: true }; }],
  ["GET", /^\/admin\/capabilities$/, () => CAPS],
  ["POST", /^\/admin\/capabilities$/, (_, b) => { CAPS.push(b); return { ok: true }; }],
  ["PUT", /^\/admin\/capabilities\/([^/]+)$/, (m, b) => { Object.assign(CAPS.find((c) => c.cap_id === m[1])!, b); return { ok: true }; }],
  ["GET", /^\/admin\/notification-rules$/, () => rules],
  ["POST", /^\/admin\/notification-rules$/, (_, b) => { const id = Math.max(...rules.map((r) => r.id)) + 1; rules.push({ ...b, id }); return { id }; }],
  ["PUT", /^\/admin\/notification-rules\/(\d+)$/, (m, b) => { rules = rules.map((r) => (r.id === +m[1] ? { ...b, id: r.id } : r)); return { ok: true }; }],
  ["DELETE", /^\/admin\/notification-rules\/(\d+)$/, (m) => { rules = rules.filter((r) => r.id !== +m[1]); return { ok: true }; }],
  ["GET", /^\/admin\/firmware$/, () => firmware],
  ["POST", /^\/admin\/firmware$/, () => ({ version: "mock", file_url: "", sha256: "" })],
  ["POST", /^\/admin\/firmware\/([^/]+)\/deploy$/, (_, b) => ({ sent: DEVICES.filter((d) => d.type_id === b.device_type).map((d) => d.id) })],
  ["GET", /^\/admin\/logs$/, (_, __, q) => q.get("kind") === "login"
    ? [{ id: 1, attempted_at: ago(5), email: "me@example.com", ip: "192.168.0.10", success: true }, { id: 2, attempted_at: ago(40), email: "me@example.com", ip: "203.0.113.7", success: false }]
    : q.get("kind") === "mqtt" ? [{ id: 1, created_at: ago(3), device_id: "SCA_Y", topic: "ashtray/SCA_Y/event", payload: { from: "IDLE", to: "SMOKING" } }] : audit],
];

const realFetch = window.fetch.bind(window);
window.fetch = async (input: RequestInfo | URL, init?: RequestInit) => {
  const url = new URL(typeof input === "string" ? input : input instanceof URL ? input.href : input.url, location.origin);
  const base = new URL(API, location.origin);
  if (!url.href.startsWith(base.href)) return realFetch(input, init);
  const path = url.pathname.slice(base.pathname.replace(/\/$/, "").length);
  const method = (init?.method ?? "GET").toUpperCase();
  let body: unknown = undefined;
  if (typeof init?.body === "string") try { body = JSON.parse(init.body); } catch { /* ignore */ }
  await new Promise((r) => setTimeout(r, 150));   // 네트워크 지연 흉내
  for (const [m, re, h] of routes) {
    const match = path.match(re);
    if (m === method && match) {
      try {
        return new Response(JSON.stringify(h(match, body, url.searchParams) ?? {}), { status: 200, headers: { "Content-Type": "application/json" } });
      } catch (e) {
        const [status, detail] = Array.isArray(e) ? e : [500, String(e)];
        return new Response(JSON.stringify({ detail }), { status, headers: { "Content-Type": "application/json" } });
      }
    }
  }
  return new Response(JSON.stringify({ detail: `목업 미구현: ${method} ${path}` }), { status: 404 });
};

// ── WebSocket: 2초마다 텔레메트리 변동 전송 ──
class MockSocket {
  static OPEN = 1;
  readyState = 0;
  onopen: (() => void) | null = null;
  onmessage: ((e: { data: string }) => void) | null = null;
  onclose: ((e: { code: number }) => void) | null = null;
  private timer?: ReturnType<typeof setInterval>;
  constructor(url: string) {
    const id = decodeURIComponent(url.split("/ws/devices/")[1] ?? "");
    setTimeout(() => {
      this.readyState = 1;
      this.onopen?.();
      this.timer = setInterval(() => {
        const t = TEL[id]; if (!t) return;
        t.gas_raw = Math.max(0, Number(t.gas_raw) + Math.round((Math.random() - 0.5) * 12));
        t.gas_diff = Math.round(Number(t.gas_diff) + (Math.random() - 0.5) * 8);
        if (t.temperature !== undefined) t.temperature = +(Number(t.temperature) + (Math.random() - 0.5) * 0.4).toFixed(1);
        this.onmessage?.({ data: JSON.stringify({ type: "telemetry", device_id: id, ts: now(), data: { ...t } }) });
      }, 2000);
    }, 300);
  }
  send(msg: string) { if (msg === "ping") setTimeout(() => this.onmessage?.({ data: "pong" }), 10); }
  close() { clearInterval(this.timer); this.readyState = 3; this.onclose?.({ code: 1000 }); }
}
(window as unknown as { WebSocket: unknown }).WebSocket = MockSocket;

console.info("%c[MOCK] 목업 API 사용 중 — 실제 서버에 연결하지 않습니다", "color:#059669;font-weight:bold");
