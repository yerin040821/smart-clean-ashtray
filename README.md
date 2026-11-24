# Smart Clean Ashtray Platform

ESP32-S3 스마트 재떨이(SCA_S / SCA_Y) 원격 모니터링·제어 통합 플랫폼. 설계: [docs/ashtray-app-spec.md](docs/ashtray-app-spec.md) · 기기 프로토콜: [docs/protocol.md](docs/protocol.md)

```
frontend/   React 18 + Vite + TS + Tailwind + TipTap 3  (www / admin 통합 빌드)
mobile/     Flutter 3.50 + Riverpod 2.x  (Android 24+ / iOS 15+ → mobile/README.md)
backend/    FastAPI + SQLAlchemy(asyncmy) + Alembic + aiomqtt + Redis
firmware/   PlatformIO 레포 위치 (README 참고)
mosquitto/  브로커 설정 · ACL · TLS 인증서
nginx/      서브도메인 리버스 프록시 · 보안 스니펫
scripts/    인증서 발급 · Let's Encrypt · crontab
```

## 로컬 개발

```bash
# 1) MySQL + Mosquitto 만 도커로
cp .env.example .env            # SECRET_KEY 32자 이상으로 변경
docker compose up -d mysql mosquitto redis   # (개발 시 1883·6379 를 임시로 ports 에 추가)

# 2) 백엔드
cd backend
python -m venv .venv && . .venv/bin/activate   # Windows: .venv\Scripts\activate
pip install -r requirements.txt
export DATABASE_URL=mysql+asyncmy://sca:sca_password@localhost:3306/sca SECRET_KEY=... \
       REDIS_URL=redis://localhost:6379/0 COOKIE_SECURE=false COOKIE_DOMAIN= CORS_ORIGINS=http://localhost:5173 FIRMWARE_DIR=./data/firmware
alembic upgrade head
python scripts/create_admin.py admin@example.com 'Adm!n-pass7'
uvicorn app.main:app --reload
pytest -q                       # 단위 테스트 (DB 불필요)

# 3) 프론트
cd frontend && npm install && npm run dev   # http://localhost:5173  (/api → :8000 프록시)
                                            # 관리자: http://localhost:5173/admin
```
SMTP 미설정 시 인증 코드는 API 로그에 출력됩니다.

## 운영 배포 (cafe24 VPS)

```bash
./scripts/gen_certs.sh                       # MQTT CA·서버·기기 인증서
./scripts/init_letsencrypt.sh admin@your-mail  # www/admin/api SAN 인증서
docker compose up -d --build
docker compose exec api alembic upgrade head
docker compose exec api python scripts/create_admin.py <email> <password>
crontab scripts/crontab.txt                  # 인증서 갱신 · 파티션 · 정리
```
방화벽: 80, 443, 8883 오픈 / 1883·3306·6379 는 외부 차단.
관리자 IP 는 `nginx/snippets/admin-allowlist.conf` 와 `.env` 의 `ADMIN_ALLOWED_IPS` 두 곳을 함께 수정.

무중단 업데이트:
```bash
docker compose up -d --no-deps --build api
docker compose up -d --no-deps --build frontend
docker compose exec nginx nginx -s reload
```

## 설계서 대비 변경점

| 설계서 | 변경 | 이유 |
|---|---|---|
| FastAPI + **asyncpg** | **asyncmy** | asyncpg 는 PostgreSQL 전용 — MySQL 사용 불가 |
| Let's Encrypt **wildcard** + webroot | www/admin/api **SAN 인증서** | 와일드카드는 DNS-01 만 가능, cafe24 DNS 자동화 플러그인 없음 |
| `ADD/DROP PARTITION IF [NOT] EXISTS` | information_schema 조회 + `REORGANIZE pmax` | MySQL 에 없는 문법 |
| 인라인 `REFERENCES` | 명시적 `FOREIGN KEY` | MySQL 은 인라인 REFERENCES 를 무시 |
| `channels JSON DEFAULT '["push"]'` | `DEFAULT (JSON_ARRAY('push'))` | JSON 리터럴 DEFAULT 불가 |
| compose `1883:1883` 공개 | 내부망 전용 | 설계서 "1883은 내부망만" 과 일치 |
| Admin: `X-Forwarded-For` 검증 | Nginx가 덮어쓰는 `X-Real-IP` 검증 + api 서버의 `/admin/` 에도 allowlist | XFF 첫 값은 위조 가능 / admin 서브도메인만 막으면 api 경유 우회 |
| Gmail 점(.) 제거 전 도메인 적용 | gmail.com 에만 | 타 도메인에선 다른 사람 주소가 충돌 |
| 문의 HTML 저장 | TipTap **JSON** 저장 | 저장형 XSS 방지 |
| (없음) | `users.email_verified`, `token_version` | 미인증 로그인 차단 · 비번 변경/권한 변경 시 기존 JWT 무효화 |
| Android API 21+ / iOS 14+ | **Android API 24+ / iOS 15+** | Flutter 3.50 최소 요구사항 (21은 빌드 불가) |
| (없음) | `devices.local_key` | 로컬 모드 HTTP 무인증 방지 |
| (없음) | `notifications`, `device_events`, `audit_logs` 테이블 | 알림 목록 · MQTT 로그 · 감사 로그 화면용 |
| (없음) | **Redis** (pub/sub · 쿨다운 잠금) + MQTT `$share` 공유 구독 | API 다중 워커 — 실시간 전송 공유, 메시지 중복 처리·중복 푸시 방지 |

## 알려진 제약
- Redis 는 휘발성(영속화 끔) — 재시작 시 쿨다운 잠금만 초기화되며 데이터 손실 없음
- 펌웨어 쪽 구현(토픽 통일, TLS, 로컬 API, OTA)은 각 기기 레포에서 작업 필요
- 모바일은 회원가입/비번 찾기 화면 없음 (웹 이용)
