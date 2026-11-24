"""CSRF Double Submit Cookie 검증 미들웨어."""
import hmac

from starlette.middleware.base import BaseHTTPMiddleware
from starlette.requests import Request
from starlette.responses import JSONResponse

from .tokens import CSRF_COOKIE

UNSAFE = {"POST", "PUT", "DELETE", "PATCH"}

# 세션 쿠키가 아직 없는 요청(로그인 전) + 기기 자체가 호출하는 엔드포인트
EXEMPT = {
    "/auth/login",
    "/auth/register",
    "/auth/verify-email",
    "/auth/password-reset",
    "/auth/password-reset/confirm",
    "/devices",          # ESP32가 pairing_token으로 직접 등록 (쿠키 미사용)
}


class CSRFMiddleware(BaseHTTPMiddleware):
    async def dispatch(self, request: Request, call_next):
        if request.method in UNSAFE and request.url.path not in EXEMPT:
            cookie_token = request.cookies.get(CSRF_COOKIE)
            header_token = request.headers.get("X-CSRF-Token")
            if not cookie_token or not header_token or not hmac.compare_digest(cookie_token, header_token):
                return JSONResponse({"detail": "CSRF 검증 실패"}, status_code=403)
        return await call_next(request)
