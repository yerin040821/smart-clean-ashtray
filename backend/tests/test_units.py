"""DB 없이 실행 가능한 순수 로직 단위 테스트:  cd backend && pytest -q"""
import os
import sys
from datetime import date

import pytest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
os.environ.setdefault("SECRET_KEY", "x" * 40)

from fastapi import HTTPException  # noqa: E402

from app.config import validate_secret_key  # noqa: E402
from app.routers.devices import validate_params  # noqa: E402
from app.security.deps import ip_allowed  # noqa: E402
from app.security.email_norm import normalize_email  # noqa: E402
from app.security.passwords import hash_password, is_reused, validate_password_policy  # noqa: E402
from app.services.mqtt_bridge import parse_topic  # noqa: E402
from app.services.notifier import matches  # noqa: E402
from scripts.manage_partitions import plan  # noqa: E402


# ── 4-2 비밀키 ──
def test_secret_key_length():
    with pytest.raises(RuntimeError):
        validate_secret_key("short")
    validate_secret_key("a" * 32)


# ── 4-6 이메일 정규화 ──
@pytest.mark.parametrize("raw,expected", [
    ("Foo.Bar+spam@Gmail.com", "foobar@gmail.com"),
    ("foo.bar@googlemail.com", "foobar@gmail.com"),
    ("first.last+x@naver.com", "first.last@naver.com"),   # 타 도메인은 점 유지
])
def test_normalize_email(raw, expected):
    assert normalize_email(raw) == expected


# ── 4-5 비밀번호 정책 ──
@pytest.mark.parametrize("pw", ["short1!", "abcdefgh", "Pass1234!", "aaaa9876x", "zxcvqwer"])
def test_password_rejected(pw):
    assert validate_password_policy(pw, "user@test.com")


def test_password_contains_email():
    assert validate_password_policy("hong99!Xk", "hong99@test.com")


@pytest.mark.parametrize("pw", ["Ash!tray7k", "sm4rt-cl3an", "Qz8#mPw2"])
def test_password_ok(pw):
    assert validate_password_policy(pw, "user@test.com") == []


def test_password_reuse():
    hashes = [hash_password(p) for p in ("Old1!pass", "Old2!pass", "Old3!pass")]
    assert is_reused("Old2!pass", hashes)
    assert not is_reused("New9!pass", hashes)


# ── 관리자 IP ──
def test_ip_allowlist():
    allow = "1.2.3.4, 192.168.0.0/24"
    assert ip_allowed("1.2.3.4", allow)
    assert ip_allowed("192.168.0.77", allow)
    assert not ip_allowed("192.168.1.1", allow)
    assert not ip_allowed("garbage", allow)


# ── Capability 파라미터 검증 ──
def test_validate_params():
    schema = {"duration_ms": {"type": "int", "min": 100, "max": 10000, "default": 1000},
              "color": {"type": "color", "default": "#00ff88"}}
    assert validate_params(schema, {}) == {"duration_ms": 1000, "color": "#00ff88"}
    assert validate_params(schema, {"duration_ms": "500", "evil": 1})["duration_ms"] == 500
    with pytest.raises(HTTPException):
        validate_params(schema, {"duration_ms": 99999})
    with pytest.raises(HTTPException):
        validate_params(schema, {"color": "red;drop"})


# ── MQTT 토픽 라우팅 ──
def test_parse_topic():
    assert parse_topic("ashtray/SCA_S/telemetry", {}) == ("SCA_S", "telemetry")
    assert parse_topic("ashtray/SCA_Y/status", {}) == ("SCA_Y", "status")
    assert parse_topic("ashtray/v1/telemetry", {"device_id": "SCA_Y"}) == ("SCA_Y", "telemetry")
    assert parse_topic("ashtray/v1/telemetry", {}) is None
    assert parse_topic("ashtray/SCA_S/command", {}) is None


def test_rule_match():
    assert matches(85, ">=", 80)
    assert matches("1", "==", 1)
    assert not matches("abc", ">", 1)
    assert not matches(1, "~", 1)


# ── 파티션 관리 ──
def test_partition_plan():
    existing = {"p202611", "p202612", "pmax"}
    add, drop = plan(date(2026, 12, 1), existing)
    assert add == [("p202701", "202702")]
    assert drop == []
    existing = {f"p{y}{m:02d}" for y in (2026, 2027) for m in range(1, 13)} | {"pmax"}
    add, drop = plan(date(2027, 12, 1), existing)
    assert add == [("p202801", "202802")]
    assert drop[-1] == "p202611" and "p202612" not in drop   # 13개월 전까지 삭제
