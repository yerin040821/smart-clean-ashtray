import json

from fastapi import Request
from sqlalchemy.ext.asyncio import AsyncConnection

from ..db import execute
from ..security.deps import client_ip


async def log_action(conn: AsyncConnection, request: Request, actor_id: int | None, action: str,
                     target: str | None = None, detail: dict | None = None) -> None:
    await execute(conn, """INSERT INTO audit_logs (actor_id, action, target, detail, ip)
                           VALUES (:a, :ac, :t, :d, :ip)""",
                  a=actor_id, ac=action, t=target, d=json.dumps(detail or {}, ensure_ascii=False),
                  ip=client_ip(request))
