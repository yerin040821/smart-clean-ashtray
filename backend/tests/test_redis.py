"""Redis 연동 테스트 (fakeredis — 실제 Redis 불필요):  pip install fakeredis"""
import asyncio
import os
import sys

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.environ.setdefault("SECRET_KEY", "x" * 40)

fakeredis = pytest.importorskip("fakeredis")

from app.services import redis_client, ws_manager  # noqa: E402


class FakeWS:
    def __init__(self):
        self.received = []

    async def accept(self):
        pass

    async def send_json(self, m):
        self.received.append(m)


@pytest.fixture
def fake_redis(monkeypatch):
    server = fakeredis.FakeServer()
    r = fakeredis.FakeAsyncRedis(server=server, decode_responses=True)
    monkeypatch.setattr(redis_client, "_client", r)
    return r


def test_cooldown_lock(fake_redis):
    async def run():
        assert await redis_client.acquire_once("sca:cooldown:1:SCA_S", 60) is True
        assert await redis_client.acquire_once("sca:cooldown:1:SCA_S", 60) is False   # 다른 워커
        assert await redis_client.acquire_once("sca:cooldown:1:SCA_Y", 60) is True
    asyncio.run(run())


def test_broadcast_reaches_other_worker(fake_redis):
    """워커 A가 MQTT를 받아 publish → 워커 B에 붙은 소켓이 수신해야 함."""
    async def run():
        worker_a, worker_b = ws_manager.ConnectionManager(), ws_manager.ConnectionManager()
        ws_b, ws_other = FakeWS(), FakeWS()
        await worker_b.connect(ws_b, "SCA_S")
        await worker_b.connect(ws_other, "SCA_Y")
        listener = asyncio.create_task(worker_b.run_listener())
        await asyncio.sleep(0.1)
        await worker_a.broadcast("SCA_S", {"type": "telemetry", "data": {"gas_diff": 120}})
        for _ in range(50):
            if ws_b.received:
                break
            await asyncio.sleep(0.02)
        listener.cancel()
        assert ws_b.received == [{"type": "telemetry", "data": {"gas_diff": 120}}]
        assert ws_other.received == []
    asyncio.run(run())
