"""4-1. JWT(HttpOnly 쿠키) + CSRF Double Submit 토큰 발급."""
import secrets
from datetime import UTC, datetime, timedelta

import jwt
from fastapi import Response

from ..config import get_settings

ALG = "HS256"
ACCESS_COOKIE = "jwt"
REFRESH_COOKIE = "jwt_refresh"
CSRF_COOKIE = "csrf_token"


def _encode(payload: dict, ttl: timedelta) -> str:
    now = datetime.now(UTC)
    payload = {**payload, "iat": now, "exp": now + ttl}
    return jwt.encode(payload, get_settings().secret_key, algorithm=ALG)


def decode(token: str, typ: str) -> dict:
    data = jwt.decode(token, get_settings().secret_key, algorithms=[ALG])
    if data.get("typ") != typ:
        raise jwt.InvalidTokenError("wrong token type")
    return data


def make_access(user_id: int, role: str, ver: int) -> str:
    s = get_settings()
    return _encode({"sub": str(user_id), "role": role, "ver": ver, "typ": "access"},
                   timedelta(minutes=s.access_token_minutes))


def make_refresh(user_id: int, ver: int) -> str:
    return _encode({"sub": str(user_id), "ver": ver, "typ": "refresh"},
                   timedelta(days=get_settings().refresh_token_days))


def make_pairing(user_id: int) -> str:
    """AP 모드 기기 등록용 계정 연동 토큰 (10분)."""
    return _encode({"sub": str(user_id), "typ": "pairing"}, timedelta(minutes=10))


def set_auth_cookies(resp: Response, user_id: int, role: str, ver: int) -> None:
    s = get_settings()
    common = dict(secure=s.cookie_secure, samesite="strict", domain=s.cookie_domain or None)
    resp.set_cookie(ACCESS_COOKIE, make_access(user_id, role, ver), httponly=True,
                    max_age=s.access_token_minutes * 60, path="/", **common)
    resp.set_cookie(REFRESH_COOKIE, make_refresh(user_id, ver), httponly=True,
                    max_age=s.refresh_token_days * 86400, path="/auth", **common)
    # CSRF 토큰은 JS가 읽어 X-CSRF-Token 헤더에 담아야 하므로 httponly=False
    resp.set_cookie(CSRF_COOKIE, secrets.token_urlsafe(32), httponly=False,
                    max_age=s.refresh_token_days * 86400, path="/", **common)


def clear_auth_cookies(resp: Response) -> None:
    s = get_settings()
    domain = s.cookie_domain or None
    resp.delete_cookie(ACCESS_COOKIE, path="/", domain=domain)
    resp.delete_cookie(REFRESH_COOKIE, path="/auth", domain=domain)
    resp.delete_cookie(CSRF_COOKIE, path="/", domain=domain)
