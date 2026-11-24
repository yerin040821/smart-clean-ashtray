import type { ReactNode } from "react";
import { Navigate, Route, Routes, useLocation } from "react-router-dom";
import { Layout } from "./components/Layout";
import { Spinner } from "./components/ui";
import { useAuth } from "./hooks/useAuth";
import { LoginPage, RegisterPage, ResetPasswordPage } from "./pages/auth/AuthPages";
import { DeviceDetailPage } from "./pages/DeviceDetailPage";
import { DeviceSetupPage } from "./pages/DeviceSetupPage";
import { HistoryPage } from "./pages/HistoryPage";
import { HomePage } from "./pages/HomePage";
import { InquiryDetailPage, InquiryListPage, InquiryNewPage } from "./pages/InquiryPage";
import { NotificationsPage } from "./pages/NotificationsPage";
import { ProfilePage } from "./pages/ProfilePage";
import { AdminCapabilities, AdminDeviceTypes, AdminFirmware, AdminNotificationRules } from "./pages/admin/AdminConfigPages";
import { AdminLayout } from "./pages/admin/AdminLayout";
import {
  AdminDashboard, AdminDevices, AdminInquiryDetail, AdminInquiryList, AdminLogs, AdminUsers,
} from "./pages/admin/AdminPages";

import { ADMIN_ROUTES_ENABLED, IS_ADMIN_HOST } from "./hostMode";

function RequireAuth({ children, admin = false }: { children: ReactNode; admin?: boolean }) {
  const { user, loading } = useAuth();
  const loc = useLocation();
  if (loading) return <Spinner />;
  if (!user) return <Navigate to="/login" replace state={{ from: loc.pathname }} />;
  if (admin && user.role !== "admin") return <p className="p-10 text-center text-red-600">관리자 권한이 필요합니다.</p>;
  return <>{children}</>;
}

function PublicOnly({ children }: { children: ReactNode }) {
  const { user, loading } = useAuth();
  if (loading) return <Spinner />;
  const from = (useLocation().state as { from?: string } | null)?.from;
  return user ? <Navigate to={from ?? (IS_ADMIN_HOST ? "/admin" : "/")} replace /> : <>{children}</>;
}

export default function App() {
  return (
    <Routes>
      <Route path="/login" element={<PublicOnly><LoginPage /></PublicOnly>} />
      <Route path="/register" element={<PublicOnly><RegisterPage /></PublicOnly>} />
      <Route path="/reset-password" element={<PublicOnly><ResetPasswordPage /></PublicOnly>} />

      {ADMIN_ROUTES_ENABLED && (
        <Route path="/admin" element={<RequireAuth admin><AdminLayout /></RequireAuth>}>
          <Route index element={<AdminDashboard />} />
          <Route path="devices" element={<AdminDevices />} />
          <Route path="device-types" element={<AdminDeviceTypes />} />
          <Route path="capabilities" element={<AdminCapabilities />} />
          <Route path="users" element={<AdminUsers />} />
          <Route path="inquiry" element={<AdminInquiryList />} />
          <Route path="inquiry/:id" element={<AdminInquiryDetail />} />
          <Route path="notifications" element={<AdminNotificationRules />} />
          <Route path="firmware" element={<AdminFirmware />} />
          <Route path="logs" element={<AdminLogs />} />
        </Route>
      )}

      {IS_ADMIN_HOST ? (
        <Route path="*" element={<Navigate to="/admin" replace />} />
      ) : (
        <Route element={<RequireAuth><Layout /></RequireAuth>}>
          <Route index element={<HomePage />} />
          <Route path="devices/setup" element={<DeviceSetupPage />} />
          <Route path="devices/:id" element={<DeviceDetailPage />} />
          <Route path="devices/:id/history" element={<HistoryPage />} />
          <Route path="notifications" element={<NotificationsPage />} />
          <Route path="inquiry" element={<InquiryListPage />} />
          <Route path="inquiry/new" element={<InquiryNewPage />} />
          <Route path="inquiry/:id" element={<InquiryDetailPage />} />
          <Route path="profile" element={<ProfilePage />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Route>
      )}
    </Routes>
  );
}
