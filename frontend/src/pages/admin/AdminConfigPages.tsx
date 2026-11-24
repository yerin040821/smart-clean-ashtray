import { useState, type FormEvent } from "react";
import { api } from "../../api/client";
import type { CapabilityDef } from "../../api/types";
import { Alert, Badge, Button, Card, fmtDate, Input } from "../../components/ui";
import { useFetch } from "../../hooks/useFetch";
import { PageTitle, Table } from "./Table";

type Row = Record<string, unknown>;

function useAction(reload: () => void) {
  const [err, setErr] = useState<string | null>(null);
  const run = async (fn: () => Promise<unknown>) => {
    setErr(null);
    try {
      await fn();
      reload();
      return true;
    } catch (e) {
      setErr((e as Error).message);
      return false;
    }
  };
  return { err, run };
}

// ───────────── 디바이스 타입 ─────────────

interface DeviceType { type_id: string; name: string; capabilities: string[]; is_builtin: boolean }

export function AdminDeviceTypes() {
  const { data, reload } = useFetch<DeviceType[]>("/admin/device-types");
  const { data: caps } = useFetch<CapabilityDef[]>("/admin/capabilities");
  const [edit, setEdit] = useState<DeviceType | null>(null);
  const [isNew, setIsNew] = useState(false);
  const { err, run } = useAction(reload);

  const save = async () => {
    if (!edit) return;
    const ok = await run(() => api(isNew ? "/admin/device-types" : `/admin/device-types/${edit.type_id}`,
      { method: isNew ? "POST" : "PUT", json: edit }));
    if (ok) setEdit(null);
  };
  const toggle = (cap: string) => edit && setEdit({
    ...edit, capabilities: edit.capabilities.includes(cap) ? edit.capabilities.filter((c) => c !== cap) : [...edit.capabilities, cap],
  });

  return (
    <div>
      <PageTitle right={<Button onClick={() => { setIsNew(true); setEdit({ type_id: "", name: "", capabilities: [], is_builtin: false }); }}>+ 타입 추가</Button>}>
        디바이스 타입
      </PageTitle>
      {edit && (
        <Card className="mb-6" title={isNew ? "새 타입" : `${edit.type_id} 수정`}>
          <div className="grid gap-3 sm:grid-cols-2">
            <Input label="type_id" disabled={!isNew} value={edit.type_id} onChange={(e) => setEdit({ ...edit, type_id: e.target.value })} />
            <Input label="이름" value={edit.name} onChange={(e) => setEdit({ ...edit, name: e.target.value })} />
          </div>
          <p className="mb-2 mt-4 text-sm font-medium text-slate-600">Capabilities</p>
          <div className="flex flex-wrap gap-2">
            {caps?.map((c) => (
              <label key={c.cap_id} className="flex cursor-pointer items-center gap-1.5 rounded-lg border px-3 py-1.5 text-sm has-[:checked]:border-brand-500 has-[:checked]:bg-brand-50">
                <input type="checkbox" checked={edit.capabilities.includes(c.cap_id)} onChange={() => toggle(c.cap_id)} />
                {c.icon} {c.display_name}
              </label>
            ))}
          </div>
          <div className="mt-4 flex items-center gap-2">
            <Alert>{err}</Alert>
            <div className="ml-auto flex gap-2">
              <Button variant="secondary" onClick={() => setEdit(null)}>취소</Button>
              <Button onClick={save}>저장</Button>
            </div>
          </div>
        </Card>
      )}
      <Table rows={data} cols={[
        { key: "type_id", label: "ID", render: (r) => <span className="font-mono">{r.type_id}</span> },
        { key: "name", label: "이름" },
        { key: "capabilities", label: "Capabilities", render: (r) => (
          <div className="flex flex-wrap gap-1">{r.capabilities.map((c) => <Badge key={c}>{c}</Badge>)}</div>
        ) },
        { key: "is_builtin", label: "구분", render: (r) => (r.is_builtin ? <Badge tone="blue">기본</Badge> : <Badge>커스텀</Badge>) },
        { key: "act", label: "", render: (r) => <Button variant="ghost" onClick={() => { setIsNew(false); setEdit({ ...r }); }}>수정</Button> },
      ]} />
    </div>
  );
}

