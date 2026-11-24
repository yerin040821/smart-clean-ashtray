"""관리자 API — Nginx IP allowlist + FastAPI IP 재검증 + role:admin."""
import hashlib
import json
import os
import re
from typing import Literal

from fastapi import APIRouter, Depends, File, Form, HTTPException, Request, UploadFile
from pydantic import BaseModel, Field
from sqlalchemy.ext.asyncio import AsyncConnection

from ..config import get_settings
from ..db import execute, fetch_all, fetch_one, get_conn
from ..security.deps import admin_user
from ..services import mqtt_bridge
from ..services.audit import log_action
from .inquiry import inquiry_detail, validate_tiptap_doc

router = APIRouter(prefix="/admin", tags=["admin"], dependencies=[Depends(admin_user)])

ID_RE = re.compile(r"^[A-Za-z0-9_]{2,50}$")
VERSION_RE = re.compile(r"^[0-9A-Za-z._-]{1,20}$")


def _json(v):
    return json.loads(v) if isinstance(v, str) else v


# ─────────────── 대시보드 ───────────────

@router.get("/stats")
async def stats(conn: AsyncConnection = Depends(get_conn)):
    q = lambda sql: fetch_one(conn, sql)  # noqa: E731
    return {
        "users": (await q("SELECT COUNT(*) n FROM users"))["n"],
        "devices": (await q("SELECT COUNT(*) n FROM devices"))["n"],
        "online": (await q("SELECT COUNT(*) n FROM devices WHERE online = TRUE"))["n"],
        "open_inquiries": (await q("SELECT COUNT(*) n FROM inquiries WHERE status = 'open'"))["n"],
        "telemetry_24h": (await q("SELECT COUNT(*) n FROM telemetry WHERE recorded_at > NOW() - INTERVAL 1 DAY"))["n"],
        "alerts_24h": (await q("SELECT COUNT(*) n FROM notifications WHERE created_at > NOW() - INTERVAL 1 DAY"))["n"],
    }


# ─────────────── 기기 ───────────────

@router.get("/devices")
async def devices(conn: AsyncConnection = Depends(get_conn)):
    return await fetch_all(conn, """SELECT d.id, d.type_id, d.name, d.local_ip, d.online, d.state, d.fw_version, d.last_seen,
                                           d.registered_at, u.email_raw AS owner_email FROM devices d
                                    LEFT JOIN users u ON u.id = d.user_id ORDER BY d.registered_at DESC""")


# ─────────────── 회원 ───────────────

@router.get("/users")
async def users(conn: AsyncConnection = Depends(get_conn)):
    return await fetch_all(conn, """SELECT u.id, u.email_raw AS email, u.role, u.email_verified, u.created_at,
                                           (SELECT COUNT(*) FROM devices d WHERE d.user_id = u.id) AS device_count
                                    FROM users u ORDER BY u.id DESC""")


class RoleIn(BaseModel):
    role: Literal["user", "admin"]


@router.patch("/users/{uid}/role")
async def set_role(uid: int, body: RoleIn, request: Request, admin: dict = Depends(admin_user),
                   conn: AsyncConnection = Depends(get_conn)):
    if uid == admin["id"] and body.role != "admin":
        raise HTTPException(400, "자기 자신의 관리자 권한은 해제할 수 없습니다.")
    # 권한 변경 즉시 반영되도록 기존 토큰 무효화
    n = await execute(conn, "UPDATE users SET role = :r, token_version = token_version + 1 WHERE id = :id",
                      r=body.role, id=uid)
    if not n:
        raise HTTPException(404, "회원을 찾을 수 없습니다.")
    await log_action(conn, request, admin["id"], "user.role", str(uid), {"role": body.role})
    return {"ok": True}


# ─────────────── 문의 ───────────────

@router.get("/inquiry")
async def inquiries(conn: AsyncConnection = Depends(get_conn)):
    return await fetch_all(conn, """SELECT i.id, i.title, i.status, i.created_at, u.email_raw AS user_email
                                    FROM inquiries i JOIN users u ON u.id = i.user_id ORDER BY i.id DESC""")


@router.get("/inquiry/{iid}")
async def inquiry(iid: int, conn: AsyncConnection = Depends(get_conn)):
    return await inquiry_detail(conn, iid)


class ReplyIn(BaseModel):
    content: dict


