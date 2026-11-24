import { useState } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import { api } from "../api/client";
import type { Inquiry, TipTapDoc } from "../api/types";
import { RichEditor } from "../components/RichEditor";
import { Alert, Badge, Button, Card, fmtDate, Input, Spinner } from "../components/ui";
import { useFetch } from "../hooks/useFetch";

export const STATUS: Record<Inquiry["status"], [string, "amber" | "green" | "slate"]> = {
  open: ["접수", "amber"], answered: ["답변 완료", "green"], closed: ["종료", "slate"],
};

export function InquiryListPage() {
  const { data, loading } = useFetch<Inquiry[]>("/inquiry");
  if (loading) return <Spinner />;
  return (
    <div className="mx-auto max-w-3xl">
      <div className="mb-6 flex items-center justify-between">
        <h1 className="text-2xl font-bold">문의</h1>
        <Link to="/inquiry/new"><Button>+ 문의 작성</Button></Link>
      </div>
      <Card className="p-0">
        {!data?.length && <p className="p-10 text-center text-slate-500">작성한 문의가 없습니다.</p>}
        <ul className="divide-y divide-slate-100">
          {data?.map((q) => (
            <li key={q.id}>
              <Link to={`/inquiry/${q.id}`} className="flex items-center justify-between px-5 py-4 hover:bg-slate-50">
                <span className="font-medium">{q.title}</span>
                <span className="flex items-center gap-3 text-xs text-slate-400">
                  {fmtDate(q.created_at)} <Badge tone={STATUS[q.status][1]}>{STATUS[q.status][0]}</Badge>
                </span>
              </Link>
            </li>
          ))}
        </ul>
      </Card>
    </div>
  );
}

export function InquiryNewPage() {
  const nav = useNavigate();
  const [title, setTitle] = useState("");
  const [content, setContent] = useState<TipTapDoc>({ type: "doc" });
  const [error, setError] = useState<string | null>(null);
  const submit = async () => {
    try {
      const r = await api<{ id: number }>("/inquiry", { method: "POST", json: { title, content } });
      nav(`/inquiry/${r.id}`);
    } catch (e) {
      setError((e as Error).message);
    }
  };
  return (
    <div className="mx-auto max-w-3xl space-y-4">
      <h1 className="text-2xl font-bold">문의 작성</h1>
      <Card>
        <div className="space-y-4">
          <Input label="제목" maxLength={200} value={title} onChange={(e) => setTitle(e.target.value)} />
          <RichEditor value={content} onChange={setContent} placeholder="문의 내용을 자세히 적어주세요." />
          <Alert>{error}</Alert>
          <div className="flex justify-end gap-2">
            <Button variant="secondary" onClick={() => nav(-1)}>취소</Button>
            <Button disabled={!title.trim() || !content.content?.length} onClick={submit}>등록</Button>
          </div>
        </div>
      </Card>
    </div>
  );
}

export function InquiryDetailView({ q }: { q: Inquiry }) {
  return (
    <div className="space-y-4">
      <Card title={q.title} right={<Badge tone={STATUS[q.status][1]}>{STATUS[q.status][0]}</Badge>}>
        <p className="mb-3 text-xs text-slate-400">{q.user_email} · {fmtDate(q.created_at)}</p>
        <RichEditor value={q.content} readOnly />
      </Card>
      {q.replies?.map((r) => (
        <Card key={r.id} title="💬 답변" className="border-brand-100 bg-brand-50/40">
          <p className="mb-2 text-xs text-slate-400">{fmtDate(r.created_at)}</p>
          <RichEditor value={r.content} readOnly />
        </Card>
      ))}
    </div>
  );
}

export function InquiryDetailPage() {
  const { id } = useParams();
  const { data, loading, error } = useFetch<Inquiry>(`/inquiry/${id}`);
  if (loading) return <Spinner />;
  if (error || !data) return <p className="text-red-600">{error}</p>;
  return (
    <div className="mx-auto max-w-3xl">
      <Link to="/inquiry" className="mb-4 inline-block text-sm text-slate-500">← 목록</Link>
      <InquiryDetailView q={data} />
    </div>
  );
}
