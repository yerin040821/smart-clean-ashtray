"""Gmail SMTP — 6자리 인증 코드 발송."""
import logging
from email.message import EmailMessage

import aiosmtplib

from ..config import get_settings

log = logging.getLogger(__name__)

SUBJECTS = {"register": "회원가입 인증 코드", "password_reset": "비밀번호 재설정 인증 코드"}


async def send_code(to: str, code: str, purpose: str) -> None:
    s = get_settings()
    if not s.smtp_user:
        log.warning("SMTP 미설정 — 개발 모드: %s 인증코드 %s (%s)", to, code, purpose)
        return
    msg = EmailMessage()
    msg["From"] = s.smtp_from or s.smtp_user
    msg["To"] = to
    msg["Subject"] = f"[Smart Clean Ashtray] {SUBJECTS[purpose]}"
    msg.set_content(f"인증 코드: {code}\n\n5분 안에 입력해 주세요. 본인이 요청하지 않았다면 무시하셔도 됩니다.")
    await aiosmtplib.send(msg, hostname=s.smtp_host, port=s.smtp_port, start_tls=True,
                          username=s.smtp_user, password=s.smtp_password)
