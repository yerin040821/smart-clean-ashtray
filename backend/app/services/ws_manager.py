"""WebSocket 연결 관리 — Redis pub/sub으로 여러 API 워커 간 브로드캐스트.

흐름: MQTT 메시지를 받은 워커 → Redis `sca:ws:{device_id}` publish
      → 모든 워커의 listener가 수신 → 각자 자기 프로세스에 연결된 소켓에 전송
Redis 장애 시에는 같은 워커의 소켓에만 직접 전송(폴백).
"""
import asyncio
import json
import logging
from collections import defaultdict

import redis.asyncio as redis
from fastapi import WebSocket

from .redis_client import get_redis

log = logging.getLogger(__name__)
CHANNEL_PREFIX = "sca:ws:"


class ConnectionManager:
    def __init__(self) -> None:
        self._subs: dict[str, set[WebSocket]] = defaultdict(set)
        self._lock = asyncio.Lock()

    async def connect(self, ws: WebSocket, device_id: str) -> None:
        await ws.accept()
        async with self._lock:
            self._subs[device_id].add(ws)

    async def disconnect(self, ws: WebSocket, device_id: str) -> None:
        async with self._lock:
            self._subs[device_id].discard(ws)
            if not self._subs[device_id]:
                self._subs.pop(device_id, None)

    async def _deliver_local(self, device_id: str, message: dict) -> None:
        dead = []
        for ws in list(self._subs.get(device_id, ())):
            try:
                await ws.send_json(message)
            except Exception:
                dead.append(ws)
        for ws in dead:
            await self.disconnect(ws, device_id)

    async def broadcast(self, device_id: str, message: dict) -> None:
        try:
            await get_redis().publish(CHANNEL_PREFIX + device_id, json.dumps(message, default=str))
        except redis.RedisError as e:
            log.warning("Redis publish 실패 (%s) — 로컬 전송만 수행", e)
            await self._deliver_local(device_id, message)

    async def run_listener(self) -> None:
        """워커마다 1개 — Redis 채널을 구독해 로컬 소켓으로 전달. 끊기면 재연결."""
        while True:
            pubsub = None
            try:
                pubsub = get_redis().pubsub()
                await pubsub.psubscribe(CHANNEL_PREFIX + "*")
                log.info("Redis WS 채널 구독 시작")
                async for msg in pubsub.listen():
                    if msg.get("type") != "pmessage":
                        continue
                    device_id = msg["channel"][len(CHANNEL_PREFIX):]
                    if device_id in self._subs:
                        await self._deliver_local(device_id, json.loads(msg["data"]))
            except asyncio.CancelledError:
                raise
            except Exception as e:
                log.warning("Redis 구독 끊김 (%s) — 3초 후 재시도", e)
                await asyncio.sleep(3)
            finally:
                if pubsub is not None:
                    try:
                        await pubsub.aclose()
                    except Exception:
                        pass


manager = ConnectionManager()
