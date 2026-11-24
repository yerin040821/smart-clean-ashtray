// 같은 빌드를 www / admin 서브도메인에서 서빙 — 관리자 라우트는 admin 호스트(또는 로컬 개발)에서만 노출
export const IS_ADMIN_HOST = location.hostname.startsWith("admin.");
export const ADMIN_ROUTES_ENABLED = IS_ADMIN_HOST || ["localhost", "127.0.0.1"].includes(location.hostname);
