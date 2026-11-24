"""initial schema (설계서 10장 + 보완)

설계서 대비 수정 사항
- MySQL은 컬럼 인라인 `REFERENCES`를 무시함 → 명시적 FOREIGN KEY 제약으로 변경
- JSON 컬럼 리터럴 DEFAULT 불가 → 표현식 DEFAULT (JSON_ARRAY('push'))
- users: email_verified, token_version(로그아웃·비번변경 시 기존 JWT 무효화) 추가
- notifications / device_events / audit_logs / fcm_tokens 테이블 추가
- telemetry: 파티션 테이블은 FK 불가 → FK 없이 (device_id, recorded_at) 인덱스

Revision ID: 0001
"""
import json
from datetime import date

from alembic import op
from dateutil.relativedelta import relativedelta

revision = "0001"
down_revision = None
branch_labels = None
depends_on = None

CAPABILITIES = [
    # cap_id, display_name, icon, component, mqtt_action, telemetry_keys, param_schema
    # 공기질: MQ-135 보정값(ppm) 없음 → SCA_Y/SCA_S 공통 gas_diff(적응형 기준선 대비 변화량) 사용
    ("air_quality_monitor", "공기질", "🌫️", "AirQualityCard", None, ["gas_diff", "gas_raw"], None),
    ("temperature_humidity", "온습도", "🌡️", "TempHumidityCard", None, ["temperature", "humidity"], None),
    ("fire_detect", "화재 감지", "🔥", "FireDetectCard", None, ["fire"], None),
    ("water_spray", "소화 분사", "💧", "WaterSprayCard", "spray", ["pump_on"],
     {"duration_ms": {"type": "int", "min": 100, "max": 10000, "default": 1000}}),
    ("deodorize", "탈취", "🌿", "DeodorizeCard", "deodorize", None,
     {"duration_ms": {"type": "int", "min": 100, "max": 10000, "default": 1500}}),
    ("auto_clean", "자동 청소", "🧽", "AutoCleanCard", "clean", None, None),
    ("door_control", "덮개", "🚪", "DoorControlCard", "door", ["door_open"],
     {"open": {"type": "bool", "default": True}}),
    ("ash_level", "재 잔량", "📏", "AshLevelCard", None, ["ash_level"], None),
    ("water_level", "수위", "🚰", "WaterLevelCard", None, ["water_level", "water_ok"], None),
    ("sleep_mode", "절전 모드", "😴", "SleepModeCard", "sleep", ["sleeping"],
     {"enable": {"type": "bool", "default": True}}),
    ("buzzer", "부저", "🔔", "BuzzerCard", "buzzer", None,
     {"duration_ms": {"type": "int", "min": 100, "max": 5000, "default": 500}}),
    ("rgb_led", "LED", "💡", "RgbLedCard", "led", ["led_color"],
     {"color": {"type": "color", "default": "#00ff88"}, "brightness": {"type": "int", "min": 0, "max": 255, "default": 128}}),
]

DEVICE_TYPES = {
    "SCA_S": ("한승찬 재떨이", ["air_quality_monitor", "fire_detect", "water_spray", "deodorize", "auto_clean",
                              "door_control", "ash_level", "water_level", "sleep_mode", "rgb_led"]),
    "SCA_Y": ("정예린 재떨이", ["air_quality_monitor", "temperature_humidity", "fire_detect", "water_spray",
                              "ash_level", "water_level", "buzzer", "rgb_led"]),
}

RULES = [
    ("fire_detect", "fire", "==", 1, "🔥 {device} 화재 감지! 소화 동작 중", 1),
    ("ash_level", "ash_level", ">=", 80, "{device} 재 잔량 {value}% — 비워주세요", 60),
    ("water_level", "water_ok", "==", 0, "{device} 소화수 부족 — 물을 보충해 주세요", 60),
    # 펌웨어 SMOKE_THRESHOLD(50)와 동일 — 관리자 화면에서 조정 가능
    ("air_quality_monitor", "gas_diff", ">", 50, "{device} 공기질 나쁨 (가스 변화량 +{value})", 30),
]


def _partitions() -> str:
    first = date.today().replace(day=1)
    parts = []
    for i in range(3):
        m = first + relativedelta(months=i)
        nxt = m + relativedelta(months=1)
        parts.append(f"PARTITION p{m:%Y%m} VALUES LESS THAN ({nxt:%Y%m})")
    parts.append("PARTITION pmax VALUES LESS THAN MAXVALUE")
    return ",\n  ".join(parts)


