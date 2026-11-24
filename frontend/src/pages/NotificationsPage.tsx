import { api } from "../api/client";
import type { Notification } from "../api/types";
import { Button, Card, cx, fmtDate, Spinner } from "../components/ui";
import { useFetch } from "../hooks/useFetch";

export function NotificationsPage() {
  const { data, setData, loading } = useFetch<Notification[]>("/notifications");
  if (loading) return <Spinner />;

  const read = async (n: Notification) => {
    if (n.is_read) return;
    await api(`/notifications/${n.id}/read`, { method: "POST" });
    setData((d) => d?.map((x) => (x.id === n.id ? { ...x, is_read: true } : x)) ?? null);
  };
  const readAll = async () => {
    await api("/notifications/read-all", { method: "POST" });
    setData((d) => d?.map((x) => ({ ...x, is_read: true })) ?? null);
  };

  return (
    <div className="mx-auto max-w-2xl">
      <div className="mb-6 flex items-center justify-between">
        <h1 className="text-2xl font-bold">알림</h1>
        <Button variant="secondary" onClick={readAll}>모두 읽음</Button>
      </div>
      <Card className="p-0">
        {!data?.length && <p className="p-10 text-center text-slate-500">알림이 없습니다.</p>}
        <ul className="divide-y divide-slate-100">
          {data?.map((n) => (
            <li key={n.id} onClick={() => read(n)}
              className={cx("cursor-pointer px-5 py-4 hover:bg-slate-50", !n.is_read && "bg-brand-50/50")}>
              <div className="flex items-center justify-between">
                <p className={cx("text-sm", !n.is_read && "font-semibold")}>{n.body}</p>
                {!n.is_read && <span className="h-2 w-2 rounded-full bg-brand-500" />}
              </div>
              <p className="mt-1 text-xs text-slate-400">{n.device_id ?? n.title} · {fmtDate(n.created_at)}</p>
            </li>
          ))}
        </ul>
      </Card>
    </div>
  );
}
