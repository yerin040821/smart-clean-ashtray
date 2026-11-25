# Smart Clean Ashtray — 개발 현황 보고 (1차 구현)

> 작성일: 2026-11-25  
> 기준 설계서: `ashtray-app-spec.md` (16장 "구현 반영 변경 이력"에 상세 변경 내역)  
> 코드 위치: 모노레포 `smart-clean-ashtray-platform/`  
> 상태: **1차 구현 완료 · 단위/빌드 검증 완료 · 실서버·실기기 통합 검증 전**

---

## 1. 요약

| 항목 | 내용 |
|---|---|
| 목표 | ESP32-S3 스마트 재떨이 2종(SCA_S, SCA_Y)을 웹·앱으로 원격 모니터링·제어하는 통합 플랫폼 |
| 구현 범위 | 백엔드 API, 사용자/관리자 웹, 모바일 앱, 펌웨어 2종, 인프라(배포·보안·인증서) |
| 진척 | 설계서 12장 개발 우선순위 Phase 1~5 중 **코드 구현은 전 Phase 완료**, 검증은 단위·빌드 수준까지 |
| 핵심 성과 | ① 두 기기 통신 규격 통일(MQTT 토픽·텔레메트리 키·공기질 알고리즘) ② SCA_S 펌웨어 신규 개발 ③ TLS 상호 인증 등 보안 설계 반영 |
| 미완료 | 실서버 배포·통합 테스트, 실기기 동작 검증, iOS 빌드, 팀원 코드 리뷰(PR 승인 2명) |

---

## 2. 시스템 구성

```
[ESP32-S3: SCA_S / SCA_Y]
   │  MQTT over TLS 8883 (기기별 클라이언트 인증서)
   ▼
[Mosquitto] ──1883(내부)── [FastAPI ×2 워커] ── [MySQL 8]
                               │   └─ [Redis] (WebSocket 공유 · 알림 쿨다운)
                               ├── WebSocket ──▶ [React 웹 (www / admin)]
                               ├── REST/HTTPS ─▶ [Flutter 앱]
                               └── FCM ────────▶ (앱 푸시)
[Flutter 앱] ── 같은 WiFi일 때 HTTP 직접 제어 (X-Local-Key) ──▶ [ESP32]
[Nginx] 서브도메인 리버스 프록시 · TLS · 관리자 IP 허용 목록
```

| 영역 | 기술 |
|---|---|
| 백엔드 | FastAPI, SQLAlchemy async + asyncmy, Alembic, aiomqtt, Redis, firebase-admin |
| DB | MySQL 8 (텔레메트리 월별 파티션, 1년 보존) |
| 웹 | React 18 + Vite + TypeScript + Tailwind CSS, TipTap 3, Recharts |
| 앱 | Flutter 3.50 + Riverpod 2.x (Android 7.0/API 24+, iOS 15+) |
| 펌웨어 | ESP32-S3 DevKitC-1, Arduino + FreeRTOS, PlatformIO |
| 인프라 | Docker Compose, Nginx, Mosquitto 2, Let's Encrypt, cafe24 VPS |

---

## 3. 영역별 구현 현황

### 3-1. 백엔드 (FastAPI) — 26개 파일, 약 1,700줄

| 기능 | 상태 | 비고 |
|---|---|---|
| 회원가입 · 이메일 인증(6자리, 5분) · 로그인 · 로그아웃 · 토큰 갱신 | ✅ | Gmail SMTP (미설정 시 로그 출력) |
| 비밀번호 재설정 · 변경 | ✅ | 최근 3개 재사용 금지 |
| JWT HttpOnly 쿠키 + CSRF Double Submit | ✅ | access 15분 / refresh 14일 |
| 로그인 시도 제한 (이메일+IP 10회/15분, 이메일 50회/24h, IP 30회/15분) | ✅ | MySQL `login_attempts` |
| 비밀번호 정책 · Gmail 별칭 차단 · 비밀키 32자 강제 | ✅ | |
| 기기 등록(연동 토큰) · 조회 · 이름 변경 · 해제 | ✅ | 등록 시 `local_key` 발급 |
| 기기 명령 → MQTT 발행 (파라미터 검증) | ✅ | Capability 정의 기반 |
| 센서 히스토리 조회 | ✅ | |
| 실시간 WebSocket (`/ws/devices/{id}`) | ✅ | Redis pub/sub로 다중 워커 지원 |
| MQTT 수신 → 저장 → 알림 규칙 평가 → FCM 푸시 | ✅ | 공유 구독(`$share`)으로 중복 처리 방지 |
| 알림 목록 · 읽음 처리 · FCM 토큰(기기별 다중) | ✅ | |
| 문의 작성 · 조회 (TipTap JSON 저장) | ✅ | |
| 관리자 API (통계·기기·회원·권한·문의 답변·타입·Capability·알림 규칙·펌웨어 업로드/OTA·로그) | ✅ | IP 허용 목록 + 관리자 권한 이중 검증 |
| DB 스키마 마이그레이션 + 기본 데이터 | ✅ | 17개 테이블, 기기 타입 2·Capability 12·알림 규칙 4 |
| 운영 스크립트 (파티션 관리 · 데이터 정리 · 관리자 생성) | ✅ | cron 등록용 |

