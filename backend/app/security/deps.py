"""인증/인가 의존성."""
import ipaddress

import jwt
from fastapi import Depends, HTTPException, Request
from sqlalchemy.ext.asyncio import AsyncConnection

from ..config import get_settings
from ..db import fetch_one, get_conn
from .tokens import ACCESS_COOKIE, decode


def client_ip(request: Request) -> str:
    """Nginx가 덮어쓰는 X-Real-IP만 신뢰 (X-Forwarded-For 첫 값은 클라이언트가 위조 가능)."""
    return request.headers.get("X-Real-IP") or (request.client.host if request.client else "")


def ip_allowed(ip: str, allowlist: str) -> bool:
    try:
        addr = ipaddress.ip_address(ip)
    except ValueError:
        return False
    for entry in allowlist.split(","):
        entry = entry.strip()
        if entry and addr in ipaddress.ip_network(entry, strict=False):
            return True
    return False


async def current_user(request: Request, conn: AsyncConnection = Depends(get_conn)) -> dict:
    token = request.cookies.get(ACCESS_COOKIE)
    if not token:
        raise HTTPException(401, "로그인이 필요합니다.")
    try:
        data = decode(token, "access")
    except jwt.PyJWTError:
        raise HTTPException(401, "세션이 만료되었습니다.")
    user = await fetch_one(conn, "SELECT id, email, email_raw, role, token_version FROM users WHERE id = :id",
                           id=int(data["sub"]))
    if not user or user["token_version"] != data.get("ver"):
        raise HTTPException(401, "세션이 만료되었습니다.")
    return user


async def admin_user(request: Request, user: dict = Depends(current_user)) -> dict:
    # Nginx allowlist + 애플리케이션 2중 검증
    if not ip_allowed(client_ip(request), get_settings().admin_allowed_ips):
        raise HTTPException(403, "허용되지 않은 IP입니다.")
    if user["role"] != "admin":
        raise HTTPException(403, "관리자 권한이 필요합니다.")
    return user
