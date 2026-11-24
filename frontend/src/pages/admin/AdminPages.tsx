import { useState } from "react";
import { Link, useParams } from "react-router-dom";
import { api } from "../../api/client";
import type { Inquiry, TipTapDoc } from "../../api/types";
import { RichEditor } from "../../components/RichEditor";
import { Alert, Badge, Button, Card, cx, fmtDate, Spinner } from "../../components/ui";
import { useFetch } from "../../hooks/useFetch";
import { InquiryDetailView, STATUS } from "../InquiryPage";
import { PageTitle, Table } from "./Table";

type Row = Record<string, unknown>;

export function AdminDashboard() {
  const { data } = useFetch<Record<string, number>>("/admin/stats");
  const items = [
    ["users", "회원", "👥"], ["devices", "전체 기기", "📟"], ["online", "온라인", "🟢"],
    ["open_inquiries", "미답변 문의", "💬"], ["telemetry_24h", "24h 텔레메트리", "📈"], ["alerts_24h", "24h 알림", "🔔"],
  ];
  return (
    <div>
      <PageTitle>대시보드</PageTitle>
      <div className="grid gap-4 sm:grid-cols-2 lg:grid-cols-3">
        {items.map(([k, label, icon]) => (
          <Card key={k}>
            <p className="text-sm text-slate-500">{icon} {label}</p>
            <p className="mt-2 text-3xl font-bold tabular-nums">{data?.[k]?.toLocaleString() ?? "—"}</p>
          </Card>
        ))}
      </div>
    </div>
  );
}

export function AdminDevices() {
  const { data } = useFetch<Row[]>("/admin/devices");
  return (
    <div>
      <PageTitle>전체 기기</PageTitle>
      <Table rows={data} cols={[
        { key: "id", label: "ID", render: (r) => <span className="font-mono">{String(r.id)}</span> },
        { key: "name", label: "이름" },
        { key: "type_id", label: "타입" },
        { key: "owner_email", label: "소유자" },
        { key: "online", label: "상태", render: (r) => <Badge tone={r.online ? "green" : "slate"}>{r.online ? "온라인" : "오프라인"}</Badge> },
        { key: "state", label: "FSM" },
        { key: "fw_version", label: "FW" },
        { key: "local_ip", label: "로컬 IP" },
        { key: "last_seen", label: "마지막 수신", render: (r) => fmtDate(r.last_seen as string) },
      ]} />
    </div>
  );
}

export function AdminUsers() {
  const { data, reload } = useFetch<Row[]>("/admin/users");
  const [err, setErr] = useState<string | null>(null);
  const setRole = async (id: unknown, role: string) => {
    if (!confirm(`권한을 ${role}(으)로 변경할까요?`)) return;
    try {
      await api(`/admin/users/${id}/role`, { method: "PATCH", json: { role } });
      reload();
    } catch (e) {
      setErr((e as Error).message);
    }
  };
  return (
    <div>
      <PageTitle>회원</PageTitle>
      <div className="mb-3"><Alert>{err}</Alert></div>
      <Table rows={data} cols={[
        { key: "id", label: "ID" },
        { key: "email", label: "이메일" },
        { key: "email_verified", label: "인증", render: (r) => (r.email_verified ? "✅" : "—") },
        { key: "device_count", label: "기기 수" },
        { key: "created_at", label: "가입일", render: (r) => fmtDate(r.created_at as string) },
        { key: "role", label: "권한", render: (r) => (
          <select value={String(r.role)} onChange={(e) => setRole(r.id, e.target.value)} className="rounded border px-2 py-1">
            <option value="user">user</option><option value="admin">admin</option>
          </select>
        ) },
      ]} />
    </div>
  );
}