### 3-2. 웹 (React) — 25개 파일, 약 2,100줄

| 화면 | 상태 |
|---|---|
| 로그인 · 회원가입(이메일 인증) · 비밀번호 찾기 | ✅ |
| 홈 대시보드 (기기 카드, 화재 시 강조) | ✅ |
| 기기 등록 안내 (연동 토큰 발급 · AP 설정 절차) | ✅ |
| 기기 상세 — **Capability 동적 렌더링** (12종 카드, 실시간 갱신, 제어) | ✅ |
| 센서 히스토리 차트 (1시간/24시간/7일/30일) | ✅ |
| 알림 · 문의(TipTap 3 에디터) · 계정 설정 | ✅ |
| 관리자 9개 화면 (대시보드·기기·디바이스 타입·Capability·회원·문의·알림 규칙·펌웨어·로그) | ✅ |
| 목업 모드 (`npm run dev:mock`, 백엔드 없이 화면 검토) | ✅ |
| www/admin 한 빌드 분리 서빙, 관리자 라우트는 admin 호스트에서만 노출 | ✅ |

### 3-3. 모바일 앱 (Flutter) — 13개 파일, 약 1,000줄

| 기능 | 상태 | 비고 |
|---|---|---|
| 로그인 · 기기 목록 · 기기 상세 · 히스토리 차트 · 알림 | ✅ | 회원가입은 웹에서 |
| Capability `param_schema` 기반 제어 UI 자동 생성 | ✅ | |
| 로컬 우선(500ms) → 클라우드 자동 전환 | ✅ | 같은 WiFi면 기기에 직접 HTTP |
| WebSocket 자동 재연결 (백오프, 앱 복귀 시 즉시) | ✅ | |
| FCM 푸시 (Firebase 프로젝트 연결 완료) | ✅ | Android 설정 완료, iOS는 맥 작업 필요 |
| Android/iOS 플랫폼 설정 (권한·로컬 네트워크·알림 채널) | ✅ | |

### 3-4. 펌웨어 SCA_S (한승찬) — 신규 개발, 28개 파일, 약 2,900줄

원본 저장소(`halong61/smart-ashtray`)에 펌웨어가 없어 **설계서 사양으로 새로 작성**.

| 기능 | 상태 | 비고 |
|---|---|---|
| **10-State FSM** | ✅ | 대기 / 사람 감지 / 흡연 / 탈취 / 재 가득 / 청소 / 경고 / 화재 / 센서 고장 / 절전 |
| **FreeRTOS 5-Task** (상태 P4, 출력 P3, 센서 P2, 통신 P1, 스케줄러 P1) | ✅ | |
| 화재 대응: 불꽃 1.5초 확정 → 덮개 닫힘 + 0.5초 분사/1초 대기 반복 + 사이렌 | ✅ | 네트워크 없이 자율 동작 |
| PIR 사람 감지 → 덮개 자동 개폐 (서보) | ✅ | |
| 흡연 종료 후 탈취제 자동 분사, 24시간마다 자동 청소 | ✅ | |
| **PIR 딥슬립** + 60초마다 안전 재확인 웨이크 | ✅ | 남은 꽁초 대비 |
| 공기질 (SCA_Y와 동일 알고리즘) · 재 잔량(초음파) | ✅ | |
| 소화수 수위 추정 (펌프 가동 시간 기반, 참고용) | ✅ | 수동 보충 후 전원 재시작 시 100% |
| 통신 계층 (SCA_Y와 공용) | ✅ | 아래 3-5 참고 |