@router.post("/inquiry/{iid}/reply", status_code=201)
async def reply(iid: int, body: ReplyIn, request: Request, admin: dict = Depends(admin_user),
                conn: AsyncConnection = Depends(get_conn)):
    inq = await inquiry_detail(conn, iid)
    await execute(conn, "INSERT INTO inquiry_replies (inquiry_id, admin_id, content) VALUES (:i, :a, :c)",
                  i=iid, a=admin["id"], c=validate_tiptap_doc(body.content))
    await execute(conn, "UPDATE inquiries SET status = 'answered' WHERE id = :i", i=iid)
    await execute(conn, """INSERT INTO notifications (user_id, title, body) VALUES (:u, '문의 답변', :b)""",
                  u=inq["user_id"], b=f"'{inq['title']}' 문의에 답변이 등록되었습니다.")
    await log_action(conn, request, admin["id"], "inquiry.reply", str(iid))
    return {"ok": True}


class StatusIn(BaseModel):
    status: Literal["open", "answered", "closed"]


@router.patch("/inquiry/{iid}/status")
async def inquiry_status(iid: int, body: StatusIn, conn: AsyncConnection = Depends(get_conn)):
    await execute(conn, "UPDATE inquiries SET status = :s WHERE id = :i", s=body.status, i=iid)
    return {"ok": True}


# ─────────────── 디바이스 타입 ───────────────

class DeviceTypeIn(BaseModel):
    type_id: str
    name: str = Field(..., max_length=100)
    capabilities: list[str]


async def _check_caps(conn: AsyncConnection, caps: list[str]) -> None:
    known = {r["cap_id"] for r in await fetch_all(conn, "SELECT cap_id FROM capability_definitions")}
    unknown = set(caps) - known
    if unknown:
        raise HTTPException(422, f"알 수 없는 capability: {', '.join(sorted(unknown))}")


@router.get("/device-types")
async def device_types(conn: AsyncConnection = Depends(get_conn)):
    rows = await fetch_all(conn, "SELECT * FROM device_types ORDER BY is_builtin DESC, type_id")
    return [{**r, "capabilities": _json(r["capabilities"])} for r in rows]


@router.post("/device-types", status_code=201)
async def create_device_type(body: DeviceTypeIn, request: Request, admin: dict = Depends(admin_user),
                             conn: AsyncConnection = Depends(get_conn)):
    if not ID_RE.match(body.type_id):
        raise HTTPException(422, "type_id는 영문/숫자/_ 2~50자")
    if await fetch_one(conn, "SELECT type_id FROM device_types WHERE type_id = :t", t=body.type_id):
        raise HTTPException(409, "이미 존재하는 타입입니다.")
    await _check_caps(conn, body.capabilities)
    await execute(conn, "INSERT INTO device_types (type_id, name, capabilities) VALUES (:t, :n, :c)",
                  t=body.type_id, n=body.name, c=json.dumps(body.capabilities))
    await log_action(conn, request, admin["id"], "device_type.create", body.type_id)
    return {"ok": True}


@router.put("/device-types/{type_id}")
async def update_device_type(type_id: str, body: DeviceTypeIn, request: Request, admin: dict = Depends(admin_user),
                             conn: AsyncConnection = Depends(get_conn)):
    await _check_caps(conn, body.capabilities)
    n = await execute(conn, "UPDATE device_types SET name = :n, capabilities = :c WHERE type_id = :t",
                      n=body.name, c=json.dumps(body.capabilities), t=type_id)
    if not n and not await fetch_one(conn, "SELECT 1 FROM device_types WHERE type_id = :t", t=type_id):
        raise HTTPException(404, "타입을 찾을 수 없습니다.")
    await log_action(conn, request, admin["id"], "device_type.update", type_id, {"capabilities": body.capabilities})
    return {"ok": True}


# ─────────────── Capability 마스터 ───────────────

class CapabilityIn(BaseModel):
    cap_id: str
    display_name: str = Field(..., max_length=100)
    icon: str | None = Field(None, max_length=10)
    component_name: str = Field(..., max_length=100)
    mqtt_action: str | None = None
    telemetry_keys: list[str] | None = None
    param_schema: dict | None = None
    is_active: bool = True


