import { NavLink, Outlet } from "react-router-dom";
import { useAuth } from "../hooks/useAuth";
import { ADMIN_ROUTES_ENABLED } from "../hostMode";
import { cx } from "./ui";

const NAV = [
  { to: "/", label: "홈", end: true },
  { to: "/notifications", label: "알림" },
  { to: "/inquiry", label: "문의" },
  { to: "/profile", label: "계정" },
];

export function Layout() {
  const { user, logout } = useAuth();
  return (
    <div className="min-h-screen">
      <header className="sticky top-0 z-10 border-b border-slate-200 bg-white/90 backdrop-blur">
        <div className="mx-auto flex max-w-6xl items-center gap-6 px-4 py-3">
          <NavLink to="/" className="font-bold text-brand-700">🚭 Smart Clean Ashtray</NavLink>
          <nav className="flex gap-1 text-sm">
            {NAV.map((n) => (
              <NavLink key={n.to} to={n.to} end={n.end}
                className={({ isActive }) => cx("rounded-lg px-3 py-1.5", isActive ? "bg-brand-50 font-semibold text-brand-700" : "text-slate-600 hover:bg-slate-100")}>
                {n.label}
              </NavLink>
            ))}
          </nav>
          <div className="ml-auto flex items-center gap-3 text-sm text-slate-500">
            {user?.role === "admin" && ADMIN_ROUTES_ENABLED && (
              <NavLink to="/admin" className="rounded-lg bg-slate-900 px-3 py-1.5 font-medium text-white hover:bg-slate-700">
                ⚙️ 관리자
              </NavLink>
            )}
            <span className="hidden sm:inline">{user?.email}</span>
            <button onClick={logout} className="rounded-lg px-3 py-1.5 hover:bg-slate-100">로그아웃</button>
          </div>
        </div>
      </header>
      <main className="mx-auto max-w-6xl px-4 py-6">
        <Outlet />
      </main>
    </div>
  );
}