### 3-5. 펌웨어 SCA_Y (정예린) — 플랫폼 연동 수정 v1.0 → v1.1.0

| 항목 | 이전 | 변경 |
|---|---|---|
| WiFi 설정 | 코드에 SSID/PW 하드코딩 | **설정 AP + 캡티브 포털** (`AshtraySetup_SCA_Y`), NVS 저장 |
| 서버 등록 | 없음 | 연동 토큰으로 자동 등록, `local_key` 저장 |
| MQTT | 공개 브로커 평문 1883, 발행만 | **TLS 상호 인증 8883**, 명령 수신, 접속 상태(LWT) |
| 원격 명령 | 없음 | 분사·부저·LED·OTA |
| 로컬 API · mDNS | 없음 | `/api/status`, `/api/command` (X-Local-Key 인증) |
| OTA | 없음 | HTTPS + **SHA-256 검증 후에만 적용** |
| WS2812B LED | 함수 본문 비어 있어 **미동작** | 상태별 색상 구현 (버그 수정) |
| 습도 | 측정 후 폐기 | 텔레메트리 포함 |

변경하지 않은 것: 원작자의 FSM 전이 로직, 센서 드라이버, 펄스 소화 알고리즘, 4-Task 구조.

### 3-6. 인프라 · 보안

