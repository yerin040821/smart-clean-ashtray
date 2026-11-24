# Smart Clean Ashtray — 통합 앱/웹 설계 초안

> 최초 작성: 2026-11-14 / 최종 업데이트: 2026-11-25  
> 목적: VS Code 개발 착수 전 아키텍처·보안·인프라 기획 메모  
> 상태: **설계 확정 완료 — 1차 구현 완료** (`smart-clean-ashtray-platform/`)  
> 구현 중 변경된 사항은 본문에 반영했고, 전체 목록은 **[16. 구현 반영 변경 이력](#16-구현-반영-변경-이력-2026-11-25)** 참고

---

## 1. 프로젝트 개요

### 1-1. 목표
ESP32-S3 기반 스마트 재떨이 2종을 웹/앱으로 원격 조작·모니터링하는 통합 플랫폼.

### 1-2. 등록 기기 2종

| 기기 ID | 구분 | 레포 | 주요 특이점 |
|---|---|---|---|
| `SCA_S` | 한승찬 재떨이 | halong61/smart-ashtray | 물 분사·탈취·덮개 제어, PIR 슬립, 10-State FSM |
| `SCA_Y` | 정예린 재떨이 | yerin040821/smart-clean-ashtray | 미스트 소화, AM2302 온습도, Float Switch 건식방지, `FIRE_DETECTED` 펄스 소화 |

#### SCA_S 주요 센서/액추에이터
- MQ-135, HC-SR04, PIR(GPIO14 wake), 플레임 센서
- 펌프(소화수/청소액/탈취제 — 탈취 펌프 **GPIO16**), 서보 덮개(**GPIO17**), WS2812B, OLED(SSD1306 ~~GPIO21/22~~ **GPIO21/47**)
- **[변경]** ESP32-S3에는 **GPIO22~25가 없음** → OLED SCL은 GPIO47. 기존 "GPIO21/22"는 구형 ESP32 핀맵 기준
- **[추가]** 펌웨어 신규 작성 (원본 저장소에 펌웨어 없음) — 핀: MQ-135 4, 플레임 5, HC-SR04 8/9, PIR 14, 소화수 펌프 15, 탈취 16, 서보 17, 청소 18, 부저 10, WS2812B 38 (`firmware/SCA_S/README.md`)
- **[추가]** 10-State: IDLE / OCCUPIED / SMOKING / DEODORIZING / ASH_FULL / CLEANING / WARNING / FIRE_DETECTED / ERROR / SLEEP
- ✅ GPIO 충돌 해소: 탈취 펌프 GPIO21→16, 서보 GPIO22→17
- 5-Task FreeRTOS: StateTask P4 / ActuatorTask P3 / SensorTask P2 / CommTask P1 / SchedulerTask P1

#### SCA_Y 주요 센서/액추에이터
- MQ-135(GPIO5), AM2302/DHT22(GPIO14), HC-SR04(GPIO6/7), Float Switch(GPIO8)
- F5305S MOSFET 펌프(GPIO18), WS2812B(GPIO4), Active Buzzer(GPIO17), SSH1106 OLED(GPIO21/47)
- 4-Task FreeRTOS: Task_State P4 / Task_Actuator P3 / Task_Sensor P2 / Task_MQTT P1

#### MQTT 토픽 통합 규칙
```
ashtray/{device_id}/telemetry   # SCA_S, SCA_Y 공통 (device_id로 구분)
ashtray/{device_id}/event       # 상태 전이 이벤트
ashtray/{device_id}/command     # 앱 → 기기
ashtray/{device_id}/status      # 기기 온라인 여부
```
> SCA_Y 기존 토픽(`ashtray/v1/telemetry`)은 device_id 필드 포함이므로 서버에서 라우팅 가능. 펌웨어 업데이트 시 위 규칙으로 통일 권장.

---

## 2. 도메인·서브도메인 구조

```
www.smart-clean-ashtray.com      사용자 웹앱 (React SPA)
admin.smart-clean-ashtray.com    관리자 페이지 (IP allowlist 적용)
api.smart-clean-ashtray.com      FastAPI 백엔드
mqtt.smart-clean-ashtray.com     Mosquitto (포트 8883 TLS)
```

> ⚠️ 도메인에 언더바(`_`)는 사용 불가 — `smart_clean_ashtray.com` → `smart-clean-ashtray.com`  
> ~~Let's Encrypt wildcard 인증서 (`*.smart-clean-ashtray.com`) 하나로 통일.~~  
> **[변경]** 와일드카드는 DNS-01 챌린지로만 발급 가능하고 cafe24 DNS는 certbot 자동화 플러그인이 없음 →
> `smart-clean-ashtray.com`, `www`, `admin`, `api` 를 SAN으로 묶은 **단일 인증서(webroot 방식)** 사용.
> `mqtt.*` 는 자체 CA(14장 ①)로 TLS 처리하므로 제외.  
> Nginx가 도메인별 프록시 처리.

### Nginx 핵심 설정

```nginx
# 관리자 — IP allowlist
server {
    server_name admin.smart-clean-ashtray.com;
    allow 1.2.3.4;      # 허용 IP 목록
    allow 192.168.0.0/24;
    deny all;
    location / { proxy_pass http://frontend:3000; }
}

# API — CORS origin 제한
server {
    server_name api.smart-clean-ashtray.com;
    location / { proxy_pass http://api:8000; }
}
```

> **Admin API 보안**: Origin 헤더 체크는 위조 가능하므로 **Nginx IP allowlist만** 신뢰.  
> ~~FastAPI 내에서도 `X-Forwarded-For` 기반 IP 검증을 2중으로 적용.~~  
> **[변경]** `X-Forwarded-For` 첫 값은 클라이언트가 위조 가능 → Nginx가 `$remote_addr`로 **덮어쓰는 `X-Real-IP`만** 신뢰해 2중 검증.  
> **[추가]** admin 서브도메인만 막으면 `api.*/admin/*` 로 우회 가능 → **api 서버 블록의 `location /admin/` 에도 동일 allowlist** 적용.
> allowlist는 `nginx/snippets/admin-allowlist.conf` 와 `.env` 의 `ADMIN_ALLOWED_IPS` 두 곳을 함께 관리.

---

## 3. 기술 스택

| 영역 | 선택 | 비고 |
|---|---|---|
| 사용자 프론트 | React 18 + Vite + TypeScript + Tailwind CSS | |
| 관리자 프론트 | React 18 (별도 빌드 or 같은 빌드 + route guard) | |
| 모바일 앱 | Flutter **3.50** + Riverpod 2.x | **[변경]** Android API **24+**, iOS **15+** (Flutter 3.50 최소 요구) |
| 에디터 | TipTap 3 | 문의·공지 작성용 — **JSON으로 저장** (HTML 저장 시 XSS) |
| 백엔드 | FastAPI (Python) + **SQLAlchemy async + asyncmy** | **[변경]** asyncpg는 PostgreSQL 전용 드라이버라 MySQL 사용 불가 |
| DB | **MySQL 8** | 단일 DB, 센서 데이터 파티셔닝 |
| 캐시/메시징 | **Redis 7** | **[추가]** WebSocket pub/sub · 알림 쿨다운 잠금 (API 다중 워커용) |
| MQTT Broker | Mosquitto 2 | 포트 1883(내부망 전용, 호스트 미공개) / 8883(TLS 외부) |
| 인프라 | Docker Compose | cafe24 VPS |
| 웹서버 | Nginx | 리버스 프록시 + TLS |
| 인증서 | Let's Encrypt + Certbot | cron 자동갱신 |
| 기기 | ESP32-S3 DevKitC-1 | FreeRTOS + PlatformIO |

---

## 4. 보안 설계

### 4-1. JWT — HttpOnly 쿠키

```
저장 위치: HttpOnly 쿠키 (JS 접근 불가 → XSS 방어)
SameSite:  Strict  (CSRF 1차 방어)
Secure:    true    (HTTPS에서만 전송)
```

#### CSRF 추가 방어 — Double Submit Cookie 패턴

```
로그인 시:
  서버 → csrf_token (일반 쿠키, JS 읽기 가능) 발급
  서버 → jwt (HttpOnly 쿠키) 발급

상태 변경 요청 시 (POST/PUT/DELETE):
  클라이언트 → 헤더에 X-CSRF-Token: <csrf_token 값> 포함
  서버 → 쿠키의 csrf_token == 헤더의 X-CSRF-Token 검증

공격자는 타 도메인에서 csrf_token 쿠키를 읽을 수 없으므로 위조 불가.
```

```python
# FastAPI CSRF 검증 미들웨어 개념
async def verify_csrf(request: Request):
    if request.method in ("POST", "PUT", "DELETE", "PATCH"):
        cookie_token = request.cookies.get("csrf_token")
        header_token = request.headers.get("X-CSRF-Token")
        if not cookie_token or cookie_token != header_token:
            raise HTTPException(status_code=403, detail="CSRF 검증 실패")
```

### 4-2. 비밀키 길이 강제

```python
# 서버 시작 시 검증
SECRET_KEY = os.environ.get("SECRET_KEY", "")
if len(SECRET_KEY) < 32:
    raise RuntimeError("SECRET_KEY는 32자 이상이어야 합니다. 서버를 시작할 수 없습니다.")
```

### 4-3. Rate Limiting (로그인 시도 제한)

| 단위 | 제한 | 잠금 시간 |
|---|---|---|
| 이메일 + IP | 10회 | 15분 |
| 이메일 | 50회 / 24시간 | 24시간 |
| IP | 30회 / 15분 | 30분 |

```python
# Redis 기반 카운터 (또는 MySQL 테이블로 대체 가능)
async def check_rate_limit(email: str, ip: str):
    key_combo = f"login:{email}:{ip}"
    key_email = f"login_email:{email}"
    key_ip    = f"login_ip:{ip}"
    # 각 키 카운트 증가 후 임계값 초과 시 429 반환
```

### 4-4. 스캔 경로 차단 (Nginx)

```nginx
# 숨김 파일, 서버 파일, phpMyAdmin 등 스캔 경로 차단
location ~* /(\.git|\.env|\.htaccess|wp-admin|phpmyadmin|admin_php|setup\.php) {
    return 404;
}
location ~* \.(sql|bak|log|conf|sh)$ {
    return 404;
}
```

### 4-5. 비밀번호 정책

```
최소 8자 이상
영문·숫자·특수문자 중 2종 이상 조합
연속 문자 금지 (1234, aaaa, abcd 등)
이메일 주소 포함 금지
최근 3개 비밀번호 재사용 금지
```

### 4-6. Gmail 별칭(+) 차단

```python
def normalize_email(email: str) -> str:
    local, _, domain = email.strip().lower().rpartition("@")
    local = local.split("+", 1)[0]          # + 이후 제거 (모든 도메인)
    if domain in ("gmail.com", "googlemail.com"):
        local = local.replace(".", "")      # 점(.) 무시는 Gmail에서만
        domain = "gmail.com"
    return f"{local}@{domain}"

# 가입 시 normalize된 이메일로 중복 체크
```
> **[변경]** 점(.) 제거를 모든 도메인에 적용하면 다른 서비스에서 서로 다른 사람의 주소(`a.b@naver.com` / `ab@naver.com`)가 충돌 → **Gmail에만** 적용.

### 4-7. [추가] 구현 시 보강한 보안 항목
| 항목 | 내용 |
|---|---|
| 세션 무효화 | `users.token_version` — 비밀번호 변경·재설정, 권한 변경 시 +1 → 기존 JWT 즉시 무효 |
| 토큰 구성 | access 15분 + refresh 14일(`path=/auth`), 둘 다 HttpOnly 쿠키. `csrf_token`은 JS가 읽도록 HttpOnly 아님 |
| 쿠키 도메인 | `.smart-clean-ashtray.com` — www/admin 페이지에서 api 쿠키 공유 |
| 실패 기록 | 로그인 실패·인증코드 오입력은 **별도 트랜잭션**으로 즉시 커밋 (401 응답 시 요청 트랜잭션 롤백돼도 남도록) |
| 인증 코드 | 1분에 1회 발급, 코드당 오입력 5회 제한 |
| 비밀번호 해시 | argon2id |
| 비번 찾기 | 가입 여부와 관계없이 동일 응답 (이메일 존재 여부 노출 방지) |
| WebSocket | Origin 검증(Cross-Site WebSocket Hijacking 방지) + 쿠키 인증 + 기기 소유자 확인 |
| 관리자 감사 | `audit_logs` 테이블에 권한 변경·타입/Capability 수정·펌웨어 업로드/배포 기록 |

---

## 5. 앱/웹 화면 구조

### 5-1. 사용자 (`www.smart-clean-ashtray.com`)

```
/                   홈 대시보드 (등록 기기 카드 목록)
/devices/setup      기기 초기 등록 (AP 모드 WiFi 설정)
/devices/:id        기기 상세 조작 (동적 Capability UI)
/devices/:id/history  센서 데이터 차트
/notifications      알림 목록
/inquiry            문의 작성 (TipTap 3 에디터)
/profile            계정 설정
```

### 5-2. 관리자 (`admin.smart-clean-ashtray.com` — IP allowlist + role:admin)

```
/admin                    대시보드 (전체 기기·유저 현황)
/admin/devices            전체 기기 목록·상태
/admin/device-types       디바이스 타입 관리 (SCA_S / SCA_Y / 커스텀)
/admin/capabilities       Capability 마스터 편집
/admin/users              회원 목록·권한 관리
/admin/inquiry            문의 목록·답변 (TipTap 3)
/admin/notifications      알림 규칙 설정
/admin/firmware           펌웨어 버전 관리·OTA 배포
/admin/logs               시스템 로그·MQTT 이벤트
```

---

## 6. 디바이스 타입 — Capability 동적 UI

### 6-1. 등록 기기 Capability 비교

| Capability | SCA_S | SCA_Y |
|---|---|---|
| `air_quality_monitor` | ✅ MQ-135 | ✅ MQ-135 |
| `temperature_humidity` | ❌ | ✅ AM2302 |
| `fire_detect` | ✅ 플레임 센서 | ✅ 가스변화량+온도상승률 |
| `water_spray` | ✅ 소화수 펌프 | ✅ 미스트 펄스 소화 |
| `deodorize` | ✅ 탈취 펌프 | ❌ |
| `auto_clean` | ✅ 청소액 펌프 | ❌ |
| `door_control` | ✅ 서보 덮개 | ❌ |
| `ash_level` | ✅ HC-SR04 | ✅ HC-SR04 |
| `water_level` | ✅ (수위 추정) | ✅ Float Switch |
| `sleep_mode` | ✅ PIR 딥슬립 | ❌ |
| `buzzer` | ❌ | ✅ Active Buzzer |
| `rgb_led` | ✅ WS2812B | ✅ WS2812B |

> **[변경]** `air_quality_monitor`: ppm 보정 없이 **`gas_diff`**(MQ-135 적응형 기준선 대비 변화량)로 표시·알림 — SCA_Y 알고리즘으로 두 기기 통일 (>50 흡연, <30 해소).  
> **[변경]** SCA_S `water_level`: 펌프 가동 시간 기반 **추정**(참고용, 분사를 막지 않음). 소화수는 **수동 보충** 후 전원 재시작 시 100%.

### 6-2. 동적 렌더링 개념 (React)

```tsx
const CAPABILITY_COMPONENTS: Record<string, React.FC<{deviceId: string}>> = {
  air_quality_monitor:  AirQualityCard,
  temperature_humidity: TempHumidityCard,
  fire_detect:          FireDetectCard,
  water_spray:          WaterSprayCard,
  deodorize:            DeodorizeCard,
  auto_clean:           AutoCleanCard,
  door_control:         DoorControlCard,
  ash_level:            AshLevelCard,
  water_level:          WaterLevelCard,
  sleep_mode:           SleepModeCard,
  buzzer:               BuzzerCard,
  rgb_led:              RgbLedCard,
};

function DeviceDetailPage({ device }) {
  return (
    <>
      {device.capabilities.map(cap => {
        const Component = CAPABILITY_COMPONENTS[cap];
        return Component ? <Component key={cap} deviceId={device.id} /> : null;
      })}
    </>
  );
}
```

---

## 7. 기기 WiFi 초기 설정 (AP 모드)

```
[1단계] ESP32 처음 부팅 → NVS에 WiFi 정보 없음
        → AP 모드: SSID "AshtraySetup_{deviceId}"

[2단계] 사용자 폰 WiFi → "AshtraySetup_XXXX" 연결

[3단계] 브라우저 → 192.168.4.1 (또는 www.smart-clean-ashtray.com/devices/setup)
        → 집 WiFi SSID·PW 입력
        → 계정 연동 토큰 입력 (앱에서 발급)

[4단계] ESP32가 집 WiFi로 재연결
        → NVS에 WiFi 정보 저장 (이후 자동 연결)
        → 서버에 기기 등록 완료
```

---

## 8. WiFi 연결 모드 (운영 중)

```
[로컬 모드] 폰과 ESP32가 같은 WiFi
  폰 앱 ──HTTP──▶ 192.168.x.x (또는 ashtray-{id}.local)
  응답 빠름, 인터넷 불필요

[클라우드 모드] 외부 네트워크
  폰 앱 ──HTTPS──▶ api.smart-clean-ashtray.com ──MQTT──▶ ESP32
  어디서나 접근 가능
```

앱이 500ms 타임아웃으로 로컬 먼저 시도 → 실패 시 클라우드 자동 폴백.

mDNS: ESP32가 `ashtray-{deviceId}.local` 등록 → IP 없이 로컬 접근 가능.

> **[추가] 로컬 모드 인증**: 인증이 없으면 같은 WiFi의 누구나 기기를 조작 가능 →
> 기기 등록 시 서버가 `local_key`(32 hex)를 발급해 ESP32 NVS에 저장, 앱은 로컬 요청에 `X-Local-Key` 헤더 첨부.
> 재등록 시 키가 바뀌어 이전 소유자의 로컬 접근도 차단됨.  
> **[참고]** 웹(HTTPS)은 혼합 콘텐츠 차단으로 `http://192.168.x.x` 호출 불가 → **로컬 모드는 Flutter 앱 전용**, 웹은 항상 클라우드.  
> 로컬 API 명세(`/api/status`, `/api/command`)는 `docs/protocol.md`.

---

## 9. 무중단 배포 (Docker + cafe24 VPS)

### 9-1. docker-compose.yml 구조

```yaml
services:
  nginx:
    image: nginx:alpine
    ports: ["80:80", "443:443"]
    volumes:
      - ./nginx/conf.d:/etc/nginx/conf.d
      - ./certbot/www:/var/www/certbot
      - ./certbot/conf:/etc/letsencrypt
    restart: always

  frontend:
    build: ./frontend
    restart: always

  api:
    build: ./backend
    restart: always
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8000/health"]
      interval: 10s
      timeout: 5s
      retries: 3
      start_period: 20s
    env_file: .env

  mosquitto:
    image: eclipse-mosquitto:2
    ports: ["8883:8883"]   # [변경] 1883은 내부망 전용 → 호스트에 공개하지 않음
    volumes:
      - ./mosquitto/config:/mosquitto/config
      - ./mosquitto/data:/mosquitto/data
    restart: always

  mysql:
    image: mysql:8.0
    volumes:
      - mysqldata:/var/lib/mysql
    env_file: .env
    restart: always
    # 절대 재시작 금지 → 마이그레이션만

  # [추가] API 다중 워커용 — 내부망 전용, 영속화 없음, 메모리 64MB
  redis:
    image: redis:7-alpine
    command: ["redis-server", "--save", "", "--appendonly", "no", "--maxmemory", "64mb"]
    restart: always

  # [추가] 9-3 cron이 `run --rm certbot renew` 로 호출 (초안엔 서비스 정의 누락)
  certbot:
    image: certbot/certbot
    volumes:
      - ./certbot/www:/var/www/certbot
      - ./certbot/conf:/etc/letsencrypt
    profiles: ["tools"]

volumes:
  mysqldata:
```
> 실제 파일은 healthcheck·depends_on 포함 — `smart-clean-ashtray-platform/docker-compose.yml`

### 9-2. 무중단 배포 명령어

```bash
# API만 업데이트
docker compose up -d --no-deps --build api

# 프론트만 업데이트
docker compose up -d --no-deps --build frontend

# DB 마이그레이션 (서비스 유지)
docker compose exec api alembic upgrade head

# Nginx 설정 리로드 (무중단)
docker compose exec nginx nginx -s reload
```

### 9-3. Let's Encrypt 자동갱신 cron

```bash
# crontab -e
0 3 * * * docker compose -f /home/ubuntu/app/docker-compose.yml run --rm certbot renew \
  && docker compose -f /home/ubuntu/app/docker-compose.yml exec nginx nginx -s reload
```
> 매일 새벽 3시 갱신 시도 (90일 만료, 30일 전부터 갱신).  
> cafe24 VPS에서 cron 직접 설정 필요.

---

## 10. DB 스키마 (MySQL 8)

> **[변경] 구현 마이그레이션(`backend/alembic/versions/0001_initial.py`)과 아래 초안의 차이**
> - MySQL은 컬럼 인라인 `REFERENCES`를 **무시**함 → 명시적 `FOREIGN KEY` 제약으로 작성
> - `channels JSON DEFAULT '["push"]'` 는 MySQL에서 오류 → `DEFAULT (JSON_ARRAY('push'))`
> - `users` + `email_verified`, `token_version` / `devices` + `local_key`, `online`, `state`, `fw_version`
> - `email_verifications` + `attempts` / `firmware_versions` PK를 `(device_type, version)`으로, + `sha256`
> - `telemetry` 파티션 테이블은 FK 불가 → FK 없이 `(device_id, recorded_at)` 인덱스
> - 신규 테이블: `notifications`(알림 목록), `device_events`(MQTT 이벤트 로그), `audit_logs`(감사 로그), `fcm_tokens`
> - 시드: device_types(SCA_S/SCA_Y), capability_definitions 12종, 기본 알림 규칙 4개(화재·잔량·수위·공기질)

```sql
-- 사용자
CREATE TABLE users (
  id           BIGINT AUTO_INCREMENT PRIMARY KEY,
  email        VARCHAR(255) UNIQUE NOT NULL,         -- normalize된 이메일
  email_raw    VARCHAR(255) NOT NULL,                -- 원본 이메일
  password_hash VARCHAR(255) NOT NULL,
  role         ENUM('user','admin') DEFAULT 'user',
  created_at   DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 비밀번호 이력 (재사용 방지)
CREATE TABLE password_history (
  id        BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id   BIGINT REFERENCES users(id),
  hash      VARCHAR(255) NOT NULL,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 로그인 시도 (Rate Limiting — Redis 대신 MySQL)
CREATE TABLE login_attempts (
  id           BIGINT AUTO_INCREMENT PRIMARY KEY,
  email        VARCHAR(255),
  ip           VARCHAR(45),
  success      BOOLEAN,
  attempted_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_email_ip (email, ip),
  INDEX idx_attempted_at (attempted_at)
);
-- 오래된 레코드 주기적 삭제 필요 (cron으로 24시간 이상 성공 기록 삭제)

-- 이메일 인증 코드
CREATE TABLE email_verifications (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  email      VARCHAR(255) NOT NULL,
  code       CHAR(6) NOT NULL,
  purpose    ENUM('register','password_reset') NOT NULL,
  expires_at DATETIME NOT NULL,              -- 발급 후 5분
  used       BOOLEAN DEFAULT FALSE,
  created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
  INDEX idx_email_code (email, code)
);

-- 디바이스 타입
CREATE TABLE device_types (
  type_id      VARCHAR(50) PRIMARY KEY,
  name         VARCHAR(100) NOT NULL,
  capabilities JSON NOT NULL,
  is_builtin   BOOLEAN DEFAULT FALSE,
  created_at   DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 등록 기기
CREATE TABLE devices (
  id           VARCHAR(50) PRIMARY KEY,   -- SCA_S, SCA_Y, ...
  type_id      VARCHAR(50) REFERENCES device_types(type_id),
  user_id      BIGINT REFERENCES users(id),
  name         VARCHAR(100),
  local_ip     VARCHAR(45),               -- 마지막 로컬 IP
  last_seen    DATETIME,
  registered_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- Capability별 설정
CREATE TABLE device_capability_config (
  device_id  VARCHAR(50),
  capability VARCHAR(50),
  config     JSON,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  PRIMARY KEY (device_id, capability)
);

-- 센서 텔레메트리 (파티셔닝 적용)
CREATE TABLE telemetry (
  id          BIGINT AUTO_INCREMENT,
  device_id   VARCHAR(50),
  recorded_at DATETIME NOT NULL,
  payload     JSON NOT NULL,
  PRIMARY KEY (id, recorded_at)
)
PARTITION BY RANGE (YEAR(recorded_at) * 100 + MONTH(recorded_at)) (
  PARTITION p202611 VALUES LESS THAN (202612),
  PARTITION p202612 VALUES LESS THAN (202701),
  PARTITION p202701 VALUES LESS THAN (202702),
  PARTITION pmax    VALUES LESS THAN MAXVALUE
);

-- 문의
CREATE TABLE inquiries (
  id          BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id     BIGINT REFERENCES users(id),
  title       VARCHAR(200) NOT NULL,
  content     LONGTEXT NOT NULL,           -- TipTap JSON/HTML
  status      ENUM('open','answered','closed') DEFAULT 'open',
  created_at  DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 문의 답변
CREATE TABLE inquiry_replies (
  id          BIGINT AUTO_INCREMENT PRIMARY KEY,
  inquiry_id  BIGINT REFERENCES inquiries(id),
  admin_id    BIGINT REFERENCES users(id),
  content     LONGTEXT NOT NULL,
  created_at  DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 알림 규칙
CREATE TABLE notification_rules (
  id          INT AUTO_INCREMENT PRIMARY KEY,
  capability  VARCHAR(50),
  field       VARCHAR(50) NOT NULL,
  operator    VARCHAR(5) NOT NULL,
  threshold   DECIMAL(10,2) NOT NULL,
  channels    JSON DEFAULT '["push"]',
  message_tpl TEXT,
  cooldown_min INT DEFAULT 10,
  is_active   BOOLEAN DEFAULT TRUE
);

-- Capability 마스터
CREATE TABLE capability_definitions (
  cap_id         VARCHAR(50) PRIMARY KEY,
  display_name   VARCHAR(100) NOT NULL,
  icon           VARCHAR(10),
  component_name VARCHAR(100) NOT NULL,
  mqtt_action    VARCHAR(50),
  telemetry_keys JSON,
  param_schema   JSON,
  is_active      BOOLEAN DEFAULT TRUE
);

-- 펌웨어 버전
CREATE TABLE firmware_versions (
  version     VARCHAR(20) PRIMARY KEY,
  device_type VARCHAR(50),
  file_url    TEXT NOT NULL,
  changelog   TEXT,
  is_stable   BOOLEAN DEFAULT FALSE,
  released_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
```

---

## 11. API 구조

### 사용자 API
| Method | Path | 설명 |
|---|---|---|
| POST | `/auth/register` | 회원가입 (이메일 인증 코드 발송) |
| POST | `/auth/verify-email` | 이메일 6자리 코드 인증 (5분 만료) |
| POST | `/auth/login` | 로그인 (JWT HttpOnly 쿠키 발급) |
| POST | `/auth/logout` | 로그아웃 |
| POST | `/auth/refresh` | 토큰 갱신 |
| POST | `/auth/password-reset` | 비밀번호 재설정 메일 발송 |
| POST | `/auth/password-reset/confirm` | **[추가]** 코드 + 새 비밀번호로 재설정 |
| GET | `/auth/me` | **[추가]** 현재 로그인 사용자 |
| POST | `/auth/password-change` | **[추가]** 비밀번호 변경 (프로필) |
| GET | `/capabilities` | **[추가]** Capability 정의 목록 |
| GET | `/devices` | 내 기기 목록 |
| POST | `/devices/pairing-token` | **[추가]** AP 설정용 계정 연동 토큰 발급 (10분) |
| POST | `/devices` | 기기 등록 — **ESP32가 pairing_token으로 직접 호출**, `local_key` 반환 |
| PATCH / DELETE | `/devices/{id}` | **[추가]** 이름 변경 / 계정에서 해제 |
| GET | `/devices/{id}` | 기기 상태 |
| POST | `/devices/{id}/command` | 기기 명령 |
| GET | `/devices/{id}/history` | 센서 히스토리 |
| GET | `/ws/devices/{id}` | WebSocket — 실시간 텔레메트리 |
| POST | `/inquiry` | 문의 작성 |
| GET | `/inquiry` | 내 문의 목록 |
| GET | `/inquiry/{id}` | **[추가]** 문의 상세 + 답변 |
| GET | `/notifications` | **[추가]** 알림 목록 |
| POST | `/notifications/{id}/read`, `/notifications/read-all` | **[추가]** 읽음 처리 |
| POST | `/me/fcm-tokens` | **[추가]** FCM 토큰 등록 (로그아웃 시 `X-FCM-Token` 헤더로 해당 토큰만 삭제) |

### 관리자 API (`/admin/*` — IP allowlist + role:admin)
| Method | Path | 설명 |
|---|---|---|
| GET | `/admin/devices` | 전체 기기 목록 |
| GET | `/admin/users` | 회원 목록 |
| PATCH | `/admin/users/{id}/role` | 권한 변경 |
| GET | `/admin/inquiry` | 전체 문의 목록 |
| POST | `/admin/inquiry/{id}/reply` | 문의 답변 |
| GET/POST/PUT | `/admin/device-types` | 타입 CRUD |
| GET/POST/PUT | `/admin/capabilities` | Capability CRUD |
| POST | `/admin/firmware` | 펌웨어 업로드 (SHA-256 계산·저장) |
| POST | `/admin/firmware/{ver}/deploy` | OTA 배포 (body: `device_type`, 선택 `device_ids`) |
| GET | `/admin/logs?kind=audit\|mqtt\|login` | 시스템 로그 |
| GET | `/admin/stats` | **[추가]** 대시보드 통계 |
| GET / PATCH | `/admin/inquiry/{id}`, `/admin/inquiry/{id}/status` | **[추가]** 문의 상세 / 상태 변경 |
| GET/POST/PUT/DELETE | `/admin/notification-rules` | **[추가]** 알림 규칙 CRUD |

---

## 12. 개발 우선순위

### Phase 1 — 인증·기반
- [ ] MySQL 스키마 생성 + Alembic 마이그레이션
- [ ] JWT HttpOnly 쿠키 + CSRF Double Submit
- [ ] 비밀키 32자 강제
- [ ] Rate Limiting (MySQL login_attempts 테이블 기반)
- [ ] Gmail 별칭 차단, 비밀번호 정책 (연속문자·재사용 포함)
- [ ] 이메일 인증 (Gmail SMTP 6자리 코드, 5분 만료)
- [ ] 회원가입·로그인·로그아웃·비밀번호 재설정 API

### Phase 2 — 기기 연동
- [ ] AP 모드 WiFi 초기 설정 페이지
- [ ] Mosquitto TLS 클라이언트 인증 설정 (CA·서버·기기별 인증서)
- [ ] MQTT 브로커 연동 (SCA_S / SCA_Y 토픽 통일)
- [ ] 텔레메트리 수신·저장 (파티셔닝 + 1년 보존 cron)
- [ ] WebSocket 실시간 푸시 (FastAPI → Flutter/React)
- [ ] 로컬/클라우드 모드 자동 전환 (Flutter)

### Phase 3 — UI
- [ ] Capability 동적 렌더링
- [ ] SCA_S / SCA_Y 각 Capability 카드 컴포넌트
- [ ] 센서 히스토리 차트
- [ ] 문의 작성·답변 (TipTap 3)

### Phase 4 — 관리자·인프라
- [ ] 관리자 페이지 (IP allowlist + role guard)
- [ ] Docker Compose 구성
- [ ] Nginx 서브도메인·보안 설정
- [ ] Let's Encrypt cron 자동갱신
- [ ] 스캔 경로 차단

### Phase 5 — 고도화
- [ ] OTA 펌웨어 배포
- [ ] 알림 규칙 설정 (관리자)
- [ ] 멀티 디바이스 비교 뷰
- [ ] 감사 로그 (Audit Log)

---

## 13. 확정 결정 사항

| 항목 | 결정 |
|---|---|
| Rate Limiting 저장소 | MySQL 테이블 (`login_attempts`) |
| 실시간 센서 푸시 | WebSocket (양방향, FastAPI WebSocket 엔드포인트) + **Redis pub/sub** (워커 간 공유) |
| 모바일 앱 | Flutter 3.50 (Riverpod 2.x), ~~Android 최소 API 21 (Android 5.0)~~ **Android 최소 API 24 (Android 7.0)** — Flutter 3.50은 23 미만 빌드 에러 |
| 푸시 알림 | **Firebase Cloud Messaging (FCM)** — 앱 백그라운드·종료 시도 수신 |
| 텔레메트리 보존 기간 | **1년** — 매월 1일 cron으로 13개월 이전 파티션 DROP |
| 텔레메트리 파티션 사전 생성 | 매월 1일 cron으로 다음 달 파티션 미리 생성 |
| 이메일 인증 | Gmail SMTP 6자리 코드, 5분 만료 |
| MQTT 인증 | TLS 클라이언트 인증 (포트 8883, 클라이언트 인증서 필수) |
| cafe24 VPS 방화벽 | 포트 8883 오픈 확인 (1883은 내부망만) |
| **실제 도메인** | `smart-clean-ashtray.com` (하이픈, 언더바 도메인 불가) |

### WebSocket 구조

```
Flutter 앱 / React 웹
  ──WSS──▶ api.smart-clean-ashtray.com/ws/devices/{id}
              │
              FastAPI WebSocket 핸들러
              │  (MQTT 수신 → WS 브로드캐스트)
              ▼
           ESP32 텔레메트리 실시간 수신
```

```python
# FastAPI WebSocket 개념
@app.websocket("/ws/devices/{device_id}")
async def ws_device(websocket: WebSocket, device_id: str):
    await manager.connect(websocket, device_id)
    try:
        while True:
            # MQTT 메시지를 받아 연결된 클라이언트에게 푸시
            await websocket.receive_text()  # 클라이언트 ping 유지
    except WebSocketDisconnect:
        manager.disconnect(websocket, device_id)
```

### FCM 푸시 알림 구조

```
ESP32 → MQTT → FastAPI
                 │
                 ├─ WebSocket → 앱(포그라운드) 실시간 표시
                 └─ FCM       → 앱(백그라운드·종료) 푸시 알림
```

화재 감지·잔량 경고처럼 **앱이 꺼져 있어도 알림이 가야 하는 이벤트**에 FCM 사용.

```python
# FastAPI — FCM 발송 (firebase-admin SDK)
import firebase_admin
from firebase_admin import messaging

def send_fcm(token: str, title: str, body: str, data: dict):
    message = messaging.Message(
        notification=messaging.Notification(title=title, body=body),
        data=data,
        token=token,
    )
    messaging.send(message)
```

```dart
// Flutter — FCM 토큰 등록 및 수신
// pubspec: firebase_messaging, flutter_local_notifications
FirebaseMessaging.instance.getToken().then((token) {
  // 로그인 후 서버에 FCM 토큰 저장
  api.saveFcmToken(token);
});

FirebaseMessaging.onMessage.listen((msg) {
  // 포그라운드: 로컬 알림으로 표시
});
```

DB에 FCM 토큰 저장:
```sql
ALTER TABLE users ADD COLUMN fcm_token TEXT;
-- 또는 기기별 다중 토큰 허용 시 별도 테이블
CREATE TABLE fcm_tokens (
  id         BIGINT AUTO_INCREMENT PRIMARY KEY,
  user_id    BIGINT REFERENCES users(id),
  token      TEXT NOT NULL,
  device_os  ENUM('android','ios') NOT NULL,
  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP
);
```

### 텔레메트리 1년 보존 — 파티션 자동화 cron (삭제 + 사전 생성)

> **[변경]** 아래 초안의 `ADD PARTITION IF NOT EXISTS` / `DROP PARTITION IF EXISTS` 는 **MySQL에 없는 문법**.
> 또한 `pmax`(MAXVALUE) 파티션이 있으면 `ADD PARTITION` 자체가 불가 →
> 구현은 `information_schema.PARTITIONS` 로 존재 여부를 조회한 뒤
> `ALTER TABLE telemetry REORGANIZE PARTITION pmax INTO (PARTITION pYYYYMM ..., PARTITION pmax ...)` 로 분할.
> 이번 달 파티션 누락 시 함께 생성. 13개월 전 이하 파티션 DROP.

```bash
# crontab -e (매월 1일 새벽 2시)
# 1) 13개월 이전 파티션 DROP
# 2) 다음 달 파티션 미리 생성
0 2 1 * * docker compose -f /home/ubuntu/app/docker-compose.yml \
  exec -T api python scripts/manage_partitions.py
```

```python
# scripts/manage_partitions.py
from datetime import datetime
from dateutil.relativedelta import relativedelta

def manage_partitions(conn):
    now = datetime.now()

    # 1. 다음 달 파티션 생성 (없으면)
    next_month = now + relativedelta(months=1)
    pname = f"p{next_month.year}{next_month.month:02d}"
    limit = (next_month + relativedelta(months=1)).strftime("%Y%m")
    conn.execute(f"""
        ALTER TABLE telemetry ADD PARTITION IF NOT EXISTS
        (PARTITION {pname} VALUES LESS THAN ({limit}))
    """)

    # 2. 13개월 이전 파티션 DROP
    old_month = now - relativedelta(months=13)
    old_pname = f"p{old_month.year}{old_month.month:02d}"
    conn.execute(f"ALTER TABLE telemetry DROP PARTITION IF EXISTS {old_pname}")
```

### MQTT TLS 클라이언트 인증 구조

```
CA 인증서 (mosquitto_ca.crt)
  ├─ 서버 인증서 (mosquitto_server.crt) → Mosquitto 브로커
  └─ 클라이언트 인증서 (sca_s.crt / sca_y.crt) → 각 ESP32

ESP32 펌웨어에 클라이언트 인증서·키 포함 (SPIFFS 또는 하드코딩)
연결 시 상호 인증 → 미인증 클라이언트 연결 차단
```

```ini
# mosquitto.conf
listener 8883
cafile   /mosquitto/certs/ca.crt
certfile /mosquitto/certs/server.crt
keyfile  /mosquitto/certs/server.key
require_certificate true   # 클라이언트 인증서 필수
```

> **[추가]** 구현 설정(`mosquitto/config/`):
> - `per_listener_settings true` — 1883(내부, FastAPI 전용)과 8883(외부, 기기)을 분리 설정
> - `use_identity_as_username true` + ACL `pattern write ashtray/%u/telemetry` 등 →
>   인증서 CN(`SCA_S`)으로 **자기 토픽만** 발행/구독 가능 (다른 기기 사칭 차단)
> - 서버 인증서에 SAN(`DNS:mqtt.smart-clean-ashtray.com`) 포함 — 최신 TLS 클라이언트는 CN만으로 호스트 검증 안 함
> - FastAPI는 `$share/sca-api/...` **공유 구독** — API 워커가 여러 개여도 메시지 1건을 1개 워커만 처리(DB 중복 저장 방지)

---

## 14. 확정 결정 (2026-11-25)

| 항목 | 확정 내용 |
|---|---|
| TLS 인증서 | openssl self-signed CA로 직접 발급 |
| SCA_S GPIO 충돌 | 탈취 펌프 → GPIO16, 서보 → GPIO17 재배정 |
| iOS 최소 버전 | ~~iOS 14~~ **iOS 15** (Flutter 3.50 최소 요구 — 16장 참고) |
| FCM 다중 디바이스 | `fcm_tokens` 별도 테이블, 멀티캐스트 발송 |

---

### ① TLS 인증서 발급 절차

**제안: openssl로 직접 발급 (self-signed CA)**

```bash
# 1. CA 키·인증서 생성
openssl genrsa -out ca.key 4096
openssl req -new -x509 -days 3650 -key ca.key -out ca.crt \
  -subj "/CN=SCA-MQTT-CA"

# 2. 서버(Mosquitto) 인증서
openssl genrsa -out server.key 2048
openssl req -new -key server.key -out server.csr \
  -subj "/CN=mqtt.smart-clean-ashtray.com"
openssl x509 -req -days 3650 -in server.csr \
  -CA ca.crt -CAkey ca.key -CAcreateserial -out server.crt

# 3. ESP32 클라이언트 인증서 (기기별 1개씩)
openssl genrsa -out sca_s.key 2048
openssl req -new -key sca_s.key -out sca_s.csr -subj "/CN=SCA_S"
openssl x509 -req -days 3650 -in sca_s.csr \
  -CA ca.crt -CAkey ca.key -CAcreateserial -out sca_s.crt

# SCA_Y도 동일하게 진행
```

인증서 파일 배치:
```
mosquitto/certs/ca.crt        → Mosquitto + ESP32 양쪽 보유
mosquitto/certs/server.crt    → Mosquitto
mosquitto/certs/server.key    → Mosquitto
firmware/certs/sca_s.crt      → SCA_S ESP32 SPIFFS
firmware/certs/sca_s.key      → SCA_S ESP32 SPIFFS
firmware/certs/sca_y.crt      → SCA_Y ESP32 SPIFFS
firmware/certs/sca_y.key      → SCA_Y ESP32 SPIFFS
```
> ⚠️ `ca.key`는 절대 레포에 커밋 금지 — `.gitignore`에 `*.key` 추가

---

### ② SCA_S GPIO 충돌 (GPIO21/22)

**제안: 핀 재배정**

| 역할 | 현재 | 변경 |
|---|---|---|
| OLED SDA | GPIO21 | GPIO21 (유지) |
| OLED SCL | GPIO22 | GPIO22 (유지) |
| 탈취 펌프 | GPIO21 → **충돌** | **GPIO16** |
| 서보 PWM | GPIO22 → **충돌** | **GPIO17** |

OLED I2C를 우선 유지하고 펌프·서보를 비어 있는 GPIO16·17로 이동.  
펌웨어 수정 범위: `hal_actuator.cpp` 핀 define 2줄 변경.

---

### ③ iOS 최소 지원 버전

**제안: iOS 14**

| 이유 | 내용 |
|---|---|
| Firebase FCM | iOS 13 이상 지원 |
| Flutter 3.x | iOS 12 이상 지원 |
| 시장 점유율 | iOS 14+ = 전체 iOS 기기의 ~95% |
| BLE/WiFi API | iOS 14부터 로컬 네트워크 접근 권한 명시 필요 |

→ ~~**iOS 14** 로 결정 권장.~~  
→ **[변경] iOS 15** — Flutter 3.50은 배포 타깃 15 미만 프로젝트를 15.0으로 자동 상향(마이그레이션)하므로 iOS 14 지원 불가.

---

### ④ FCM 다중 디바이스

**제안: `fcm_tokens` 별도 테이블 (이미 스키마에 포함)**

```
users (1) ──── fcm_tokens (N)
```

- 같은 계정으로 폰 A·B 로그인 시 토큰 2개 저장
- 알림 발송 시 해당 user_id의 모든 토큰에 FCM 멀티캐스트
- 로그아웃 시 해당 토큰만 삭제

```python
# 멀티캐스트 발송
def send_fcm_to_user(user_id: int, title: str, body: str):
    tokens = db.query("SELECT token FROM fcm_tokens WHERE user_id = ?", user_id)
    if not tokens: return
    message = messaging.MulticastMessage(
        notification=messaging.Notification(title=title, body=body),
        tokens=[t.token for t in tokens],
    )
    messaging.send_each_for_multicast(message)
```

---

## 15. 레포 구조 (yerin040821 레포 방식 동일 적용)

### 브랜치 전략
```
main       최종 안정 버전 (발표·배포용)
develop    다음 버전 통합 브랜치
feature/*  기능별 개발 브랜치
```
머지: `feature/*` → `develop` PR → 최소 2명 승인 → 머지

### 커밋 컨벤션
| 태그 | 사용 예 |
|---|---|
| `feat` | `feat: FCM 푸시 알림 발송 로직 구현` |
| `fix` | `fix: 로그인 Rate Limiting 카운터 초기화 오류` |
| `design` | `design: 디바이스 카드 UI 레이아웃 수정` |
| `docs` | `docs: API 엔드포인트 명세 추가` |
| `refactor` | `refactor: Capability 렌더링 컴포넌트 분리` |
| `chore` | `chore: docker-compose.yml MySQL 설정 추가` |
| `test` | `test: 로그인 Rate Limiting 단위 테스트` |

### 폴더 구조 (모노레포)
```
smart-clean-ashtray-platform/
├── frontend/          React 18 + Vite (사용자·관리자 통합 빌드)
├── mobile/            Flutter 앱
├── backend/           FastAPI
│   └── scripts/       manage_partitions.py 등 cron 스크립트
├── firmware/
│   ├── SCA_S/         halong61 재떨이 (PlatformIO)
│   └── SCA_Y/         yerin040821 재떨이 (PlatformIO)
├── mosquitto/
│   ├── config/        mosquitto.conf, acl
│   └── certs/         TLS 인증서 (*.key는 .gitignore)
├── nginx/
│   ├── conf.d/        서브도메인별 설정
│   └── snippets/      ssl, 보안 헤더·스캔 차단, 관리자 allowlist
├── scripts/           gen_certs.sh, init_letsencrypt.sh, crontab.txt
├── docs/              설계 문서, protocol.md (기기↔서버 프로토콜)
├── docker-compose.yml
└── .gitignore
```

### .gitignore 필수 항목
```gitignore
# 인증서 키 (절대 커밋 금지)
*.key
*.pem

# 환경변수
.env
.env.*
!.env.example

# 빌드 결과물
frontend/dist/
mobile/build/
__pycache__/
*.pyc
.pio/

# IDE
.vscode/settings.json
.idea/
```

### 개발 중 주의
- cafe24 VPS Docker 메모리 최소 1GB 권장 (MySQL + FastAPI + Mosquitto + Nginx + Redis) — API 워커는 기본 2개(`WEB_CONCURRENCY`)
- 5GHz WiFi ESP32-S3 미지원 → 반드시 2.4GHz 공유기
- MySQL JSON 컬럼 인덱스 미지원 → Capability 조회는 generated column + index 고려
- Flutter WebSocket 재연결 로직 필수 (앱 백그라운드 진입 시 연결 끊김)
- iOS 로컬 네트워크 접근 → `Info.plist`에 `NSLocalNetworkUsageDescription` 추가 필수

---

---

## 전체 확정 사항 요약

| 항목 | 확정값 |
|---|---|
| 도메인 | `smart-clean-ashtray.com` |
| 사용자 웹 | `www.smart-clean-ashtray.com` |
| 관리자 웹 | `admin.smart-clean-ashtray.com` (IP allowlist) |
| API | `api.smart-clean-ashtray.com` |
| MQTT | `mqtt.smart-clean-ashtray.com` (포트 8883 TLS) |
| DB | MySQL 8, 단일 DB |
| 백엔드 | FastAPI + SQLAlchemy async + **asyncmy** |
| 프론트 | React 18 + Vite + TypeScript + Tailwind CSS |
| 에디터 | TipTap 3 (JSON 저장) |
| 모바일 | Flutter 3.50 + Riverpod 2.x (**Android API 24+, iOS 15+**) |
| 푸시 알림 | Firebase FCM + `fcm_tokens` 테이블 |
| 실시간 | WebSocket (`/ws/devices/{id}`) + **Redis pub/sub** |
| 캐시/메시징 | **Redis 7** (내부망 전용) |
| 인프라 | Docker Compose + cafe24 VPS |
| 무중단 배포 | `docker compose up -d --no-deps --build` |
| 인증서 | Let's Encrypt **SAN 인증서(www·admin·api)** + cron 자동갱신 |
| JWT | HttpOnly 쿠키 + CSRF Double Submit Cookie |
| Rate Limiting | MySQL `login_attempts` 테이블 |
| 이메일 인증 | Gmail SMTP 6자리 코드, 5분 만료 |
| MQTT 인증 | TLS 클라이언트 인증 (openssl self-signed CA) |
| 텔레메트리 보존 | 1년, 월 1일 cron 파티션 삭제+생성 자동화 |
| 기기 1 | SCA_S (halong61), 탈취 GPIO16, 서보 GPIO17 |
| 기기 2 | SCA_Y (yerin040821), AM2302, Float Switch |
| 레포 구조 | 모노레포, yerin040821 레포 컨벤션 동일 적용 |
| 로컬 모드 인증 | 기기별 `local_key` + `X-Local-Key` 헤더 (앱 전용) |

*설계 확정 완료. 개발 착수 시 각 섹션을 별도 문서로 분리 예정.*

---

## 16. 구현 반영 변경 이력 (2026-11-25)

1차 구현(`smart-clean-ashtray-platform/`) 과정에서 설계 초안대로는 동작하지 않거나 보안상 보강이 필요해 바꾼 항목.

### 16-1. 동작 불가 → 수정
| # | 설계 초안 | 변경 | 이유 | 관련 섹션 |
|---|---|---|---|---|
| 1 | FastAPI + **asyncpg** | **asyncmy** (SQLAlchemy async) | asyncpg는 PostgreSQL 전용 | 3 |
| 2 | Let's Encrypt **wildcard** + webroot | www·admin·api **SAN 인증서** | 와일드카드는 DNS-01만 가능, cafe24 DNS 자동화 불가 | 2 |
| 3 | `ADD/DROP PARTITION IF [NOT] EXISTS` | information_schema 조회 + `REORGANIZE pmax` | MySQL에 없는 문법, MAXVALUE 파티션 존재 시 ADD 불가 | 13 |
| 4 | 컬럼 인라인 `REFERENCES` | 명시적 `FOREIGN KEY` | MySQL은 인라인 REFERENCES 무시 | 10 |
| 5 | `channels JSON DEFAULT '["push"]'` | `DEFAULT (JSON_ARRAY('push'))` | JSON 리터럴 DEFAULT 불가 | 10 |
| 6 | Android API **21+** | Android API **24+** (7.0) | Flutter 3.50: minSdk 23 미만 빌드 에러, 24 미만 경고 | 3, 13 |
| 7 | iOS **14+** | iOS **15+** | Flutter 3.50: 15 미만 배포 타깃을 15.0으로 강제 상향 | 3, 14 ③ |
| 8 | compose에 certbot 서비스 없음 | certbot 서비스 추가 (`profiles: tools`) | 9-3 cron이 `run --rm certbot` 사용 | 9 |

### 16-2. 보안 보강
| # | 설계 초안 | 변경 | 이유 |
|---|---|---|---|
| 1 | FastAPI `X-Forwarded-For` 검증 | Nginx가 덮어쓰는 `X-Real-IP` 검증 | XFF 첫 값은 위조 가능 |
| 2 | admin 서브도메인에만 IP allowlist | api 서버 `location /admin/` 에도 allowlist | `api.*/admin/*` 경유 우회 차단 |
| 3 | compose `1883:1883` 공개 | 1883 미공개(내부망 전용) | 설계서 "1883은 내부망만"과 일치, 무인증 리스너 노출 방지 |
| 4 | Gmail 점(.) 제거를 모든 도메인에 | Gmail에만 적용 | 타 도메인 사용자 간 주소 충돌 |
| 5 | 문의 HTML 저장 | TipTap JSON 저장 + read-only 에디터 렌더링 | 저장형 XSS 방지 |
| 6 | (없음) | `users.token_version` | 비번 변경·권한 변경 시 기존 JWT 즉시 무효화 |
| 7 | (없음) | `users.email_verified` | 이메일 미인증 계정 로그인 차단 |
| 8 | (없음) | `devices.local_key` + `X-Local-Key` | 로컬 모드 HTTP 무인증 → 같은 WiFi 누구나 조작 가능 문제 |
| 9 | (없음) | Mosquitto ACL `ashtray/%u/...` | 기기 인증서로 다른 기기 토픽 사칭 차단 |
| 10 | (없음) | 로그인 실패 기록을 별도 트랜잭션으로 커밋 | 401 응답 시 롤백돼 Rate Limit이 무력화되는 문제 |
| 11 | (없음) | WebSocket Origin 검증 | Cross-Site WebSocket Hijacking 방지 |

### 16-3. 기능 추가
| # | 항목 | 이유 |
|---|---|---|
| 1 | **Redis 7** + MQTT `$share` 공유 구독 | API 다중 워커 — WS 실시간 전송 공유, MQTT 메시지 중복 처리 방지, 알림 쿨다운 원자적 잠금 (Redis 장애 시 DB 폴백) |
| 2 | `notifications`, `device_events`, `audit_logs` 테이블 | `/notifications` 화면, 관리자 MQTT 이벤트·감사 로그 화면 |
| 3 | `POST /devices/pairing-token` | AP 설정(7장)의 "계정 연동 토큰" 발급 주체 명확화 |
| 4 | 비번 재설정 확인·변경, `/auth/me` 등 API | 11장 참고 (**[추가]** 표시) |
| 5 | `docs/protocol.md` | 펌웨어가 지켜야 할 MQTT 페이로드·명령·로컬 API·등록 절차 |
| 6 | 펌웨어 OTA에 SHA-256 | 업로드 시 계산, 기기는 검증 실패 시 적용 금지 |

### 16-4. 남은 작업
- [x] 펌웨어 SCA_Y v1.1.0 — 토픽 통일, TLS 클라이언트 인증, AP 설정 포털 + 연동 토큰 등록 + `local_key`, 로컬 API·mDNS, SHA-256 검증 OTA, WS2812B 구현, 습도 전송 (ESP32-S3 빌드 성공 / 실기기 미검증) — `firmware/README.md`
- [x] 펌웨어 SCA_S v1.0.0 — 원본 저장소에 펌웨어가 없어 **설계서 사양으로 신규 작성**: 10-State FSM, 5-Task, PIR 딥슬립(60초 안전 재확인 웨이크), 서보 덮개, 펌프 3종, 소화수 수위 추정, SCA_Y와 동일한 네트워크 스택 (빌드 성공, FSM 단위 테스트 15개 통과 / 실기기 미검증)
- [x] SCA_S 핀 배치 — 별도 회로도 없음 → `firmware/SCA_S/README.md` 표를 **기본 배치로 확정** (2026-11-25)
- [x] SCA_S 소화수 — **수동 보충, 보충 버튼 없음**(2026-11-25 결정): 수위 추정은 참고용(전원 재시작 시 100%), 분사를 막지 않음. 추정 ≤10% → `water_ok=0` → 보충 알림. ERROR 상태는 가스 센서 고장으로 재정의
- [x] 공기질 — **SCA_Y 방식으로 통일**(2026-11-25 결정): ppm 보정 없이 `gas_diff`(적응형 기준선 대비 변화량)·`gas_raw` 사용. SCA_S도 같은 알고리즘·임계값(>50 흡연, <30 해소). capability `telemetry_keys`, 알림 규칙(`gas_diff > 50`), 웹/앱 공기질 카드·차트 변경
- [x] Firebase — 프로젝트 ID `smart-clean-ashtray-app-64ff5` 에 Android/iOS 앱 등록, `flutterfire configure` 완료 (`lib/firebase_options.dart`, `android/app/google-services.json`)
- [x] Firebase Admin 서비스 계정 키 → `secrets/firebase.json` (같은 프로젝트)
- [ ] (맥) `flutterfire configure` 재실행으로 `ios/Runner/GoogleService-Info.plist` 생성 + APNs 키 등록
- [ ] Android SDK 설치 후 APK 빌드 확인 / 맥에서 iOS 빌드·APNs 설정
- [ ] FCM: firebase_messaging 17의 신규 `register()`/`onRegistered`(설치 ID 방식)로 전환 검토 — 현재는 기존 토큰 방식 유지
- [ ] 실서버 통합 테스트 (MySQL·Mosquitto·Redis 연동)