@router.get("/capabilities")
async def capabilities(conn: AsyncConnection = Depends(get_conn)):
    rows = await fetch_all(conn, "SELECT * FROM capability_definitions ORDER BY cap_id")
    return [{**r, "telemetry_keys": _json(r["telemetry_keys"]), "param_schema": _json(r["param_schema"])}
            for r in rows]


async def _upsert_cap(conn: AsyncConnection, body: CapabilityIn) -> None:
    await execute(conn, """INSERT INTO capability_definitions
        (cap_id, display_name, icon, component_name, mqtt_action, telemetry_keys, param_schema, is_active)
        VALUES (:id, :dn, :ic, :cn, :ma, :tk, :ps, :ac)
        ON DUPLICATE KEY UPDATE display_name = VALUES(display_name), icon = VALUES(icon),
          component_name = VALUES(component_name), mqtt_action = VALUES(mqtt_action),
          telemetry_keys = VALUES(telemetry_keys), param_schema = VALUES(param_schema), is_active = VALUES(is_active)""",
        id=body.cap_id, dn=body.display_name, ic=body.icon, cn=body.component_name, ma=body.mqtt_action,
        tk=json.dumps(body.telemetry_keys) if body.telemetry_keys is not None else None,
        ps=json.dumps(body.param_schema) if body.param_schema is not None else None, ac=body.is_active)


@router.post("/capabilities", status_code=201)
async def create_capability(body: CapabilityIn, request: Request, admin: dict = Depends(admin_user),
                            conn: AsyncConnection = Depends(get_conn)):
    if not ID_RE.match(body.cap_id):
        raise HTTPException(422, "cap_id는 영문/숫자/_ 2~50자")
    if await fetch_one(conn, "SELECT 1 FROM capability_definitions WHERE cap_id = :c", c=body.cap_id):
        raise HTTPException(409, "이미 존재하는 capability입니다.")
    await _upsert_cap(conn, body)
    await log_action(conn, request, admin["id"], "capability.create", body.cap_id)
    return {"ok": True}


@router.put("/capabilities/{cap_id}")
async def update_capability(cap_id: str, body: CapabilityIn, request: Request, admin: dict = Depends(admin_user),
                            conn: AsyncConnection = Depends(get_conn)):
    body.cap_id = cap_id
    await _upsert_cap(conn, body)
    await log_action(conn, request, admin["id"], "capability.update", cap_id)
    return {"ok": True}


# ─────────────── 알림 규칙 ───────────────

class RuleIn(BaseModel):
    capability: str | None = None
    field: str = Field(..., max_length=50)
    operator: Literal[">", ">=", "<", "<=", "==", "!="]
    threshold: float
    channels: list[Literal["push"]] = ["push"]
    message_tpl: str | None = None
    cooldown_min: int = Field(10, ge=0, le=10080)
    is_active: bool = True


@router.get("/notification-rules")
async def rules(conn: AsyncConnection = Depends(get_conn)):
    rows = await fetch_all(conn, "SELECT * FROM notification_rules ORDER BY id")
    return [{**r, "channels": _json(r["channels"]), "threshold": float(r["threshold"])} for r in rows]


@router.post("/notification-rules", status_code=201)
async def create_rule(body: RuleIn, conn: AsyncConnection = Depends(get_conn)):
    rid = await execute(conn, """INSERT INTO notification_rules
        (capability, field, operator, threshold, channels, message_tpl, cooldown_min, is_active)
        VALUES (:c, :f, :o, :t, :ch, :m, :cd, :a)""",
        c=body.capability, f=body.field, o=body.operator, t=body.threshold, ch=json.dumps(body.channels),
        m=body.message_tpl, cd=body.cooldown_min, a=body.is_active)
    return {"id": rid}


@router.put("/notification-rules/{rid}")
async def update_rule(rid: int, body: RuleIn, conn: AsyncConnection = Depends(get_conn)):
    await execute(conn, """UPDATE notification_rules SET capability = :c, field = :f, operator = :o, threshold = :t,
        channels = :ch, message_tpl = :m, cooldown_min = :cd, is_active = :a WHERE id = :id""",
        c=body.capability, f=body.field, o=body.operator, t=body.threshold, ch=json.dumps(body.channels),
        m=body.message_tpl, cd=body.cooldown_min, a=body.is_active, id=rid)
    return {"ok": True}


@router.delete("/notification-rules/{rid}")
async def delete_rule(rid: int, conn: AsyncConnection = Depends(get_conn)):
    await execute(conn, "DELETE FROM notification_rules WHERE id = :id", id=rid)
    return {"ok": True}


