"""4-3. 로그인 Rate Limiting — MySQL login_attempts 테이블 기반.

규칙: (실패 횟수 limit / 관찰 window) 도달 시 마지막 실패 시점부터 lock 동안 차단.
"""
from dataclasses import dataclass
from datetime import timedelta

from fastapi import HTTPException
from sqlalchemy.ext.asyncio import AsyncConnection

from ..db import execute_autonomous, fetch_one


@dataclass(frozen=True)
class Rule:
    name: str
    where: str
    limit: int
    window: timedelta
    lock: timedelta


RULES = (
    Rule("email+ip", "email = :email AND ip = :ip", 10, timedelta(minutes=15), timedelta(minutes=15)),
    Rule("email", "email = :email", 50, timedelta(hours=24), timedelta(hours=24)),
    Rule("ip", "ip = :ip", 30, timedelta(minutes=15), timedelta(minutes=30)),
)


async def check_rate_limit(conn: AsyncConnection, email: str, ip: str) -> None:
    for r in RULES:
        last = await fetch_one(
            conn,
            f"SELECT MAX(attempted_at) AS t FROM login_attempts WHERE {r.where} AND success = FALSE",
            email=email, ip=ip,
        )
        if not last or not last["t"]:
            continue
        row = await fetch_one(
            conn,
            f"""SELECT COUNT(*) AS n,
                       TIMESTAMPDIFF(SECOND, NOW(), :last + INTERVAL :lock SECOND) AS remain
                FROM login_attempts
                WHERE {r.where} AND success = FALSE
                  AND attempted_at > :last - INTERVAL :window SECOND""",
            email=email, ip=ip, last=last["t"],
            lock=int(r.lock.total_seconds()), window=int(r.window.total_seconds()),
        )
        if row["n"] >= r.limit and row["remain"] > 0:
            raise HTTPException(
                status_code=429,
                detail=f"로그인 시도가 너무 많습니다. {row['remain'] // 60 + 1}분 후 다시 시도하세요.",
                headers={"Retry-After": str(row["remain"])},
            )


async def record_attempt(email: str, ip: str, success: bool) -> None:
    # 실패 시 요청 트랜잭션은 401로 롤백되므로 별도 트랜잭션에 기록
    await execute_autonomous("INSERT INTO login_attempts (email, ip, success) VALUES (:e, :ip, :s)",
                             e=email, ip=ip, s=success)