| 항목 | 상태 |
|---|---|
| Docker Compose (nginx·frontend·api·mosquitto·mysql·redis·certbot) | ✅ |
| Nginx 서브도메인(www/admin/api), 보안 헤더, 스캔 경로 차단, 관리자 IP 허용 목록 | ✅ |
| Mosquitto TLS 상호 인증 + 기기별 토픽 ACL (다른 기기 사칭 차단) | ✅ |
| 인증서 발급 스크립트 (MQTT 자체 CA, Let's Encrypt) · cron 자동 갱신 | ✅ |
| 비밀 파일 관리 (`secrets/`, `.env`, `*.key` 커밋 제외) | ✅ |

### 3-7. 펌웨어 시뮬레이션 (Wokwi) — 2026-11-25 추가

실제 보드 없이 VS Code의 **Wokwi 시뮬레이터**에서 **실제 펌웨어 바이너리**를 가상 ESP32-S3 + 부품 회로로 실행.

| 항목 | 내용 |
|---|---|
| 회로 | `firmware/SCA_S/diagram.json`(부품 12개), `firmware/SCA_Y/diagram.json`(부품 9개) — 핀은 `hal_pins.h`와 동일, Wokwi 공식 보드 정의로 핀 이름 검증 |
| 실행 설정 | 각 펌웨어 폴더의 `wokwi.toml` |
| 시뮬레이션 빌드 | `platformio.ini` 의 `[env:wokwi]` — 가상 WiFi `Wokwi-GUEST` 자동 접속, SCA_S 절전 진입 1분 (실제 기기용 빌드에는 영향 없음) |
| 대체 부품 | MQ-135 → 가변저항, 플레임 센서 → 버튼(FLAME), 펌프 → LED, 플로트 스위치 → 슬라이드 스위치 |
| 시나리오 | 사람 감지·흡연·탈취·화재(덮개 닫힘+펄스 분사+사이렌)·재 가득·센서 고장·절전 — `firmware/WOKWI.md` |
| 한계 | 서버(MQTT TLS) 연결 불가(시뮬레이터에 인증서 없음), 설정 AP 미확인, SCA_Y OLED는 SH1106 대신 SSD1306으로 표시, 센서 물리 특성 미재현 |

---

## 4. 검증 결과

### 4-1. 수행한 검증

| 대상 | 방법 | 결과 |
|---|---|---|
| 백엔드 | pytest 단위 테스트 (보안 로직·파티션·토픽 라우팅·Redis 다중 워커 시뮬레이션) | **21개 통과** |
| 백엔드 | 앱 로딩 · 라우트 등록 확인 | REST·WebSocket 엔드포인트 50여 개 등록 확인 |
| 웹 | TypeScript 타입 검사 + 운영 빌드 | 통과 (목업 코드 운영 빌드 미포함 확인) |
| 웹 | 목업 모드로 전 화면 브라우저 확인 | 화면 렌더링·이동 확인 |
| 앱 | `flutter analyze` + `flutter test` | 경고 0건, 테스트 통과 |
| SCA_S 펌웨어 | ESP32-S3 빌드 | 성공 — RAM 17.4%, Flash 35.6% |
| SCA_S 펌웨어 | FSM 단위 테스트 (Unity, PC 실행) | **15개 통과** (화재 우선순위, 수위 추정이 분사를 막지 않음 등) |
| SCA_Y 펌웨어 | ESP32-S3 빌드 | 성공 — RAM 17.3%, Flash 35.0% |
| SCA_Y 펌웨어 | 원작자 기존 단위 테스트 (FSM 7 + 알고리즘 3) | **10개 통과** (수정 후 회귀 없음) |
| SCA_S 펌웨어 | **Wokwi 시뮬레이터 실행** (VS Code) | 실행 확인 (2026-11-25, 담당자 직접 실행) |
| SCA_Y 펌웨어 | Wokwi 시뮬레이션 빌드 | 빌드 성공 — RAM 17.3%, Flash 35.0% (시뮬레이터 실행은 미확인) |

### 4-2. 아직 검증하지 않은 것 (결과보고서 작성 시 구분 필요)

| 항목 | 사유 |
|---|---|
| 실서버 통합 동작 (MySQL·Mosquitto·Redis 연동, 실제 로그인·명령·알림 흐름) | 개발 PC에 Docker 없음, VPS 배포 전 |
| 실기기 동작 (센서 값, 펌프·서보·LED, 딥슬립, OTA, TLS 접속) | 실제 보드·부품 미연결 (동작 흐름은 Wokwi 시뮬레이션으로 대체 확인, 센서 보정·기구 동작은 실기기 필요) |
| 시뮬레이터에서의 서버 연동 | 시뮬레이터 플래시에 TLS 인증서가 없어 MQTT 접속 생략 |
| Android APK 빌드 · 실기기 앱 실행 | Android SDK 미설치 |
| iOS 빌드 · APNs 푸시 | 맥 필요 |
| 웹 실서버 연동 | 화면은 목업 데이터로만 확인 |

---

## 5. 설계서 대비 주요 변경 (상세: 설계서 16장)

| 구분 | 설계 | 변경 | 사유 |
|---|---|---|---|
| DB 드라이버 | asyncpg | asyncmy | asyncpg는 PostgreSQL 전용 |
| 인증서 | 와일드카드 | www·admin·api SAN 인증서 | 와일드카드는 DNS 인증만 가능, cafe24 자동화 불가 |
| 파티션 SQL | `IF NOT EXISTS` | 존재 조회 + `REORGANIZE` | MySQL 미지원 문법 |
| 모바일 최소 버전 | Android 21 / iOS 14 | **Android 24 / iOS 15** | Flutter 3.50 최소 요구사항 |
| SCA_S OLED 핀 | GPIO21/22 | GPIO21/**47** | ESP32-S3에 GPIO22 없음 |
| 공기질 | ppm | **`gas_diff`** (기준선 대비 변화량) | 센서 보정 없음 → SCA_Y 방식으로 통일 |
| 실시간 처리 | 단일 프로세스 | **Redis + MQTT 공유 구독** | API 다중 워커 지원 |
| 로컬 모드 | 인증 없음 | 기기별 `local_key` | 같은 WiFi 사용자의 무단 조작 방지 |
| 관리자 보안 | X-Forwarded-For 검증 | X-Real-IP + api 서버에도 허용 목록 | 헤더 위조·우회 차단 |

---

## 6. 의사결정 기록 (2026-11-25)

| 결정 | 내용 |
|---|---|
| Firebase 프로젝트 | `smart-clean-ashtray-app-64ff5` 사용 (앱·서버 키 동일 프로젝트) |
| 공기질 기준 | SCA_Y 알고리즘으로 두 기기 통일 — `gas_diff` > 50 흡연, < 30 해소. 알림 규칙 `gas_diff > 50` (관리자 화면에서 조정 가능) |
| SCA_S 핀 배치 | 별도 회로도 없음 → 펌웨어 README의 표를 기본 배치로 확정 |
| SCA_S 소화수 | 보충 버튼 없이 **수동 보충**. 수위 추정은 참고용(분사를 막지 않음), 전원 재시작 시 100%, 10% 이하 보충 알림 |
| SCA_S 오류 상태 | 물 부족 대신 **가스 센서 고장**으로 정의 (화재 대응은 계속 동작) |
| 브랜치 전략 | 기능 개발은 `feature/*`, 완료 후 `develop`으로 PR, **팀원 2명 이상 승인 후 병합** |

---

## 7. 형상 관리

| 저장소 | 브랜치 | 내용 |
|---|---|---|
| `halong61/smart-ashtray` | `feature/platform` → `develop` **PR #1 병합 완료**, `main` 반영 | 통합 플랫폼 모노레포 — https://github.com/halong61/smart-ashtray/pull/1 |
| `halong61/smart-ashtray` | `feature/wokwi-simulation` → `develop` **PR #2** | Wokwi 펌웨어 시뮬레이션 + 본 보고서 갱신 |
| `yerin040821/smart-clean-ashtray` | `feature/platform` → `develop` **PR #1** (리뷰 대기) | 통합 플랫폼 모노레포 — https://github.com/yerin040821/smart-clean-ashtray/pull/1 |
| (공통) | `develop` | 2026-11-25 `main` 에서 생성 (PR 대상) |

- 비밀 파일(Firebase 서버 키, `.env`, 인증서 키)은 커밋 대상에서 제외
- 커밋 컨벤션: `feat` / `fix` / `docs` / `chore` 등 (설계서 15장)

---

## 8. 남은 과제

| 우선순위 | 과제 |
|---|---|
| 높음 | PR 코드 리뷰 및 팀원 2명 승인 → `develop` 병합 |
| 높음 | cafe24 VPS 배포, 실서버 통합 테스트 (로그인 → 기기 등록 → 텔레메트리 → 명령 → 알림) |
| 높음 | 실기기 검증: 센서 보정(초음파 거리, 서보 각도, 펌프 유량), 화재 시나리오, 딥슬립 |
| 중간 | Wokwi 시나리오 전체 점검 (SCA_S `WOKWI.md` 10개, SCA_Y 7개) 및 SCA_Y 시뮬레이터 실행 확인 |
| 중간 | Android APK 빌드·실기기 테스트, iOS 빌드 및 APNs 등록 (맥) |
| 중간 | 공기질 알림 임계값 현장 튜닝 (흡연마다 알림이 과다한지 확인) |
| 낮음 | FCM 신규 등록 방식(`register()`) 전환 검토, Firebase API 키 앱 제한 설정 |
| 낮음 | 멀티 디바이스 비교 뷰 (설계서 Phase 5) |

---

## 9. 리스크 · 제약

| 항목 | 내용 | 대응 |
|---|---|---|
| 소화수 수위 | 센서 없이 추정 → 실제와 다를 수 있음 | 참고용으로만 사용, 분사는 막지 않음. 보충 후 전원 재시작 운영 규칙 필요 |
| 딥슬립 중 화재 감지 공백 | 최대 60초 | 60초 주기 재확인 웨이크, 사람 감지 시 즉시 깨어남 |
| 공기질 절대값 부재 | ppm 보정 없음 | 상대 변화량 기준으로 판정, 필요 시 센서 보정 후 확장 |
| 서버 메모리 | VPS 1GB 권장 | Redis 64MB 제한, 워커 수 조정 가능 |

---

## 10. 실행 방법 (검토자용)

```bash
# 웹 화면만 확인 (백엔드 불필요)
cd frontend && npm install && npm run dev:mock        # http://localhost:5173 (관리자: /admin)

# 백엔드 단위 테스트
cd backend && pip install -r requirements.txt && pytest -q

# 펌웨어 빌드
cd firmware/SCA_S && pio run
cd firmware/SCA_Y && pio run

# 펌웨어 시뮬레이션 (Wokwi, firmware/WOKWI.md)
cd firmware/SCA_S && pio run -e wokwi     # VS Code: F1 → Wokwi: Select Config File → Start Simulator

# 전체 배포 (VPS)
docker compose up -d --build && docker compose exec api alembic upgrade head
```

## 11. 코드 규모

| 영역 | 파일 | 줄 수 |
|---|---|---|
| 백엔드 (app·scripts·alembic) | 32 | 약 2,100 |
| 웹 (src) | 25 | 약 2,100 |
| 앱 (lib) | 13 | 약 1,000 |
| 펌웨어 SCA_S | 28 | 약 2,900 |
| 펌웨어 SCA_Y (수정·추가 포함 전체) | 19 | 약 2,600 |