# ─────────────── 펌웨어 / OTA ───────────────

@router.get("/firmware")
async def firmware_list(conn: AsyncConnection = Depends(get_conn)):
    return await fetch_all(conn, "SELECT * FROM firmware_versions ORDER BY released_at DESC")


@router.post("/firmware", status_code=201)
async def firmware_upload(request: Request, version: str = Form(...), device_type: str = Form(...),
                          changelog: str = Form(""), is_stable: bool = Form(False),
                          file: UploadFile = File(...), admin: dict = Depends(admin_user),
                          conn: AsyncConnection = Depends(get_conn)):
    if not VERSION_RE.match(version) or not ID_RE.match(device_type):
        raise HTTPException(422, "잘못된 버전 또는 타입")
    if not await fetch_one(conn, "SELECT 1 FROM device_types WHERE type_id = :t", t=device_type):
        raise HTTPException(422, "알 수 없는 디바이스 타입")
    data = await file.read()
    if not data or len(data) > 16 * 1024 * 1024:
        raise HTTPException(422, "펌웨어 파일 크기가 올바르지 않습니다 (최대 16MB).")
    s = get_settings()
    folder = os.path.join(s.firmware_dir, device_type)
    os.makedirs(folder, exist_ok=True)
    fname = f"{device_type}-{version}.bin"
    with open(os.path.join(folder, fname), "wb") as f:
        f.write(data)
    sha = hashlib.sha256(data).hexdigest()
    url = f"{s.firmware_base_url.rstrip('/')}/{device_type}/{fname}"
    await execute(conn, """INSERT INTO firmware_versions (version, device_type, file_url, sha256, changelog, is_stable)
                           VALUES (:v, :t, :u, :s, :c, :st)
                           ON DUPLICATE KEY UPDATE file_url = VALUES(file_url), sha256 = VALUES(sha256),
                             changelog = VALUES(changelog), is_stable = VALUES(is_stable)""",
                  v=version, t=device_type, u=url, s=sha, c=changelog, st=is_stable)
    await log_action(conn, request, admin["id"], "firmware.upload", f"{device_type}@{version}", {"sha256": sha})
    return {"version": version, "file_url": url, "sha256": sha}


class DeployIn(BaseModel):
    device_type: str
    device_ids: list[str] | None = None   # None = 해당 타입 전체


@router.post("/firmware/{version}/deploy")
async def firmware_deploy(version: str, body: DeployIn, request: Request, admin: dict = Depends(admin_user),
                          conn: AsyncConnection = Depends(get_conn)):
    fw = await fetch_one(conn, "SELECT * FROM firmware_versions WHERE version = :v AND device_type = :t",
                         v=version, t=body.device_type)
    if not fw:
        raise HTTPException(404, "펌웨어를 찾을 수 없습니다.")
    targets = await fetch_all(conn, "SELECT id FROM devices WHERE type_id = :t", t=body.device_type)
    ids = [t["id"] for t in targets if body.device_ids is None or t["id"] in body.device_ids]
    sent = []
    for did in ids:
        try:
            await mqtt_bridge.publish_command(did, "ota", {"url": fw["file_url"], "version": version,
                                                           "sha256": fw["sha256"]})
            sent.append(did)
        except RuntimeError as e:
            raise HTTPException(503, str(e))
    await log_action(conn, request, admin["id"], "firmware.deploy", f"{body.device_type}@{version}", {"devices": sent})
    return {"sent": sent}


# ─────────────── 로그 ───────────────

@router.get("/logs")
async def logs(kind: Literal["audit", "mqtt", "login"] = "audit", limit: int = 200,
               conn: AsyncConnection = Depends(get_conn)):
    limit = max(1, min(limit, 1000))
    if kind == "audit":
        return await fetch_all(conn, """SELECT a.*, u.email_raw AS actor_email FROM audit_logs a
                                        LEFT JOIN users u ON u.id = a.actor_id ORDER BY a.id DESC LIMIT :l""", l=limit)
    if kind == "mqtt":
        return await fetch_all(conn, "SELECT * FROM device_events ORDER BY id DESC LIMIT :l", l=limit)
    return await fetch_all(conn, "SELECT * FROM login_attempts ORDER BY id DESC LIMIT :l", l=limit)
