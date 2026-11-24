"""4-5. 비밀번호 정책 + 해싱 (argon2id)."""
import re

from argon2 import PasswordHasher
from argon2.exceptions import InvalidHashError, VerifyMismatchError

_ph = PasswordHasher()

SEQ_LEN = 4  # 4자 이상 연속/반복 금지


def hash_password(pw: str) -> str:
    return _ph.hash(pw)


def verify_password(hashed: str, pw: str) -> bool:
    try:
        return _ph.verify(hashed, pw)
    except (VerifyMismatchError, InvalidHashError):
        return False


def _has_sequence(pw: str) -> bool:
    s = pw.lower()
    for i in range(len(s) - SEQ_LEN + 1):
        chunk = s[i : i + SEQ_LEN]
        if len(set(chunk)) == 1:                                   # aaaa, 1111
            return True
        diffs = {ord(chunk[j + 1]) - ord(chunk[j]) for j in range(SEQ_LEN - 1)}
        if (diffs == {1} or diffs == {-1}) and chunk.isalnum():  # 1234, abcd, 4321, dcba
            return True
    return False


def validate_password_policy(pw: str, email: str) -> list[str]:
    """위반 사유 목록 반환 (빈 리스트 = 통과). 재사용 검사는 DB가 필요하므로 별도."""
    errors: list[str] = []
    if len(pw) < 8:
        errors.append("비밀번호는 8자 이상이어야 합니다.")
    kinds = sum(
        bool(re.search(p, pw)) for p in (r"[A-Za-z]", r"[0-9]", r"[^A-Za-z0-9]")
    )
    if kinds < 2:
        errors.append("영문·숫자·특수문자 중 2종 이상을 조합해야 합니다.")
    if _has_sequence(pw):
        errors.append("연속되거나 반복되는 문자(1234, aaaa, abcd 등)는 사용할 수 없습니다.")
    local = email.lower().split("@")[0]
    if email.lower() in pw.lower() or (len(local) >= 3 and local in pw.lower()):
        errors.append("비밀번호에 이메일 주소를 포함할 수 없습니다.")
    return errors


def is_reused(pw: str, recent_hashes: list[str]) -> bool:
    """최근 3개 비밀번호 재사용 검사."""
    return any(verify_password(h, pw) for h in recent_hashes[:3])
