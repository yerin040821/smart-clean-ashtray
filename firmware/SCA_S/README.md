# SCA_S Firmware (한승찬 재떨이)

ESP32-S3 DevKitC-1 · Arduino + FreeRTOS · **10-State FSM** · **5-Task** (설계서 §1-2)

원본 저장소(`halong61/smart-ashtray`)에 펌웨어가 없어 설계서 사양을 기준으로 새로 작성했습니다.
네트워크 계층(설정 AP·등록·MQTT/TLS·로컬 API·OTA)은 SCA_Y와 동일한 HAL을 사용합니다.

## 하드웨어 / 핀

| 부품 | GPIO | 비고 |
|---|---|---|
| MQ-135 AO | 4 | ADC1 (WiFi 사용 중 ADC2 불가). AO가 5V면 분압 |
| 플레임 센서 DO | 5 | 기본 active LOW (`PIN_FLAME_ACTIVE_LOW`) |
| HC-SR04 TRIG / ECHO | 8 / 9 | **ECHO 5V → 3.3V 분압 필수** |
| PIR (HC-SR501) | **14** | 설계서 지정. RTC GPIO → 딥슬립 깨우기 |
| 소화수 펌프 | 15 | MOSFET 게이트, **10kΩ 풀다운** |
| 탈취제 펌프 | **16** | 설계서 지정 (21 → 16) |
| 덮개 서보 | **17** | 설계서 지정 (22 → 17), 50Hz |
| 청소액 펌프 | 18 | MOSFET 게이트, 10kΩ 풀다운 |
| 부저 | 10 | 액티브 부저 |
| WS2812B | 38 | DevKitC-1 v1.1 보드 내장 LED와 같은 핀 |
| SSD1306 SDA / SCL | 21 / **47** | ⚠️ ESP32-S3에는 GPIO22가 없음 → 설계서의 "OLED GPIO22"는 구형 ESP32 기준 |

위 표가 SCA_S의 **기본 핀 배치**입니다 (별도 회로도가 없어 설계서 지정값 + ESP32-S3 제약 기준으로 확정).
배선을 바꿀 땐 `platformio.ini` 의 `build_flags` 에 `-D PIN_XXX=nn` 으로 덮어쓰면 됩니다 (목록: `hal/hal_pins.h`).

## 10-State FSM

| 상태 | 진입 조건 | 동작 |
|---|---|---|
| IDLE | 기본 | 덮개 닫힘, 초록 LED, 가스 기준선 학습 |
| OCCUPIED | PIR 감지 | **덮개 열림**, 청록 LED. 30초간 미감지 시 IDLE |
| SMOKING | gas_diff > 50 | 덮개 열림, 노랑 LED |
| DEODORIZING | 흡연 종료(gas_diff < 30) / 화재 진압 후 | 덮개 닫힘, **탈취제 1.5초 분사**, 5초 후 IDLE |
| ASH_FULL | 재 ≥ 80% | 주황 LED (70% 미만이면 해제) |
| CLEANING | `clean` 명령 / 24시간마다 | 덮개 닫힘, **청소액 3초 분사**, 8초 후 IDLE |
| WARNING | 불꽃 감지 또는 gas_diff ≥ 400 | 빨강 깜박임 + 비프, 덮개 닫힘 |
| FIRE_DETECTED | 불꽃 **1.5초 연속** | **덮개 닫힘(질식) + 0.5초 분사/1초 대기 반복 + 사이렌**. 10초간 불꽃 없으면 DEODORIZING |
| ERROR | 가스 센서 고장 (MQ-135 ADC 4095 포화 5회 연속) | 보라 LED, 서버 알림. **화재 대응은 계속 동작** |
| SLEEP | 10분간 아무도 없음 | 출력 OFF → 딥슬립 |

우선순위: **FIRE > WARNING > ERROR(센서 고장) > 일반**. 화재 대응은 네트워크와 무관하게 기기가 자율 수행합니다.
FSM은 하드웨어 의존 없는 순수 함수(`src/fsm.c`)이고, 단위 테스트 15개로 검증합니다.

## 5-Task

