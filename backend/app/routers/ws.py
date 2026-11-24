"""WebSocket — 실시간 텔레메트리 (/ws/devices/{id})."""
import jwt
from fastapi import APIRouter, WebSocket, WebSocketDisconnect

from ..config import get_settings
from ..db import engine, fetch_one
from ..security.tokens import ACCESS_COOKIE, decode
from ..services.ws_manager import manager

router = APIRouter()


@router.websocket("/ws/devices/{device_id}")
async def ws_device(websocket: WebSocket, device_id: str):
    # Cross-Site WebSocket Hijacking 방지: 브라우저 Origin 검증 (모바일 앱은 Origin 없음)
    origin = websocket.headers.get("origin")
    if origin and origin not in get_settings().cors_origin_list:
        await websocket.close(code=4403)
        return
    token = websocket.cookies.get(ACCESS_COOKIE)
    try:
        data = decode(token or "", "access")
    except jwt.PyJWTError:
        await websocket.close(code=4401)
        return
    async with engine().connect() as conn:
        user = await fetch_one(conn, "SELECT id, role, token_version FROM users WHERE id = :id", id=int(data["sub"]))
        dev = await fetch_one(conn, "SELECT user_id FROM devices WHERE id = :id", id=device_id)
    if (not user or user["token_version"] != data.get("ver") or not dev
            or dev["user_id"] != user["id"]):
        await websocket.close(code=4403)
        return

    await manager.connect(websocket, device_id)
    try:
        while True:
            msg = await websocket.receive_text()   # 클라이언트 ping 유지
            if msg == "ping":
                await websocket.send_text("pong")
    except WebSocketDisconnect:
        pass
    finally:
        await manager.disconnect(websocket, device_id)