// ───────────── Capability 마스터 ─────────────

const EMPTY_CAP: CapabilityDef = {
  cap_id: "", display_name: "", icon: "", component_name: "GenericCard", mqtt_action: null,
  telemetry_keys: [], param_schema: null, is_active: true,
};

export function AdminCapabilities() {
  const { data, reload } = useFetch<CapabilityDef[]>("/admin/capabilities");
  const [edit, setEdit] = useState<CapabilityDef | null>(null);
  const [schemaText, setSchemaText] = useState("");
  const [isNew, setIsNew] = useState(false);
  const { err, run } = useAction(reload);

  const open = (c: CapabilityDef, n: boolean) => {
    setIsNew(n);
    setEdit(c);
    setSchemaText(c.param_schema ? JSON.stringify(c.param_schema, null, 2) : "");
  };
  const save = async (e: FormEvent) => {
    e.preventDefault();
    if (!edit) return;
    let param_schema = null;
    try {
      param_schema = schemaText.trim() ? JSON.parse(schemaText) : null;
    } catch {
      return alert("param_schema JSON 형식 오류");
    }
    const body = { ...edit, param_schema, mqtt_action: edit.mqtt_action || null };
    const ok = await run(() => api(isNew ? "/admin/capabilities" : `/admin/capabilities/${edit.cap_id}`,
      { method: isNew ? "POST" : "PUT", json: body }));
    if (ok) setEdit(null);
  };

  return (
    <div>
      <PageTitle right={<Button onClick={() => open(EMPTY_CAP, true)}>+ Capability 추가</Button>}>Capability 마스터</PageTitle>
      {edit && (
        <Card className="mb-6" title={isNew ? "새 Capability" : `${edit.cap_id} 수정`}>
          <form onSubmit={save} className="grid gap-3 sm:grid-cols-2">
            <Input label="cap_id" disabled={!isNew} required value={edit.cap_id} onChange={(e) => setEdit({ ...edit, cap_id: e.target.value })} />
            <Input label="표시 이름" required value={edit.display_name} onChange={(e) => setEdit({ ...edit, display_name: e.target.value })} />
            <Input label="아이콘 (이모지)" value={edit.icon ?? ""} onChange={(e) => setEdit({ ...edit, icon: e.target.value })} />
            <Input label="React 컴포넌트" required value={edit.component_name} onChange={(e) => setEdit({ ...edit, component_name: e.target.value })} />
            <Input label="MQTT action (제어형만)" value={edit.mqtt_action ?? ""} onChange={(e) => setEdit({ ...edit, mqtt_action: e.target.value })} />
            <Input label="telemetry_keys (쉼표 구분)" value={(edit.telemetry_keys ?? []).join(",")}
              onChange={(e) => setEdit({ ...edit, telemetry_keys: e.target.value.split(",").map((s) => s.trim()).filter(Boolean) })} />
            <label className="sm:col-span-2">
              <span className="mb-1 block text-sm font-medium text-slate-600">param_schema (JSON)</span>
              <textarea value={schemaText} onChange={(e) => setSchemaText(e.target.value)} rows={5}
                placeholder='{"duration_ms": {"type": "int", "min": 100, "max": 10000, "default": 1000}}'
                className="w-full rounded-lg border border-slate-300 p-2 font-mono text-xs" />
            </label>
            <label className="flex items-center gap-2 text-sm">
              <input type="checkbox" checked={edit.is_active} onChange={(e) => setEdit({ ...edit, is_active: e.target.checked })} /> 활성
            </label>
            <div className="flex items-center justify-end gap-2 sm:col-span-2">
              <Alert>{err}</Alert>
              <Button type="button" variant="secondary" onClick={() => setEdit(null)}>취소</Button>
              <Button>저장</Button>
            </div>
          </form>
        </Card>
      )}
      <Table rows={data} cols={[
        { key: "cap_id", label: "ID", render: (r) => <span className="font-mono">{r.icon} {r.cap_id}</span> },
        { key: "display_name", label: "이름" },
        { key: "component_name", label: "컴포넌트" },
        { key: "mqtt_action", label: "action" },
        { key: "telemetry_keys", label: "telemetry", render: (r) => (r.telemetry_keys ?? []).join(", ") || "—" },
        { key: "is_active", label: "상태", render: (r) => <Badge tone={r.is_active ? "green" : "slate"}>{r.is_active ? "활성" : "비활성"}</Badge> },
        { key: "act", label: "", render: (r) => <Button variant="ghost" onClick={() => open(r, false)}>수정</Button> },
      ]} />
    </div>
  );
}