| Task | 우선순위 | 역할 |
|---|---|---|
| Task_State | 4 | FSM (500ms 스냅샷 → 출력 명령) |
| Task_Actuator | 3 | 펌프·덮개·부저·LED·OLED 단독 소유, 50ms 논블로킹 루프 |
| Task_Sensor | 2 | 500ms 샘플링, MQ-135 적응형 기준선(RTC 메모리 → 딥슬립 유지) |
| Task_Comm | 1 | 설정 AP, 등록, MQTT/TLS, 로컬 API, OTA |
| Task_Scheduler | 1 | 자동 절전, 24시간 자동 청소, 수위 추정값 저장 |

## 딥슬립 (PIR)
- IDLE + 10분간 사람 없음 → 마지막 텔레메트리와 `status: sleeping` 전송 → 딥슬립
- **PIR 감지 시 즉시 깨어남**, 그리고 **60초마다 타이머로 깨어나 불꽃·연기를 재확인** (꽁초가 남아 있을 수 있어서)
  → 이상 없으면 20초 뒤 다시 절전
- 절전 중 펌프 핀은 LOW로 **고정(hold)** — 게이트가 떠서 펌프가 켜지는 것 방지 (하드웨어 풀다운도 권장)
- `sleep {enable:false}` 로 자동 절전 끄기 (설정은 NVS에 저장).
  단, 이미 잠든 기기는 MQTT 명령을 받을 수 없어 다음 깨어날 때까지 반영되지 않습니다.

## 공기질 (SCA_Y와 동일)
- 첫 정상값으로 기준선 초기화 → **IDLE에서만** IIR로 기준선 갱신 (SCA_Y: 1초 α=0.1 / SCA_S: 0.5초 α=0.05, 같은 속도)
- `gas_diff = raw − baseline` → **> 50 흡연, < 30 해소** (SCA_Y와 같은 임계값)
- ADC 4095 포화 시 기준선 고정, 5회 연속이면 센서 고장(ERROR)
- 텔레메트리 키도 SCA_Y와 동일: `gas_raw`, `gas_diff` (ppm 아님). 웹/앱 공기질 카드·알림 규칙도 `gas_diff` 기준
- SCA_S만의 차이: 기준선을 RTC 메모리에 보관해 딥슬립 후에도 유지

## 소화수 — 수동 보충, 수위는 "추정"
수위 센서도 보충 버튼도 없으므로 기기는 물이 채워졌는지 알 수 없습니다. 그래서:
- 남은 양 = **소화수 펌프 가동 시간 × 유량**으로 계산 (NVS 저장, 딥슬립 후 유지)
- **물을 수동으로 채운 뒤 전원을 껐다 켜면 100%로 리셋** (딥슬립 깨어남은 리셋 안 됨)
- 추정 ≤ 10% → `water_ok=0` → 서버가 "소화수 부족 — 물을 보충해 주세요" 알림
- **추정값은 참고용이며 분사를 절대 막지 않습니다** — 추정이 0%여도 화재 시 소화 펌프는 동작
  (실제로 물을 채웠는데 추정값 때문에 소화가 막히는 상황 방지)
- 보정: `WATER_TANK_ML`(기본 500), `PUMP_FLOW_ML_PER_S`(기본 20) — 계량컵으로 측정 후 수정
- 탈취제·청소액 탱크 잔량은 추적하지 않습니다

## 원격 명령
`spray`, `deodorize`, `clean`, `door`, `sleep`, `led`, `ota` — 상세는 `include/app_protocol.h`.
- 화재 중엔 `spray` 무시(이미 자동 분사), 화재·경고·청소·탈취·절전 중엔 덮개 명령 무시
- OTA는 화재·경고·흡연·청소·탈취 중 거부, SHA-256 일치할 때만 적용

## 빌드 / 설치
```bash
# 플랫폼 루트: MQTT 인증서 + API 루트 CA → SCA_S/data 자동 배치
./scripts/gen_certs.sh

cd firmware/SCA_S
pio run -t uploadfs     # 인증서
pio run -t upload       # 펌웨어
pio device monitor
pio test -e native      # FSM 단위 테스트 (gcc 필요)
```
초기 설정은 SCA_Y와 같습니다: 웹에서 연동 토큰 발급 → 휴대폰으로 `AshtraySetup_SCA_S` 접속 → WiFi + 토큰 입력.
