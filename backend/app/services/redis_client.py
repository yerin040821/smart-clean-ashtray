"""Redis 연결 (워커 간 공유 상태).

- WebSocket 브로드캐스트 pub/sub  (ws_manager)
- 알림 쿨다운 잠금                 (notifier)
"""
import logging

import redis.asyncio as redis

from ..config import get_settings

log = logging.getLogger(__name__)
_client: redis.Redis | None = None


def get_redis() -> redis.Redis:
    global _client
    if _client is None:
        _client = redis.from_url(get_settings().redis_url, decode_responses=True,
                                 health_check_interval=30, socket_connect_timeout=3)
    return _client


async def close_redis() -> None:
    global _client
    if _client is not None:
        await _client.aclose()
        _client = None


async def acquire_once(key: str, ttl_seconds: int) -> bool | None:
    """SET NX EX — 처음 획득하면 True, 이미 있으면 False, Redis 장애면 None (호출측 폴백)."""
    if ttl_seconds <= 0:
        return True
    try:
        return bool(await get_redis().set(key, "1", nx=True, ex=ttl_seconds))
    except redis.RedisError as e:
        log.warning("Redis 사용 불가 (%s) — DB 폴백", e)
        return None