export function AdminInquiryList() {
  const { data } = useFetch<Inquiry[]>("/admin/inquiry");
  return (
    <div>
      <PageTitle>문의</PageTitle>
      <Table rows={data} cols={[
        { key: "id", label: "#" },
        { key: "title", label: "제목", render: (r) => <Link className="text-brand-700 hover:underline" to={`/admin/inquiry/${r.id}`}>{r.title}</Link> },
        { key: "user_email", label: "작성자" },
        { key: "status", label: "상태", render: (r) => <Badge tone={STATUS[r.status][1]}>{STATUS[r.status][0]}</Badge> },
        { key: "created_at", label: "작성일", render: (r) => fmtDate(r.created_at) },
      ]} />
    </div>
  );
}

export function AdminInquiryDetail() {
  const { id } = useParams();
  const { data, reload, loading } = useFetch<Inquiry>(`/admin/inquiry/${id}`);
  const [reply, setReply] = useState<TipTapDoc>({ type: "doc" });
  const [key, setKey] = useState(0);
  const [err, setErr] = useState<string | null>(null);
  if (loading || !data) return <Spinner />;
  const send = async () => {
    try {
      await api(`/admin/inquiry/${id}/reply`, { method: "POST", json: { content: reply } });
      setReply({ type: "doc" });
      setKey((k) => k + 1);
      reload();
    } catch (e) {
      setErr((e as Error).message);
    }
  };
  const setStatus = async (status: string) => {
    await api(`/admin/inquiry/${id}/status`, { method: "PATCH", json: { status } });
    reload();
  };
  return (
    <div className="max-w-3xl space-y-4">
      <div className="flex items-center justify-between">
        <Link to="/admin/inquiry" className="text-sm text-slate-500">← 목록</Link>
        <Button variant="secondary" onClick={() => setStatus(data.status === "closed" ? "open" : "closed")}>
          {data.status === "closed" ? "다시 열기" : "종료 처리"}
        </Button>
      </div>
      <InquiryDetailView q={data} />
      <Card title="답변 작성">
        <RichEditor key={key} value={reply} onChange={setReply} placeholder="답변을 입력하세요." />
        <div className="mt-3 flex items-center justify-between">
          <Alert>{err}</Alert>
          <Button className="ml-auto" disabled={!reply.content?.length} onClick={send}>답변 등록</Button>
        </div>
      </Card>
    </div>
  );
}

export function AdminLogs() {
  const [kind, setKind] = useState<"audit" | "mqtt" | "login">("audit");
  const { data } = useFetch<Row[]>(`/admin/logs?kind=${kind}`);
  const cols = {
    audit: [
      { key: "created_at", label: "시각", render: (r: Row) => fmtDate(r.created_at as string) },
      { key: "actor_email", label: "관리자" }, { key: "action", label: "작업" }, { key: "target", label: "대상" },
      { key: "detail", label: "상세", render: (r: Row) => <code className="text-xs">{JSON.stringify(r.detail)}</code> },
      { key: "ip", label: "IP" },
    ],
    mqtt: [
      { key: "created_at", label: "시각", render: (r: Row) => fmtDate(r.created_at as string) },
      { key: "device_id", label: "기기" }, { key: "topic", label: "토픽" },
      { key: "payload", label: "페이로드", render: (r: Row) => <code className="text-xs">{JSON.stringify(r.payload)}</code> },
    ],
    login: [
      { key: "attempted_at", label: "시각", render: (r: Row) => fmtDate(r.attempted_at as string) },
      { key: "email", label: "이메일" }, { key: "ip", label: "IP" },
      { key: "success", label: "결과", render: (r: Row) => <Badge tone={r.success ? "green" : "red"}>{r.success ? "성공" : "실패"}</Badge> },
    ],
  }[kind];
  return (
    <div>
      <PageTitle right={
        <div className="flex gap-1 rounded-lg bg-slate-100 p-1">
          {(["audit", "mqtt", "login"] as const).map((k) => (
            <button key={k} onClick={() => setKind(k)} className={cx("rounded-md px-3 py-1 text-sm", kind === k ? "bg-white font-semibold shadow-sm" : "text-slate-500")}>
              {{ audit: "감사 로그", mqtt: "MQTT 이벤트", login: "로그인" }[k]}
            </button>
          ))}
        </div>
      }>시스템 로그</PageTitle>
      <Table rows={data} cols={cols} />
    </div>
  );
}
