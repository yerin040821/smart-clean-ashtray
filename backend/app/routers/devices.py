import json
import re
import secrets
from datetime import datetime, timedelta

import jwt
from fastapi import APIRouter, Depends, HTTPException, Query
from pydantic import BaseModel, Field
from sqlalchemy.ext.asyncio import AsyncConnection

from ..db import execute, fetch_all, fetch_one, get_conn
from ..security.deps import current_user
from ..security.tokens import decode, make_pairing
from ..services import mqtt_bridge

router = APIRouter(tags=["devices"])

DEVICE_ID_RE = re.compile(r"^[A-Za-z0-9_-]{2,50}$")


def _json(v):
    return json.loads(v) if isinstance(v, str) else v


async def _cap_defs(conn: AsyncConnection) -> dict[str, dict]:
    rows = await fetch_all(conn, "SELECT * FROM capability_definitions WHERE is_active = TRUE")
    return {r["cap_id"]: {**r, "telemetry_keys": _json(r["telemetry_keys"]),
                          "param_schema": _json(r["param_schema"])} for r in rows}


async def owned_device(conn: AsyncConnection, device_id: str, user: dict) -> dict:
    dev = await fetch_one(conn, """SELECT d.*, t.name AS type_name, t.capabilities FROM devices d
                                   JOIN device_types t ON t.type_id = d.type_id WHERE d.id = :id""", id=device_id)
    # 사용자 API는 소유자만 (관리자는 IP allowlist가 걸린 /admin/* 경유)
    if not dev or dev["user_id"] != user["id"]:
        raise HTTPException(404, "기기를 찾을 수 없습니다.")
    dev["capabilities"] = _json(dev["capabilities"])
    return dev


def validate_params(schema: dict | None, params: dict) -> dict:
    """capability_definitions.param_schema 기준 검증 + 기본값 채움."""
    schema = schema or {}
    out = {}
    for key, rule in schema.items():
        v = params.get(key, rule.get("default"))
        t = rule.get("type")
        try:
            if t == "int":
                v = int(v)
                if ("min" in rule and v < rule["min"]) or ("max" in rule and v > rule["max"]):
                    raise ValueError
            elif t == "bool":
                v = bool(v)
            elif t == "color":
                if not re.fullmatch(r"#[0-9a-fA-F]{6}", str(v)):
                    raise ValueError
        except (TypeError, ValueError):
            raise HTTPException(422, f"잘못된 파라미터: {key}")
        out[key] = v
    return out


# ─────────────────────────── 조회 ───────────────────────────

