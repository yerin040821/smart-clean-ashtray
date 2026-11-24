import json

from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel, Field
from sqlalchemy.ext.asyncio import AsyncConnection

from ..db import execute, fetch_all, fetch_one, get_conn
from ..security.deps import current_user

router = APIRouter(prefix="/inquiry", tags=["inquiry"])

MAX_CONTENT = 1_000_000


def validate_tiptap_doc(content: dict) -> str:
    """TipTap JSON 문서만 저장 (HTML 저장 시 XSS 위험 → 렌더링은 TipTap read-only 에디터로)."""
    if not isinstance(content, dict) or content.get("type") != "doc":
        raise HTTPException(422, "잘못된 문서 형식입니다.")
    raw = json.dumps(content, ensure_ascii=False)
    if len(raw) > MAX_CONTENT:
        raise HTTPException(413, "내용이 너무 깁니다.")
    return raw


class InquiryIn(BaseModel):
    title: str = Field(..., min_length=1, max_length=200)
    content: dict


@router.post("", status_code=201)
async def create(body: InquiryIn, user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    iid = await execute(conn, "INSERT INTO inquiries (user_id, title, content) VALUES (:u, :t, :c)",
                        u=user["id"], t=body.title, c=validate_tiptap_doc(body.content))
    return {"id": iid}


@router.get("")
async def my_list(user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    return await fetch_all(conn, """SELECT id, title, status, created_at FROM inquiries
                                    WHERE user_id = :u ORDER BY id DESC""", u=user["id"])


async def inquiry_detail(conn: AsyncConnection, iid: int) -> dict:
    inq = await fetch_one(conn, """SELECT i.*, u.email_raw AS user_email FROM inquiries i
                                   JOIN users u ON u.id = i.user_id WHERE i.id = :id""", id=iid)
    if not inq:
        raise HTTPException(404, "문의를 찾을 수 없습니다.")
    inq["content"] = json.loads(inq["content"])
    replies = await fetch_all(conn, "SELECT id, content, created_at FROM inquiry_replies WHERE inquiry_id = :id ORDER BY id",
                              id=iid)
    inq["replies"] = [{**r, "content": json.loads(r["content"])} for r in replies]
    return inq


@router.get("/{iid}")
async def detail(iid: int, user: dict = Depends(current_user), conn: AsyncConnection = Depends(get_conn)):
    inq = await inquiry_detail(conn, iid)
    if inq["user_id"] != user["id"]:
        raise HTTPException(404, "문의를 찾을 수 없습니다.")
    return inq
