import { useEffect, useState } from "react";
import { api } from "../api/client";
import { Alert, Button, Card } from "../components/ui";

const STEPS = [
  ["전원 켜기", "기기를 처음 켜면 WiFi 정보가 없어 설정 모드(AP)로 시작합니다. LED가 파랗게 깜박입니다."],
  ["기기 WiFi 연결", "휴대폰 WiFi 설정에서 “AshtraySetup_XXXX” 네트워크에 연결하세요."],
  ["설정 페이지 열기", "브라우저에서 http://192.168.4.1 을 엽니다. (자동으로 열리는 경우도 있습니다)"],
  ["정보 입력", "집 WiFi 이름·비밀번호와 아래 연동 토큰을 입력하세요. ⚠️ 2.4GHz WiFi만 지원합니다."],
  ["완료", "기기가 집 WiFi로 재연결되고 자동으로 계정에 등록됩니다. 홈 화면에서 확인하세요."],
];

export function DeviceSetupPage() {
  const [token, setToken] = useState<string | null>(null);
  const [expires, setExpires] = useState(0);
  const [error, setError] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);

  const issue = async () => {
    setError(null);
    try {
      const r = await api<{ pairing_token: string; expires_in: number }>("/devices/pairing-token", { method: "POST" });
      setToken(r.pairing_token);
      setExpires(Date.now() + r.expires_in * 1000);
    } catch (e) {
      setError((e as Error).message);
    }
  };

  const [, tick] = useState(0);
  useEffect(() => {
    const t = setInterval(() => tick((n) => n + 1), 1000);
    return () => clearInterval(t);
  }, []);
  const remain = Math.max(0, Math.round((expires - Date.now()) / 1000));

  return (
    <div className="mx-auto max-w-2xl space-y-4">
      <h1 className="text-2xl font-bold">기기 등록</h1>
      <Card title="1. 연동 토큰 발급" icon="🔑">
        <p className="mb-3 text-sm text-slate-600">기기 설정 페이지에 입력할 토큰입니다. 10분간 유효합니다.</p>
        {token && remain > 0 ? (
          <div>
            <textarea readOnly value={token} className="h-24 w-full rounded-lg border bg-slate-50 p-2 font-mono text-xs" />
            <div className="mt-2 flex items-center justify-between">
              <span className="text-sm text-slate-500">남은 시간 {Math.floor(remain / 60)}:{String(remain % 60).padStart(2, "0")}</span>
              <Button variant="secondary" onClick={async () => { await navigator.clipboard.writeText(token); setCopied(true); }}>
                {copied ? "복사됨 ✓" : "복사"}
              </Button>
            </div>
          </div>
        ) : (
          <Button onClick={issue}>{token ? "토큰 재발급" : "토큰 발급"}</Button>
        )}
        <div className="mt-2"><Alert>{error}</Alert></div>
      </Card>
      <Card title="2. 기기 WiFi 설정 (AP 모드)" icon="📶">
        <ol className="space-y-3">
          {STEPS.map(([t, d], i) => (
            <li key={t} className="flex gap-3">
              <span className="flex h-7 w-7 shrink-0 items-center justify-center rounded-full bg-brand-100 text-sm font-bold text-brand-700">{i + 1}</span>
              <div><p className="font-medium">{t}</p><p className="text-sm text-slate-500">{d}</p></div>
            </li>
          ))}
        </ol>
      </Card>
    </div>
  );
}
