"""Firebase Cloud Messaging — 사용자 보유 모든 토큰에 멀티캐스트."""
import asyncio
import logging
import os

from sqlalchemy.ext.asyncio import AsyncConnection

from ..config import get_settings
from ..db import execute, fetch_all

log = logging.getLogger(__name__)
_app = None


def _init() -> bool:
    global _app
    if _app is not None:
        return True
    path = get_settings().firebase_credentials
    if not path or not os.path.exists(path):
        return False
    import firebase_admin
    from firebase_admin import credentials
    _app = firebase_admin.initialize_app(credentials.Certificate(path))
    return True


async def send_to_user(conn: AsyncConnection, user_id: int, title: str, body: str, data: dict | None = None) -> None:
    rows = await fetch_all(conn, "SELECT token FROM fcm_tokens WHERE user_id = :u", u=user_id)
    if not rows:
        return
    if not _init():
        log.info("FCM 미설정 — 푸시 생략: user=%s %s", user_id, title)
        return
    from firebase_admin import messaging
    tokens = [r["token"] for r in rows]
    msg = messaging.MulticastMessage(
        notification=messaging.Notification(title=title, body=body),
        data={k: str(v) for k, v in (data or {}).items()},
        tokens=tokens,
        android=messaging.AndroidConfig(priority="high"),
    )
    resp = await asyncio.to_thread(messaging.send_each_for_multicast, msg)
    # 만료/삭제된 토큰 정리
    for token, r in zip(tokens, resp.responses):
        if not r.success and r.exception and getattr(r.exception, "code", "") in (
            "NOT_FOUND", "UNREGISTERED", "INVALID_ARGUMENT"):
            await execute(conn, "DELETE FROM fcm_tokens WHERE token = :t", t=token)
