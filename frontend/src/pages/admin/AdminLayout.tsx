import { IS_ADMIN_HOST } from "../../hostMode";
import { NavLink, Outlet } from "react-router-dom";
import { cx } from "../../components/ui";
import { useAuth } from "../../hooks/useAuth";

const NAV = [
  ["/admin", "대시보드", "📊"],
  ["/admin/devices", "기기", "📟"],
  ["/admin/device-types", "디바이스 타입", "🧩"],
  ["/admin/capabilities", "Capability", "⚙️"],
  ["/admin/users", "회원", "👥"],
  ["/admin/inquiry", "문의", "💬"],
  ["/admin/notifications", "알림 규칙", "🔔"],
  ["/admin/firmware", "펌웨어", "📦"],
  ["/admin/logs", "로그", "📜"],
] as const;

export function AdminLayout() {
  const { user, logout } = useAuth();
  return (
    <div className="flex min-h-screen">
      <aside className="w-56 shrink-0 bg-slate-900 text-slate-300">
        <p className="px-5 py-4 font-bold text-white">🚭 SCA Admin</p>
        <nav className="space-y-0.5 px-2 text-sm">
          {NAV.map(([to, label, icon]) => (
            <NavLink key={to} to={to} end={to === "/admin"}
              className={({ isActive }) => cx("flex items-center gap-2 rounded-lg px-3 py-2", isActive ? "bg-slate-700 text-white" : "hover:bg-slate-800")}>
              <span>{icon}</span>{label}
            </NavLink>
          ))}
        </nav>
        <div className="mt-6 border-t border-slate-700 px-5 py-4 text-xs">
          <p className="truncate">{user?.email} <span className="text-slate-500">(관리자)</span></p>
          {!IS_ADMIN_HOST && <NavLink to="/" className="mt-2 block text-slate-400 hover:text-white">← 사용자 화면</NavLink>}
          <button onClick={logout} className="mt-2 text-slate-400 hover:text-white">로그아웃</button>
        </div>
      </aside>
      <main className="min-w-0 flex-1 p-6"><Outlet /></main>
    </div>
  );
}
