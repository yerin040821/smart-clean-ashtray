import { useState } from "react";
import { api } from "../api/client";
import { Alert, Button, Card, Input } from "../components/ui";
import { useAuth } from "../hooks/useAuth";

export function ProfilePage() {
  const { user } = useAuth();
  const [cur, setCur] = useState("");
  const [pw, setPw] = useState("");
  const [pw2, setPw2] = useState("");
  const [msg, setMsg] = useState<{ ok: boolean; text: string } | null>(null);

  const change = async () => {
    if (pw !== pw2) return setMsg({ ok: false, text: "새 비밀번호가 일치하지 않습니다." });
    try {
      await api("/auth/password-change", { method: "POST", json: { current_password: cur, new_password: pw } });
      setMsg({ ok: true, text: "비밀번호가 변경되었습니다. 다른 기기의 로그인은 해제됩니다." });
      setCur(""); setPw(""); setPw2("");
    } catch (e) {
      setMsg({ ok: false, text: (e as Error).message });
    }
  };

  return (
    <div className="mx-auto max-w-lg space-y-4">
      <h1 className="text-2xl font-bold">계정 설정</h1>
      <Card title="내 정보">
        <dl className="space-y-2 text-sm">
          <div className="flex justify-between"><dt className="text-slate-500">이메일</dt><dd>{user?.email}</dd></div>
          <div className="flex justify-between"><dt className="text-slate-500">권한</dt><dd>{user?.role === "admin" ? "관리자" : "사용자"}</dd></div>
        </dl>
      </Card>
      <Card title="비밀번호 변경">
        <div className="space-y-3">
          <Input label="현재 비밀번호" type="password" value={cur} onChange={(e) => setCur(e.target.value)} />
          <Input label="새 비밀번호" type="password" autoComplete="new-password" value={pw} onChange={(e) => setPw(e.target.value)} />
          <Input label="새 비밀번호 확인" type="password" autoComplete="new-password" value={pw2} onChange={(e) => setPw2(e.target.value)} />
          <Alert tone={msg?.ok ? "green" : "red"}>{msg?.text}</Alert>
          <Button onClick={change} disabled={!cur || !pw}>변경</Button>
        </div>
      </Card>
    </div>
  );
}