// ───────────── 알림 규칙 ─────────────

interface Rule {
  id?: number; capability: string | null; field: string; operator: string; threshold: number;
  channels: string[]; message_tpl: string | null; cooldown_min: number; is_active: boolean;
}
const EMPTY_RULE: Rule = { capability: null, field: "", operator: ">=", threshold: 0, channels: ["push"], message_tpl: "", cooldown_min: 10, is_active: true };

export function AdminNotificationRules() {
  const { data, reload } = useFetch<Rule[]>("/admin/notification-rules");
  const [edit, setEdit] = useState<Rule | null>(null);
  const { err, run } = useAction(reload);
  const save = async () => {
    if (!edit) return;
    const ok = await run(() => api(edit.id ? `/admin/notification-rules/${edit.id}` : "/admin/notification-rules",
      { method: edit.id ? "PUT" : "POST", json: { ...edit, capability: edit.capability || null } }));
    if (ok) setEdit(null);
  };
  const remove = (id: number) => confirm("삭제할까요?") && run(() => api(`/admin/notification-rules/${id}`, { method: "DELETE" }));

  return (
    <div>
      <PageTitle right={<Button onClick={() => setEdit({ ...EMPTY_RULE })}>+ 규칙 추가</Button>}>알림 규칙</PageTitle>
      {edit && (
        <Card className="mb-6" title={edit.id ? `규칙 #${edit.id}` : "새 규칙"}>
          <div className="grid gap-3 sm:grid-cols-4">
            <Input label="capability (선택)" value={edit.capability ?? ""} onChange={(e) => setEdit({ ...edit, capability: e.target.value })} />
            <Input label="필드" value={edit.field} onChange={(e) => setEdit({ ...edit, field: e.target.value })} />
            <label>
              <span className="mb-1 block text-sm font-medium text-slate-600">연산자</span>
              <select className="w-full rounded-lg border border-slate-300 px-3 py-2 text-sm" value={edit.operator} onChange={(e) => setEdit({ ...edit, operator: e.target.value })}>
                {[">", ">=", "<", "<=", "==", "!="].map((o) => <option key={o}>{o}</option>)}
              </select>
            </label>
            <Input label="임계값" type="number" value={edit.threshold} onChange={(e) => setEdit({ ...edit, threshold: +e.target.value })} />
            <div className="sm:col-span-3">
              <Input label="메시지 템플릿 ({device}, {field}, {value})" value={edit.message_tpl ?? ""} onChange={(e) => setEdit({ ...edit, message_tpl: e.target.value })} />
            </div>
            <Input label="쿨다운(분)" type="number" value={edit.cooldown_min} onChange={(e) => setEdit({ ...edit, cooldown_min: +e.target.value })} />
          </div>
          <div className="mt-4 flex items-center gap-3">
            <label className="flex items-center gap-2 text-sm"><input type="checkbox" checked={edit.is_active} onChange={(e) => setEdit({ ...edit, is_active: e.target.checked })} /> 활성</label>
            <Alert>{err}</Alert>
            <div className="ml-auto flex gap-2">
              <Button variant="secondary" onClick={() => setEdit(null)}>취소</Button>
              <Button onClick={save}>저장</Button>
            </div>
          </div>
        </Card>
      )}
      <Table rows={data} cols={[
        { key: "id", label: "#" },
        { key: "capability", label: "capability" },
        { key: "cond", label: "조건", render: (r) => <code>{r.field} {r.operator} {r.threshold}</code> },
        { key: "message_tpl", label: "메시지" },
        { key: "cooldown_min", label: "쿨다운", render: (r) => `${r.cooldown_min}분` },
        { key: "is_active", label: "상태", render: (r) => <Badge tone={r.is_active ? "green" : "slate"}>{r.is_active ? "활성" : "꺼짐"}</Badge> },
        { key: "act", label: "", render: (r) => (
          <div className="flex gap-1">
            <Button variant="ghost" onClick={() => setEdit({ ...r })}>수정</Button>
            <Button variant="ghost" onClick={() => remove(r.id!)}>삭제</Button>
          </div>
        ) },
      ]} />
    </div>
  );
}

