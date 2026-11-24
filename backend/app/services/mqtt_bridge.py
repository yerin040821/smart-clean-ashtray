"""MQTT ↔ FastAPI 브리지.

구독: ashtray/+/telemetry | event | status  (+ SCA_Y 레거시 ashtray/v1/telemetry)
처리: DB 저장 → WebSocket 브로드캐스트 → 알림 규칙 평가(FCM)
발행: ashtray/{device_id}/command
"""
import asyncio
import json
import logging
import uuid
from datetime import datetime

import aiomqtt

from ..config import get_settings
from ..db import engine, execute, fetch_one
from . import notifier
from .ws_manager import manager

log = logging.getLogger(__name__)
LEGACY_TOPIC = "ashtray/v1/telemetry"
SHARE_GROUP = "$share/sca-api"

_client: aiomqtt.Client | None = None


def parse_topic(topic: str, payload: dict) -> tuple[str, str] | None:
    """(device_id, kind) 반환. 레거시 토픽은 payload의 device_id로 라우팅."""
    parts = topic.split("/")
    if topic == LEGACY_TOPIC:
        dev = payload.get("device_id")
        return (dev, "telemetry") if dev else None
    if len(parts) == 3 and parts[0] == "ashtray" and parts[2] in ("telemetry", "event", "status"):
        return parts[1], parts[2]
    return None


async def handle(topic: str, raw: bytes) -> None:
    try:
        payload = json.loads(raw) if raw else {}
    except json.JSONDecodeError:
        # status 토픽은 "online"/"offline" 평문일 수 있음
        payload = {"status": raw.decode(errors="replace")}
    if not isinstance(payload, dict):
        payload = {"value": payload}
    parsed = parse_topic(topic, payload)
    if not parsed:
        return
    device_id, kind = parsed

    async with engine().begin() as conn:
        device = await fetch_one(conn, """SELECT d.*, t.capabilities FROM devices d
                                          JOIN device_types t ON t.type_id = d.type_id WHERE d.id = :id""",
                                 id=device_id)
        if not device:
            log.debug("미등록 기기 메시지 무시: %s", device_id)
            return
        now = datetime.now()
        if kind == "telemetry":
            await execute(conn, "INSERT INTO telemetry (device_id, recorded_at, payload) VALUES (:d, :t, :p)",
                          d=device_id, t=now, p=json.dumps(payload))
            await execute(conn, """UPDATE devices SET last_seen = :t, online = TRUE,
                                   state = COALESCE(:s, state) WHERE id = :d""",
                          t=now, s=payload.get("state"), d=device_id)
            caps = device["capabilities"]
            caps = json.loads(caps) if isinstance(caps, str) else caps
            await notifier.evaluate(conn, device, caps, payload)
        elif kind == "event":
            await execute(conn, "INSERT INTO device_events (device_id, topic, payload) VALUES (:d, :tp, :p)",
                          d=device_id, tp=topic, p=json.dumps(payload))
            if "state" in payload or "to" in payload:
                await execute(conn, "UPDATE devices SET state = :s, last_seen = :t WHERE id = :d",
                              s=payload.get("to") or payload.get("state"), t=now, d=device_id)
        elif kind == "status":
            online = str(payload.get("status", payload.get("online", ""))).lower() in ("online", "true", "1")
            await execute(conn, """UPDATE devices SET online = :o, last_seen = :t,
                                   local_ip = COALESCE(:ip, local_ip), fw_version = COALESCE(:fw, fw_version)
                                   WHERE id = :d""",
                          o=online, t=now, ip=payload.get("ip"), fw=payload.get("fw"), d=device_id)

    await manager.broadcast(device_id, {"type": kind, "device_id": device_id,
                                        "ts": now.isoformat(), "data": payload})


async def run_forever() -> None:
    global _client
    s = get_settings()
    while True:
        try:
            async with aiomqtt.Client(s.mqtt_host, s.mqtt_port, identifier=f"sca-api-{uuid.uuid4().hex[:6]}") as c:
                _client = c
                # 공유 구독: 워커가 여러 개여도 메시지 1건은 워커 1개만 받음 → DB 중복 저장 방지
                for t in ("ashtray/+/telemetry", "ashtray/+/event", "ashtray/+/status", LEGACY_TOPIC):
                    await c.subscribe(f"{SHARE_GROUP}/{t}", qos=1)
                log.info("MQTT 연결됨 %s:%s", s.mqtt_host, s.mqtt_port)
                async for msg in c.messages:
                    try:
                        await handle(str(msg.topic), msg.payload)
                    except Exception:
                        log.exception("MQTT 메시지 처리 실패: %s", msg.topic)
        except aiomqtt.MqttError as e:
            _client = None
            log.warning("MQTT 연결 끊김 (%s) — 5초 후 재시도", e)
            await asyncio.sleep(5)


async def publish_command(device_id: str, action: str, params: dict | None = None) -> str:
    if _client is None:
        raise RuntimeError("MQTT 브로커에 연결되어 있지 않습니다.")
    req_id = uuid.uuid4().hex[:12]
    body = {"req_id": req_id, "action": action, "params": params or {}, "ts": int(datetime.now().timestamp())}
    await _client.publish(f"ashtray/{device_id}/command", json.dumps(body), qos=1)
    return req_id
