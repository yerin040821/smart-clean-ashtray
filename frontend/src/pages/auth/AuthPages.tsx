import { useState, type FormEvent, type ReactNode } from "react";
import { Link, useLocation, useNavigate } from "react-router-dom";
import { api } from "../../api/client";
import { Alert, Button, Input } from "../../components/ui";
import { useAuth } from "../../hooks/useAuth";

function Shell({ title, children }: { title: string; children: ReactNode }) {
  return (
    <div className="flex min-h-screen items-center justify-center p-4">
      <div className="w-full max-w-sm rounded-2xl border border-slate-200 bg-white p-8 shadow-sm">
        <p className="mb-1 text-center text-3xl">🚭</p>
        <h1 className="mb-6 text-center text-xl font-bold">{title}</h1>
        {children}
      </div>
    </div>
  );
}

function useSubmit() {
  const [error, setError] = useState<string | null>(null);
  const [busy, setBusy] = useState(false);
  const wrap = (fn: () => Promise<void>) => async (e: FormEvent) => {
    e.preventDefault();
    setBusy(true);
    setError(null);
    try {
      await fn();
    } catch (err) {
      setError((err as Error).message);
    } finally {
      setBusy(false);
    }
  };
  return { error, busy, wrap };
}

export function LoginPage() {
  const { login } = useAuth();
  const nav = useNavigate();
  // RequireAuth 가 넘겨준 원래 경로(예: /admin)로 복귀
  const from = (useLocation().state as { from?: string } | null)?.from ?? "/";
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const { error, busy, wrap } = useSubmit();
  return (
    <Shell title="로그인">
      <form className="space-y-4" onSubmit={wrap(async () => { await login(email, password); nav(from, { replace: true }); })}>
        <Input label="이메일" type="email" autoComplete="email" required value={email} onChange={(e) => setEmail(e.target.value)} />
        <Input label="비밀번호" type="password" autoComplete="current-password" required value={password} onChange={(e) => setPassword(e.target.value)} />
        <Alert>{error}</Alert>
        <Button className="w-full" disabled={busy}>로그인</Button>
      </form>
      <div className="mt-4 flex justify-between text-sm text-slate-500">
        <Link to="/register" className="hover:text-brand-700">회원가입</Link>
        <Link to="/reset-password" className="hover:text-brand-700">비밀번호 찾기</Link>
      </div>
    </Shell>
  );
}

const POLICY_HINT = "8자 이상 · 영문/숫자/특수문자 중 2종 이상 · 연속·반복 문자(1234, aaaa) 및 이메일 포함 불가";

export function RegisterPage() {
  const nav = useNavigate();
  const [step, setStep] = useState<"form" | "verify">("form");
  const [email, setEmail] = useState("");
  const [password, setPassword] = useState("");
  const [password2, setPassword2] = useState("");
  const [code, setCode] = useState("");
  const { error, busy, wrap } = useSubmit();

  if (step === "verify") {
    return (
      <Shell title="이메일 인증">
        <form className="space-y-4" onSubmit={wrap(async () => {
          await api("/auth/verify-email", { method: "POST", json: { email, code } });
          nav("/login");
        })}>
          <p className="text-sm text-slate-600"><b>{email}</b>로 보낸 6자리 코드를 5분 안에 입력하세요.</p>
          <Input label="인증 코드" inputMode="numeric" maxLength={6} pattern="\d{6}" required value={code} onChange={(e) => setCode(e.target.value)} />
          <Alert>{error}</Alert>
          <Button className="w-full" disabled={busy}>인증하기</Button>
          <button type="button" className="w-full text-sm text-slate-500 hover:text-brand-700"
            onClick={wrap(async () => { await api("/auth/register", { method: "POST", json: { email, password } }); })}>
            코드 재발송
          </button>
        </form>
      </Shell>
    );
  }

  return (
    <Shell title="회원가입">
      <form className="space-y-4" onSubmit={wrap(async () => {
        if (password !== password2) throw new Error("비밀번호가 일치하지 않습니다.");
        await api("/auth/register", { method: "POST", json: { email, password } });
        setStep("verify");
      })}>
        <Input label="이메일" type="email" required value={email} onChange={(e) => setEmail(e.target.value)} />
        <Input label="비밀번호" type="password" autoComplete="new-password" required value={password} onChange={(e) => setPassword(e.target.value)} />
        <Input label="비밀번호 확인" type="password" autoComplete="new-password" required value={password2} onChange={(e) => setPassword2(e.target.value)} />
        <p className="text-xs text-slate-400">{POLICY_HINT}</p>
        <Alert>{error}</Alert>
        <Button className="w-full" disabled={busy}>인증 코드 받기</Button>
      </form>
      <p className="mt-4 text-center text-sm text-slate-500">이미 계정이 있나요? <Link to="/login" className="text-brand-700">로그인</Link></p>
    </Shell>
  );
}

export function ResetPasswordPage() {
  const nav = useNavigate();
  const [step, setStep] = useState<"request" | "confirm">("request");
  const [email, setEmail] = useState("");
  const [code, setCode] = useState("");
  const [pw, setPw] = useState("");
  const { error, busy, wrap } = useSubmit();
  return (
    <Shell title="비밀번호 재설정">
      {step === "request" ? (
        <form className="space-y-4" onSubmit={wrap(async () => {
          await api("/auth/password-reset", { method: "POST", json: { email } });
          setStep("confirm");
        })}>
          <Input label="가입한 이메일" type="email" required value={email} onChange={(e) => setEmail(e.target.value)} />
          <Alert>{error}</Alert>
          <Button className="w-full" disabled={busy}>인증 코드 받기</Button>
        </form>
      ) : (
        <form className="space-y-4" onSubmit={wrap(async () => {
          await api("/auth/password-reset/confirm", { method: "POST", json: { email, code, new_password: pw } });
          nav("/login");
        })}>
          <Input label="인증 코드" inputMode="numeric" maxLength={6} required value={code} onChange={(e) => setCode(e.target.value)} />
          <Input label="새 비밀번호" type="password" autoComplete="new-password" required value={pw} onChange={(e) => setPw(e.target.value)} />
          <p className="text-xs text-slate-400">{POLICY_HINT} · 최근 3개 비밀번호 재사용 불가</p>
          <Alert>{error}</Alert>
          <Button className="w-full" disabled={busy}>비밀번호 변경</Button>
        </form>
      )}
      <p className="mt-4 text-center text-sm"><Link to="/login" className="text-slate-500 hover:text-brand-700">로그인으로</Link></p>
    </Shell>
  );
}
