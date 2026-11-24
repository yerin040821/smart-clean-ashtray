import secrets
from typing import Literal

import jwt
from fastapi import APIRouter, BackgroundTasks, Depends, HTTPException, Request, Response
from pydantic import BaseModel, EmailStr
from sqlalchemy.ext.asyncio import AsyncConnection

from ..db import execute, execute_autonomous, fetch_all, fetch_one, get_conn
from ..security.deps import client_ip, current_user
from ..security.email_norm import normalize_email
from ..security.passwords import hash_password, is_reused, validate_password_policy, verify_password
from ..security.rate_limit import check_rate_limit, record_attempt
from ..security.tokens import REFRESH_COOKIE, clear_auth_cookies, decode, set_auth_cookies
from ..services.mailer import send_code

router = APIRouter(prefix="/auth", tags=["auth"])

CODE_TTL_MIN = 5
MAX_CODE_ATTEMPTS = 5
Purpose = Literal["register", "password_reset"]


class RegisterIn(BaseModel):
    email: EmailStr
    password: str


class VerifyIn(BaseModel):
    email: EmailStr
    code: str


class LoginIn(BaseModel):
    email: EmailStr
    password: str


class ResetRequestIn(BaseModel):
    email: EmailStr


class ResetConfirmIn(BaseModel):
    email: EmailStr
    code: str
    new_password: str


class ChangePasswordIn(BaseModel):
    current_password: str
    new_password: str


async def _issue_code(conn: AsyncConnection, bg: BackgroundTasks, email: str, to: str, purpose: Purpose) -> None:
    recent = await fetch_one(conn, """SELECT id FROM email_verifications WHERE email = :e AND purpose = :p
                                      AND created_at > NOW() - INTERVAL 60 SECOND""", e=email, p=purpose)
    if recent:
        raise HTTPException(429, "인증 코드는 1분에 한 번만 요청할 수 있습니다.")
    code = f"{secrets.randbelow(10**6):06d}"
    await execute(conn, "UPDATE email_verifications SET used = TRUE WHERE email = :e AND purpose = :p AND used = FALSE",
                  e=email, p=purpose)
    await execute(conn, """INSERT INTO email_verifications (email, code, purpose, expires_at)
                           VALUES (:e, :c, :p, NOW() + INTERVAL :ttl MINUTE)""",
                  e=email, c=code, p=purpose, ttl=CODE_TTL_MIN)
    bg.add_task(send_code, to, code, purpose)


async def _consume_code(conn: AsyncConnection, email: str, code: str, purpose: Purpose) -> None:
    row = await fetch_one(conn, """SELECT id, code, attempts FROM email_verifications
                                   WHERE email = :e AND purpose = :p AND used = FALSE AND expires_at > NOW()
                                   ORDER BY id DESC LIMIT 1""", e=email, p=purpose)
    if not row or row["attempts"] >= MAX_CODE_ATTEMPTS:
        raise HTTPException(400, "인증 코드가 만료되었거나 유효하지 않습니다.")
    if not secrets.compare_digest(row["code"], code.strip()):
        await execute_autonomous("UPDATE email_verifications SET attempts = attempts + 1 WHERE id = :id",
                                 id=row["id"])
        raise HTTPException(400, "인증 코드가 일치하지 않습니다.")
    await execute(conn, "UPDATE email_verifications SET used = TRUE WHERE id = :id", id=row["id"])


async def _recent_hashes(conn: AsyncConnection, user_id: int) -> list[str]:
    rows = await fetch_all(conn, "SELECT hash FROM password_history WHERE user_id = :u ORDER BY id DESC LIMIT 3",
                           u=user_id)
    return [r["hash"] for r in rows]


async def _set_password(conn: AsyncConnection, user_id: int, pw: str, bump_version: bool) -> None:
    h = hash_password(pw)
    await execute(conn, f"""UPDATE users SET password_hash = :h
                            {', token_version = token_version + 1' if bump_version else ''} WHERE id = :id""",
                  h=h, id=user_id)
    await execute(conn, "INSERT INTO password_history (user_id, hash) VALUES (:u, :h)", u=user_id, h=h)


def _check_policy(pw: str, email: str) -> None:
    errors = validate_password_policy(pw, email)
    if errors:
        raise HTTPException(422, " ".join(errors))


@router.post("/register", status_code=201)
async def register(body: RegisterIn, bg: BackgroundTasks, conn: AsyncConnection = Depends(get_conn)):
    email = normalize_email(body.email)
    _check_policy(body.password, body.email)
    user = await fetch_one(conn, "SELECT id, email_verified FROM users WHERE email = :e", e=email)
    if user and user["email_verified"]:
        raise HTTPException(409, "이미 가입된 이메일입니다.")
    if user:  # 미인증 상태 재가입 → 비밀번호 갱신 후 코드 재발송
        await _set_password(conn, user["id"], body.password, bump_version=True)
    else:
        uid = await execute(conn, """INSERT INTO users (email, email_raw, password_hash)
                                     VALUES (:e, :r, :h)""", e=email, r=body.email, h=hash_password(body.password))
        h = (await fetch_one(conn, "SELECT password_hash FROM users WHERE id = :id", id=uid))["password_hash"]
        await execute(conn, "INSERT INTO password_history (user_id, hash) VALUES (:u, :h)", u=uid, h=h)
    await _issue_code(conn, bg, email, body.email, "register")
    return {"message": "인증 코드를 이메일로 보냈습니다."}


