"""MySQL 8 비동기 연결.

설계안의 `asyncpg`는 PostgreSQL 전용 드라이버라 MySQL에 쓸 수 없음 → `asyncmy` 사용.
"""
from collections.abc import AsyncIterator
from typing import Any

from sqlalchemy import text
from sqlalchemy.ext.asyncio import AsyncConnection, AsyncEngine, create_async_engine

from .config import get_settings

_engine: AsyncEngine | None = None


def engine() -> AsyncEngine:
    global _engine
    if _engine is None:
        _engine = create_async_engine(get_settings().database_url, pool_pre_ping=True, pool_recycle=1800)
    return _engine


async def get_conn() -> AsyncIterator[AsyncConnection]:
    """FastAPI 의존성: 요청 단위 트랜잭션 (정상 종료 시 commit)."""
    async with engine().begin() as conn:
        yield conn


async def fetch_one(conn: AsyncConnection, sql: str, **params: Any) -> dict | None:
    row = (await conn.execute(text(sql), params)).mappings().first()
    return dict(row) if row else None


async def fetch_all(conn: AsyncConnection, sql: str, **params: Any) -> list[dict]:
    return [dict(r) for r in (await conn.execute(text(sql), params)).mappings().all()]


async def execute_autonomous(sql: str, **params: Any) -> None:
    """요청 트랜잭션과 독립적으로 즉시 커밋 (이후 HTTPException으로 롤백돼도 남아야 하는 기록용)."""
    async with engine().begin() as conn:
        await conn.execute(text(sql), params)


async def execute(conn: AsyncConnection, sql: str, **params: Any) -> int:
    """INSERT 시 lastrowid, 그 외 rowcount 반환."""
    res = await conn.execute(text(sql), params)
    return res.lastrowid if sql.lstrip().upper().startswith("INSERT") else res.rowcount