// ───────────── 펌웨어 / OTA ─────────────

export function AdminFirmware() {
  const { data, reload } = useFetch<Row[]>("/admin/firmware");
  const { data: types } = useFetch<DeviceType[]>("/admin/device-types");
  const [form, setForm] = useState({ version: "", device_type: "SCA_S", changelog: "", is_stable: false });
  const [file, setFile] = useState<File | null>(null);
  const [msg, setMsg] = useState<string | null>(null);
  const { err, run } = useAction(reload);

  const upload = async (e: FormEvent) => {
    e.preventDefault();
    if (!file) return;
    const fd = new FormData();
    Object.entries(form).forEach(([k, v]) => fd.append(k, String(v)));
    fd.append("file", file);
    if (await run(() => api("/admin/firmware", { method: "POST", body: fd }))) setMsg("업로드 완료");
  };
  const deploy = (r: Row) => {
    if (!confirm(`${r.device_type} 전체 기기에 ${r.version} OTA를 배포할까요?`)) return;
    run(async () => {
      const res = await api<{ sent: string[] }>(`/admin/firmware/${r.version}/deploy`, { method: "POST", json: { device_type: r.device_type } });
      setMsg(`배포 명령 전송: ${res.sent.join(", ") || "대상 없음"}`);
    });
  };

  return (
    <div>
      <PageTitle>펌웨어 / OTA</PageTitle>
      <Card className="mb-6" title="펌웨어 업로드">
        <form onSubmit={upload} className="grid gap-3 sm:grid-cols-4">
          <Input label="버전" required placeholder="1.2.0" value={form.version} onChange={(e) => setForm({ ...form, version: e.target.value })} />
          <label>
            <span className="mb-1 block text-sm font-medium text-slate-600">타입</span>
            <select className="w-full rounded-lg border border-slate-300 px-3 py-2 text-sm" value={form.device_type} onChange={(e) => setForm({ ...form, device_type: e.target.value })}>
              {types?.map((t) => <option key={t.type_id}>{t.type_id}</option>)}
            </select>
          </label>
          <label className="sm:col-span-2">
            <span className="mb-1 block text-sm font-medium text-slate-600">.bin 파일</span>
            <input type="file" accept=".bin" required onChange={(e) => setFile(e.target.files?.[0] ?? null)} className="text-sm" />
          </label>
          <div className="sm:col-span-3">
            <Input label="변경 사항" value={form.changelog} onChange={(e) => setForm({ ...form, changelog: e.target.value })} />
          </div>
          <label className="flex items-end gap-2 pb-2 text-sm">
            <input type="checkbox" checked={form.is_stable} onChange={(e) => setForm({ ...form, is_stable: e.target.checked })} /> 안정 버전
          </label>
          <div className="flex items-center gap-2 sm:col-span-4">
            <Alert>{err}</Alert>
            <Alert tone="green">{msg}</Alert>
            <Button className="ml-auto">업로드</Button>
          </div>
        </form>
      </Card>
      <Table rows={data} cols={[
        { key: "device_type", label: "타입" },
        { key: "version", label: "버전", render: (r) => <span className="font-mono">{String(r.version)}</span> },
        { key: "is_stable", label: "", render: (r) => (r.is_stable ? <Badge tone="green">stable</Badge> : <Badge>beta</Badge>) },
        { key: "changelog", label: "변경 사항" },
        { key: "sha256", label: "SHA-256", render: (r) => <code className="text-xs">{String(r.sha256 ?? "").slice(0, 12)}…</code> },
        { key: "released_at", label: "등록일", render: (r) => fmtDate(r.released_at as string) },
        { key: "act", label: "", render: (r) => <Button variant="danger" onClick={() => deploy(r)}>OTA 배포</Button> },
      ]} />
    </div>
  );
}
