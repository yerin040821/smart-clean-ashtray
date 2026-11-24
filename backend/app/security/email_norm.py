"""4-6. Gmail 별칭(+) 차단 — 가입 중복 체크용 이메일 정규화."""

GMAIL_DOMAINS = {"gmail.com", "googlemail.com"}


def normalize_email(email: str) -> str:
    local, _, domain = email.strip().lower().rpartition("@")
    if not local or not domain:
        raise ValueError("올바른 이메일 형식이 아닙니다.")
    local = local.split("+", 1)[0]          # + 이후 제거 (모든 도메인)
    if domain in GMAIL_DOMAINS:
        # 점(.) 무시는 Gmail에서만 같은 메일함으로 취급됨 — 타 도메인에 적용하면 다른 사람이 충돌
        local = local.replace(".", "")
        domain = "gmail.com"
    return f"{local}@{domain}"
