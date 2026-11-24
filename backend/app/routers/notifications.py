from typing import Literal

from fastapi import APIRouter, Depends
from pydantic import BaseModel, Field
from sqlalchemy.ext.asyncio import AsyncConnection

from ..db import execute, fetch_all, get_conn
from ..security.deps import current_user

router = APIRouter(tags=["notifications"])


@router.get("/notifications")
async def list_notifications(user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    return await fetch_all(conn, """SELECT id, device_id, title, body, is_read, created_at FROM notifications
                                    WHERE user_id = :u ORDER BY id DESC LIMIT 200""", u=user["id"])


@router.post("/notifications/{nid}/read")
async def mark_read(nid: int, user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    await execute(conn, "UPDATE notifications SET is_read = TRUE WHERE id = :id AND user_id = :u", id=nid, u=user["id"])
    return {"ok": True}


@router.post("/notifications/read-all")
async def mark_all_read(user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    await execute(conn, "UPDATE notifications SET is_read = TRUE WHERE user_id = :u", u=user["id"])
    return {"ok": True}


class FcmTokenIn(BaseModel):
    token: str = Field(..., max_length=512)
    device_os: Literal["android", "ios", "web"]


@router.post("/me/fcm-tokens")
async def save_fcm_token(body: FcmTokenIn, user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    # 같은 토큰이 다른 계정에 있으면 소유자 이전 (폰에서 계정 전환한 경우)
    await execute(conn, """INSERT INTO fcm_tokens (user_id, token, device_os) VALUES (:u, :t, :o)
                           ON DUPLICATE KEY UPDATE user_id = VALUES(user_id), device_os = VALUES(device_os)""",
                  u=user["id"], t=body.token, o=body.device_os)
    return {"ok": True}