@router.get("/capabilities")
async def list_capabilities(_: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    return list((await _cap_defs(conn)).values())


@router.get("/devices")
async def list_devices(user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    rows = await fetch_all(conn, """SELECT d.id, d.name, d.type_id, t.name AS type_name, t.capabilities,
                                           d.online, d.state, d.last_seen, d.local_ip, d.fw_version
                                    FROM devices d JOIN device_types t ON t.type_id = d.type_id
                                    WHERE d.user_id = :u ORDER BY d.registered_at""", u=user["id"])
    for r in rows:
        r["capabilities"] = _json(r["capabilities"])
        last = await fetch_one(conn, """SELECT payload, recorded_at FROM telemetry WHERE device_id = :d
                                        ORDER BY recorded_at DESC LIMIT 1""", d=r["id"])
        r["latest"] = _json(last["payload"]) if last else None
    return rows


@router.get("/devices/{device_id}")
async def get_device(device_id: str, user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    dev = await owned_device(conn, device_id, user)
    defs = await _cap_defs(conn)
    last = await fetch_one(conn, """SELECT payload, recorded_at FROM telemetry WHERE device_id = :d
                                    ORDER BY recorded_at DESC LIMIT 1""", d=device_id)
    configs = await fetch_all(conn, "SELECT capability, config FROM device_capability_config WHERE device_id = :d",
                              d=device_id)
    return {
        **{k: dev[k] for k in ("id", "name", "type_id", "type_name", "online", "state", "last_seen",
                               "local_ip", "local_key", "fw_version")},
        "capabilities": [defs[c] for c in dev["capabilities"] if c in defs],
        "latest": _json(last["payload"]) if last else None,
        "latest_at": last["recorded_at"] if last else None,
        "config": {c["capability"]: _json(c["config"]) for c in configs},
    }


@router.get("/devices/{device_id}/history")
async def history(device_id: str, hours: int = Query(24, ge=1, le=24 * 366),
                  limit: int = Query(2000, ge=1, le=10000),
                  user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    await owned_device(conn, device_id, user)
    since = datetime.now() - timedelta(hours=hours)
    rows = await fetch_all(conn, """SELECT recorded_at, payload FROM telemetry
                                    WHERE device_id = :d AND recorded_at >= :s
                                    ORDER BY recorded_at DESC LIMIT :l""", d=device_id, s=since, l=limit)
    rows.reverse()
    return [{"t": r["recorded_at"], **_json(r["payload"])} for r in rows]


# ─────────────────────────── 등록 (AP 모드) ───────────────────────────

@router.post("/devices/pairing-token")
async def pairing_token(user: dict = Depends(current_user)):
    """앱/웹에서 발급 → AP 설정 페이지(192.168.4.1)에 입력 → ESP32가 POST /devices 로 제출."""
    return {"pairing_token": make_pairing(user["id"]), "expires_in": 600}


class DeviceRegisterIn(BaseModel):
    device_id: str
    type_id: str
    pairing_token: str
    name: str | None = Field(None, max_length=100)
    local_ip: str | None = None
    fw_version: str | None = None


@router.post("/devices", status_code=201)
async def register_device(body: DeviceRegisterIn, conn: AsyncConnection = Depends(get_conn)):
    """ESP32가 집 WiFi 재연결 후 호출. 인증은 pairing_token(10분)으로만."""
    try:
        user_id = int(decode(body.pairing_token, "pairing")["sub"])
    except jwt.PyJWTError:
        raise HTTPException(401, "연동 토큰이 만료되었거나 유효하지 않습니다.")
    if not DEVICE_ID_RE.match(body.device_id):
        raise HTTPException(422, "잘못된 device_id")
    if not await fetch_one(conn, "SELECT type_id FROM device_types WHERE type_id = :t", t=body.type_id):
        raise HTTPException(422, "알 수 없는 디바이스 타입")
    existing = await fetch_one(conn, "SELECT user_id FROM devices WHERE id = :id", id=body.device_id)
    local_key = secrets.token_hex(16)   # 재등록 시 갱신 → 이전 소유자의 로컬 접근 차단
    if existing and existing["user_id"] not in (None, user_id):
        raise HTTPException(409, "다른 계정에 등록된 기기입니다. 기존 소유자가 먼저 해제해야 합니다.")
    if existing:
        await execute(conn, """UPDATE devices SET user_id = :u, type_id = :t, name = COALESCE(:n, name),
                               local_ip = :ip, fw_version = :fw, local_key = :k, last_seen = NOW() WHERE id = :id""",
                      k=local_key, u=user_id, t=body.type_id, n=body.name, ip=body.local_ip, fw=body.fw_version, id=body.device_id)
    else:
        await execute(conn, """INSERT INTO devices (id, type_id, user_id, name, local_ip, fw_version, local_key, last_seen)
                               VALUES (:id, :t, :u, :n, :ip, :fw, :k, NOW())""",
                      k=local_key, id=body.device_id, t=body.type_id, u=user_id, n=body.name or body.device_id,
                      ip=body.local_ip, fw=body.fw_version)
    return {"device_id": body.device_id, "local_key": local_key, "mqtt_host": "mqtt.smart-clean-ashtray.com", "mqtt_port": 8883}


class DeviceUpdateIn(BaseModel):
    name: str = Field(..., min_length=1, max_length=100)


@router.patch("/devices/{device_id}")
async def update_device(device_id: str, body: DeviceUpdateIn, user: dict = Depends(current_user),
                        conn: AsyncConnection = Depends(get_conn)):
    await owned_device(conn, device_id, user)
    await execute(conn, "UPDATE devices SET name = :n WHERE id = :id", n=body.name, id=device_id)
    return {"ok": True}


@router.delete("/devices/{device_id}")
async def unlink_device(device_id: str, user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    await owned_device(conn, device_id, user)
    await execute(conn, "UPDATE devices SET user_id = NULL WHERE id = :id", id=device_id)
    return {"ok": True}


# ─────────────────────────── 명령 ───────────────────────────

class CommandIn(BaseModel):
    capability: str
    params: dict = {}


@router.post("/devices/{device_id}/command", status_code=202)
async def command(device_id: str, body: CommandIn, user: dict = Depends(current_user),
                  conn: AsyncConnection = Depends(get_conn)):
    dev = await owned_device(conn, device_id, user)
    if body.capability not in dev["capabilities"]:
        raise HTTPException(400, "이 기기가 지원하지 않는 기능입니다.")
    cap = (await _cap_defs(conn)).get(body.capability)
    if not cap or not cap["mqtt_action"]:
        raise HTTPException(400, "제어할 수 없는 기능입니다.")
    params = validate_params(cap["param_schema"], body.params)
    try:
        req_id = await mqtt_bridge.publish_command(device_id, cap["mqtt_action"], params)
    except RuntimeError as e:
        raise HTTPException(503, str(e))
    await execute(conn, """INSERT INTO device_capability_config (device_id, capability, config) VALUES (:d, :c, :p)
                           ON DUPLICATE KEY UPDATE config = VALUES(config)""",
                  d=device_id, c=body.capability, p=json.dumps(params))
    return {"req_id": req_id, "action": cap["mqtt_action"], "params": params}