def upgrade() -> None:
    op.execute("""
CREATE TABLE users (
  id             BIGINT AUTO_INCREMENT PRIMARY KEY,
  email          VARCHAR(255) NOT NULL UNIQUE,
  email_raw      VARCHAR(255) NOT NULL,
  password_hash  VARCHAR(255) NOT NULL,
  role           ENUM('user','admin') NOT NULL DEFAULT 'user',
  email_verified BOOLEAN NOT NULL DEFAULT FALSE,
  token_version  INT NOT NULL DEFAULT 0,
  created_at     DATETIME DEFAULT CURRENT_TIMESTAMP
)""")
    op.execute("""
CREATE TABLE password_history (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id    BIGINT NOT NULL,
  hash       VARCHAR(255) NOT NULL,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_user_created (user_id, created_at),
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
)""")
    op.execute("""
CREATE TABLE login_attempts (
  id           BIGINT AUTO_INCREMENT PRIMARY KEY,
  email        VARCHAR(255),
  ip           VARCHAR(45),
  success      BOOLEAN,
  attempted_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_email_ip (email, ip, attempted_at),
  INDEX idx_ip (ip, attempted_at),
  INDEX idx_attempted_at (attempted_at)
)""")
    op.execute("""
CREATE TABLE email_verifications (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  email      VARCHAR(255) NOT NULL,
  code       CHAR(6) NOT NULL,
  purpose    ENUM('register','password_reset') NOT NULL,
  expires_at DATETIME NOT NULL,
  used       BOOLEAN DEFAULT FALSE,
  attempts   INT NOT NULL DEFAULT 0,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_email_code (email, code)
)""")
    op.execute("""
CREATE TABLE capability_definitions (
  cap_id         VARCHAR(50) PRIMARY KEY,
  display_name   VARCHAR(100) NOT NULL,
  icon           VARCHAR(10),
  component_name VARCHAR(100) NOT NULL,
  mqtt_action    VARCHAR(50),
  telemetry_keys JSON,
  param_schema   JSON,
  is_active      BOOLEAN DEFAULT TRUE
)""")
    op.execute("""
CREATE TABLE device_types (
  type_id      VARCHAR(50) PRIMARY KEY,
  name         VARCHAR(100) NOT NULL,
  capabilities JSON NOT NULL,
  is_builtin   BOOLEAN DEFAULT FALSE,
  created_at   DATETIME DEFAULT CURRENT_TIMESTAMP
)""")
    op.execute("""
CREATE TABLE devices (
  id            VARCHAR(50) PRIMARY KEY,
  type_id       VARCHAR(50) NOT NULL,
  user_id       BIGINT,
  name          VARCHAR(100),
  local_ip      VARCHAR(45),
  local_key     CHAR(32),                 -- 로컬 모드 HTTP 인증 키 (등록 시 발급)
  online        BOOLEAN NOT NULL DEFAULT FALSE,
  state         VARCHAR(50),
  fw_version    VARCHAR(20),
  last_seen     DATETIME,
  registered_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_user (user_id),
  FOREIGN KEY (type_id) REFERENCES device_types(type_id),
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE SET NULL
)""")
    op.execute("""
CREATE TABLE device_capability_config (
  device_id  VARCHAR(50),
  capability VARCHAR(50),
  config     JSON,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (device_id, capability),
  FOREIGN KEY (device_id) REFERENCES devices(id) ON DELETE CASCADE
)""")
    op.execute(f"""
CREATE TABLE telemetry (
  id          BIGINT AUTO_INCREMENT,
  device_id   VARCHAR(50) NOT NULL,
  recorded_at DATETIME NOT NULL,
  payload     JSON NOT NULL,
  PRIMARY KEY (id, recorded_at),
  INDEX idx_device_time (device_id, recorded_at)
)
PARTITION BY RANGE (YEAR(recorded_at) * 100 + MONTH(recorded_at)) (
  {_partitions()}
)""")
    op.execute("""
CREATE TABLE device_events (
  id          BIGINT AUTO_INCREMENT PRIMARY KEY,
  device_id   VARCHAR(50) NOT NULL,
  topic       VARCHAR(100) NOT NULL,
  payload     JSON,
  created_at  DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_device_time (device_id, created_at),
  INDEX idx_created (created_at)
)""")
    op.execute("""
CREATE TABLE notifications (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id    BIGINT NOT NULL,
  device_id  VARCHAR(50),
  rule_id    INT,
  title      VARCHAR(200) NOT NULL,
  body       TEXT,
  is_read    BOOLEAN NOT NULL DEFAULT FALSE,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_user_time (user_id, created_at),
  INDEX idx_rule_device (rule_id, device_id, created_at),
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
)""")
    op.execute("""
CREATE TABLE inquiries (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id    BIGINT NOT NULL,
  title      VARCHAR(200) NOT NULL,
  content    LONGTEXT NOT NULL,
  status     ENUM('open','answered','closed') DEFAULT 'open',
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_user (user_id),
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
)""")
    op.execute("""
CREATE TABLE inquiry_replies (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  inquiry_id BIGINT NOT NULL,
  admin_id   BIGINT,
  content    LONGTEXT NOT NULL,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  FOREIGN KEY (inquiry_id) REFERENCES inquiries(id) ON DELETE CASCADE,
  FOREIGN KEY (admin_id) REFERENCES users(id) ON DELETE SET NULL
)""")
    op.execute("""
CREATE TABLE notification_rules (
  id           INT AUTO_INCREMENT PRIMARY KEY,
  capability   VARCHAR(50),
  field        VARCHAR(50) NOT NULL,
  operator     VARCHAR(5) NOT NULL,
  threshold    DECIMAL(10,2) NOT NULL,
  channels     JSON DEFAULT (JSON_ARRAY('push')),
  message_tpl  TEXT,
  cooldown_min INT DEFAULT 10,
  is_active    BOOLEAN DEFAULT TRUE
)""")
    op.execute("""
CREATE TABLE firmware_versions (
  version     VARCHAR(20) NOT NULL,
  device_type VARCHAR(50) NOT NULL,
  file_url    TEXT NOT NULL,
  sha256      CHAR(64),
  changelog   TEXT,
  is_stable   BOOLEAN DEFAULT FALSE,
  released_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (device_type, version),
  FOREIGN KEY (device_type) REFERENCES device_types(type_id)
)""")
    op.execute("""
CREATE TABLE fcm_tokens (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id    BIGINT NOT NULL,
  token      VARCHAR(512) NOT NULL UNIQUE,
  device_os  ENUM('android','ios','web') NOT NULL,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  INDEX idx_user (user_id),
  FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE
)""")
    op.execute("""
CREATE TABLE audit_logs (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  actor_id   BIGINT,
  action     VARCHAR(100) NOT NULL,
  target     VARCHAR(200),
  detail     JSON,
  ip         VARCHAR(45),
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_created (created_at)
)""")

    # ── 시드 데이터 ──
    conn = op.get_bind()
    from sqlalchemy import text
    for cap_id, name, icon, comp, action, keys, schema in CAPABILITIES:
        conn.execute(text(
            "INSERT INTO capability_definitions (cap_id, display_name, icon, component_name, mqtt_action,"
            " telemetry_keys, param_schema) VALUES (:a, :b, :c, :d, :e, :f, :g)"),
            dict(a=cap_id, b=name, c=icon, d=comp, e=action,
                 f=json.dumps(keys) if keys else None, g=json.dumps(schema) if schema else None))
    for type_id, (name, caps) in DEVICE_TYPES.items():
        conn.execute(text("INSERT INTO device_types (type_id, name, capabilities, is_builtin)"
                          " VALUES (:t, :n, :c, TRUE)"), dict(t=type_id, n=name, c=json.dumps(caps)))
    for cap, field, opr, th, tpl, cd in RULES:
        conn.execute(text("INSERT INTO notification_rules (capability, field, operator, threshold, channels,"
                          " message_tpl, cooldown_min) VALUES (:c, :f, :o, :t, :ch, :m, :cd)"),
                     dict(c=cap, f=field, o=opr, t=th, ch=json.dumps(["push"]), m=tpl, cd=cd))


def downgrade() -> None:
    for t in ("audit_logs", "fcm_tokens", "firmware_versions", "notification_rules", "inquiry_replies",
              "inquiries", "notifications", "device_events", "telemetry", "device_capability_config",
              "devices", "device_types", "capability_definitions", "email_verifications",
              "login_attempts", "password_history", "users"):
        op.execute(f"DROP TABLE IF EXISTS {t}")