@router.post("/verify-email")
async def verify_email(body: VerifyIn, conn: AsyncConnection = Depends(get_conn)):
    email = normalize_email(body.email)
    await _consume_code(conn, email, body.code, "register")
    await execute(conn, "UPDATE users SET email_verified = TRUE WHERE email = :e", e=email)
    return {"message": "이메일 인증이 완료되었습니다."}


@router.post("/login")
async def login(body: LoginIn, request: Request, response: Response, conn: AsyncConnection = Depends(get_conn)):
    email = normalize_email(body.email)
    ip = client_ip(request)
    await check_rate_limit(conn, email, ip)
    user = await fetch_one(conn, "SELECT * FROM users WHERE email = :e", e=email)
    ok = bool(user) and verify_password(user["password_hash"], body.password)
    await record_attempt(email, ip, ok)
    if not ok:
        raise HTTPException(401, "이메일 또는 비밀번호가 올바르지 않습니다.")
    if not user["email_verified"]:
        raise HTTPException(403, "이메일 인증이 필요합니다.")
    set_auth_cookies(response, user["id"], user["role"], user["token_version"])
    return {"id": user["id"], "email": user["email_raw"], "role": user["role"]}


@router.post("/logout")
async def logout(response: Response, request: Request, conn: AsyncConnection = Depends(get_conn)):
    fcm_token = request.headers.get("X-FCM-Token")
    if fcm_token:
        await execute(conn, "DELETE FROM fcm_tokens WHERE token = :t", t=fcm_token)
    clear_auth_cookies(response)
    return {"message": "로그아웃되었습니다."}


@router.post("/refresh")
async def refresh(request: Request, response: Response, conn: AsyncConnection = Depends(get_conn)):
    token = request.cookies.get(REFRESH_COOKIE)
    if not token:
        raise HTTPException(401, "로그인이 필요합니다.")
    try:
        data = decode(token, "refresh")
    except jwt.PyJWTError:
        raise HTTPException(401, "세션이 만료되었습니다.")
    user = await fetch_one(conn, "SELECT id, role, token_version FROM users WHERE id = :id", id=int(data["sub"]))
    if not user or user["token_version"] != data.get("ver"):
        raise HTTPException(401, "세션이 만료되었습니다.")
    set_auth_cookies(response, user["id"], user["role"], user["token_version"])
    return {"message": "ok"}


@router.post("/password-reset")
async def password_reset(body: ResetRequestIn, bg: BackgroundTasks, conn: AsyncConnection = Depends(get_conn)):
    email = normalize_email(body.email)
    user = await fetch_one(conn, "SELECT id FROM users WHERE email = :e AND email_verified = TRUE", e=email)
    if user:
        await _issue_code(conn, bg, email, body.email, "password_reset")
    # 가입 여부 노출 방지 — 항상 동일 응답
    return {"message": "가입된 이메일이라면 인증 코드가 발송됩니다."}


@router.post("/password-reset/confirm")
async def password_reset_confirm(body: ResetConfirmIn, conn: AsyncConnection = Depends(get_conn)):
    email = normalize_email(body.email)
    user = await fetch_one(conn, "SELECT id FROM users WHERE email = :e", e=email)
    if not user:
        raise HTTPException(400, "인증 코드가 만료되었거나 유효하지 않습니다.")
    _check_policy(body.new_password, body.email)
    if is_reused(body.new_password, await _recent_hashes(conn, user["id"])):
        raise HTTPException(422, "최근 사용한 3개의 비밀번호는 다시 사용할 수 없습니다.")
    await _consume_code(conn, email, body.code, "password_reset")
    await _set_password(conn, user["id"], body.new_password, bump_version=True)
    return {"message": "비밀번호가 변경되었습니다. 다시 로그인해 주세요."}


@router.get("/me")
async def me(user: dict = Depends(current_user)):
    return {"id": user["id"], "email": user["email_raw"], "role": user["role"]}


@router.post("/password-change")
async def password_change(body: ChangePasswordIn, response: Response, user: dict = Depends(current_user),
                          conn: AsyncConnection = Depends(get_conn)):
    row = await fetch_one(conn, "SELECT password_hash, role, token_version FROM users WHERE id = :id", id=user["id"])
    if not verify_password(row["password_hash"], body.current_password):
        raise HTTPException(400, "현재 비밀번호가 올바르지 않습니다.")
    _check_policy(body.new_password, user["email_raw"])
    if is_reused(body.new_password, await _recent_hashes(conn, user["id"])):
        raise HTTPException(422, "최근 사용한 3개의 비밀번호는 다시 사용할 수 없습니다.")
    await _set_password(conn, user["id"], body.new_password, bump_version=True)
    # 다른 기기 세션은 무효화, 현재 세션은 새 버전으로 재발급
    set_auth_cookies(response, user["id"], row["role"], row["token_version"] + 1)
    return {"message": "비밀번호가 변경되었습니다."}
