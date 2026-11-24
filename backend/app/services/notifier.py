"""텔레메트리 → 알림 규칙 평가 → notifications 저장 + FCM 발송."""
import json
import logging
import operator

from sqlalchemy.ext.asyncio import AsyncConnection

from ..db import execute, fetch_all, fetch_one
from . import fcm
from .redis_client import acquire_once

log = logging.getLogger(__name__)

OPS = {">": operator.gt, ">=": operator.ge, "<": operator.lt, "<=": operator.le,
       "==": operator.eq, "!=": operator.ne}


def matches(value, op: str, threshold: float) -> bool:
    if op not in OPS:
        return False
    try:
        return OPS[op](float(value), float(threshold))
    except (TypeError, ValueError):
        return False


async def evaluate(conn: AsyncConnection, device: dict, capabilities: list[str], payload: dict) -> None:
    if not device.get("user_id"):
        return
    rules = await fetch_all(conn, "SELECT * FROM notification_rules WHERE is_active = TRUE")
    for rule in rules:
        if rule["capability"] and rule["capability"] not in capabilities:
            continue
        value = payload.get(rule["field"])
        if value is None or not matches(value, rule["operator"], rule["threshold"]):
            continue
        # 쿨다운: Redis 원자적 잠금(워커 간 동시 처리에도 1번만), Redis 장애 시 DB 조회로 폴백
        cooldown = int(rule["cooldown_min"] or 0)
        got = await acquire_once(f"sca:cooldown:{rule['id']}:{device['id']}", cooldown * 60)
        if got is False:
            continue
        if got is None:
            recent = await fetch_one(
                conn,
                """SELECT id FROM notifications WHERE rule_id = :r AND device_id = :d
                   AND created_at > NOW() - INTERVAL :cd MINUTE LIMIT 1""",
                r=rule["id"], d=device["id"], cd=cooldown,
            )
            if recent:
                continue
        name = device.get("name") or device["id"]
        tpl = rule["message_tpl"] or "{device}: {field} = {value}"
        body = tpl.format(device=name, field=rule["field"], value=value)
        title = "Smart Clean Ashtray"
        await execute(conn, """INSERT INTO notifications (user_id, device_id, rule_id, title, body)
                               VALUES (:u, :d, :r, :t, :b)""",
                      u=device["user_id"], d=device["id"], r=rule["id"], t=title, b=body)
        channels = rule["channels"]
        if isinstance(channels, str):
            channels = json.loads(channels)
        if "push" in (channels or []):
            try:
                await fcm.send_to_user(conn, device["user_id"], title, body,
                                       {"device_id": device["id"], "rule_id": rule["id"]})
            except Exception:
                log.exception("FCM 발송 실패")
