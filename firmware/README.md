# Firmware (PlatformIO, ESP32-S3)

| 폴더 | 원본 저장소 | 상태 |
|---|---|---|
| `SCA_Y/` | github.com/yerin040821/smart-clean-ashtray | ✅ 플랫폼 연동 수정 완료 (v1.1.0) |
| `SCA_S/` | (신규 작성) | ✅ 설계서 사양으로 새로 작성 (v1.0.0) — 상세는 [SCA_S/README.md](SCA_S/README.md) |
| `_reference/halong61-smart-ashtray/` | github.com/halong61/smart-ashtray | 참고용 clone (웹/백엔드/Flutter만 있고 펌웨어 없음) |

`SCA_Y/` 는 원본 저장소 clone(`.git` 포함)이라 그 폴더에서 브랜치를 만들어 PR을 올릴 수 있습니다.
`SCA_S/` 는 새 코드라 git 이력이 없습니다 — `halong61/smart-ashtray` 에 `firmware/` 로 올리거나 모노레포에 포함하세요.

## 시뮬레이션
보드 없이 VS Code **Wokwi** 시뮬레이터로 펌웨어를 실행할 수 있습니다 → [WOKWI.md](WOKWI.md)

## SCA_S 요약
설계서 사양(PIR 딥슬립, 서보 덮개, 플레임 센서, 소화수/탈취/청소 펌프 3개, 10-State FSM, 5-Task)대로 구현.
- ESP32-S3에는 GPIO22가 없어 OLED SCL은 GPIO47 (설계서의 21/22는 구형 ESP32 핀맵 기준)
- 탈취 펌프 GPIO16, 서보 GPIO17, PIR GPIO14 — 설계서 지정값 적용
- 공기질은 SCA_Y와 같은 알고리즘·키(`gas_raw`, `gas_diff`)·임계값(>50 흡연)
- 소화수는 수동 보충 — "수위 추정"(펌프 가동 시간)은 참고용, 보충 후 전원 재시작 시 100%, **분사를 막지 않음**
- 핀 배치: `SCA_S/README.md` 표가 기본 배치
- 딥슬립 중에도 60초마다 깨어 불꽃·연기 재확인

## SCA_Y 변경 요약 (v1.0 → v1.1.0)

| 영역 | 이전 | 변경 |
|---|---|---|
| WiFi | 코드에 SSID/PW 하드코딩 | NVS 저장 + **설정 AP** `AshtraySetup_SCA_Y` (http://192.168.4.1, 캡티브 포털) |
| 계정 등록 | 없음 | 연동 토큰으로 `POST https://api/devices` → `local_key` NVS 저장 |
| MQTT | `broker.hivemq.com:1883` 평문, 발행만 | `mqtt.smart-clean-ashtray.com:8883` **TLS 상호 인증**, 명령 구독, LWT |
| 토픽 | `ashtray/v1/telemetry`, `ashtray/v1/event` | `ashtray/SCA_Y/{telemetry,event,status,command}` |
| 텔레메트리 | `gas_raw, temp, ash_pct, water_empty…` | 프로토콜 키 `temperature, humidity, fire, pump_on, ash_level, water_ok, led_color, state` (+ `gas_raw, gas_diff`) |
| 원격 명령 | 없음 | `spray`, `buzzer`, `led`, `ota` |
| 로컬 API | 없음 | `GET /api/status`, `POST /api/command` (`X-Local-Key`) + mDNS `ashtray-sca_y.local` |
| OTA | 없음 | HTTPS 다운로드 + **SHA-256 검증 후에만 적용**, 화재 관련 상태에선 거부 |
| WS2812B | 함수 본문 비어 있음 (LED 미동작) | `neopixelWrite` 구현 — 상태별 색, IDLE일 때 사용자 색 |
| 습도 | 읽지만 버림 | 텔레메트리 `humidity` 포함 |
| 파티션 | 기본 | `default_8MB.csv` (OTA 2슬롯 + LittleFS) |

변경하지 않은 것: FSM 전이 로직(`task_state.c`), 센서 드라이버, 펄스 소화 알고리즘, 4-Task 우선순위.
화재 대응은 여전히 **기기 자율 동작**이며 네트워크가 없어도 그대로 동작합니다.

### 안전 규칙 (원격 명령)
- `spray`: 물 부족(ERROR·플로트 스위치)이면 거부, 분사 중 물이 떨어지면 즉시 중단, FIRE 펄스 루프 중엔 무시
- `ota`: FIRE_DETECTED / WARNING / SMOKING 상태에선 거부, SHA-256 불일치 시 기존 펌웨어 유지
- `led`: IDLE일 때만 사용자 색 표시 — 경보 상태 색은 항상 우선

## 처음 설치

```bash
# 플랫폼 루트에서 — MQTT 인증서 + API 루트 CA 생성, SCA_Y/data 에 자동 배치
./scripts/gen_certs.sh

cd firmware/SCA_Y
pio run -t uploadfs     # 인증서 (LittleFS)
pio run -t upload       # 펌웨어
pio device monitor
```
1. 웹 `/devices/setup` 에서 **연동 토큰** 발급 (10분 유효)
2. 휴대폰 WiFi → `AshtraySetup_SCA_Y` 연결 → 자동으로 뜨는 페이지(또는 http://192.168.4.1)
3. 집 WiFi(2.4GHz) + 토큰 입력 → 재부팅 후 자동 등록

WiFi 연결에 60초 이상 실패하면 설정 AP가 다시 켜집니다(기존 연결도 계속 재시도).

## 테스트
```bash
pio test -e native      # FSM 단위 테스트 (PC)
```
