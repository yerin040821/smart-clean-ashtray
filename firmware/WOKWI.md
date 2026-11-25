# Wokwi 펌웨어 시뮬레이션

실제 보드 없이 VS Code 안에서 **실제 펌웨어 바이너리**를 가상 ESP32-S3 + 부품 회로로 실행합니다.

> 실행 확인: SCA_S — 2026-11-25 VS Code Wokwi에서 시뮬레이터 실행 확인

| 파일 | 역할 |
|---|---|
| `SCA_S/diagram.json`, `SCA_Y/diagram.json` | 가상 회로 (부품·배선) — 펌웨어 `hal_pins.h` 핀과 동일 |
| `SCA_S/wokwi.toml`, `SCA_Y/wokwi.toml` | 실행할 펌웨어 경로 (`.pio/build/wokwi/`) |
| `platformio.ini` 의 `[env:wokwi]` | 시뮬레이션 전용 빌드 — 가상 WiFi `Wokwi-GUEST` 자동 접속 (SCA_S는 절전 진입 1분) |

## 처음 한 번
1. VS Code 확장 **Wokwi Simulator** 설치 (완료됨)
2. `F1` → **Wokwi: Request a New License** → 브라우저에서 무료 라이선스 발급 → VS Code로 자동 적용

## 실행
```bash
cd firmware/SCA_S        # 또는 SCA_Y
pio run -e wokwi         # 시뮬레이션용 빌드 (VS Code PlatformIO에서 env: wokwi 선택 후 Build 도 가능)
```
VS Code에서 `F1` → **Wokwi: Select Config File** → `firmware/SCA_S/wokwi.toml` 선택 → `F1` → **Wokwi: Start Simulator**
(또는 `firmware/SCA_S` 폴더 자체를 VS Code로 열면 설정 파일을 자동으로 찾습니다)

시리얼 로그는 시뮬레이터 아래 터미널에 나옵니다 (`[STATE] IDLE → OCCUPIED ...`).

## 실제 부품 ↔ 시뮬레이터 대체 부품

| 실제 | 시뮬레이터 | 조작 |
|---|---|---|
| MQ-135 가스 센서 | 가변저항 | 돌려서 연기 농도 변경 |
| 플레임 센서 (SCA_S) | 빨간 버튼 **FLAME** | 누르고 있기 (키보드 `f`) |
| 펌프 (MOSFET) | LED | 켜짐 = 펌프 동작 |
| 플로트 스위치 (SCA_Y) | 슬라이드 스위치 | 왼쪽/오른쪽으로 물 있음/없음 |
| PIR, HC-SR04, 서보, DHT22, WS2812B, 부저, OLED | 동일 부품 | 부품 클릭 → 슬라이더/버튼 |

## 시나리오 — SCA_S

| # | 조작 | 기대 결과 |
|---|---|---|
| 1 | 시작 | OLED `SCA_S IDLE`, LED 초록, 덮개(서보) 닫힘 |
| 2 | PIR 클릭(움직임 감지) | **OCCUPIED** — 서보가 열림, LED 청록. 30초간 움직임 없으면 닫힘 |
| 3 | 가스 가변저항을 조금 올림 | **SMOKING** — LED 노랑 (gas_diff > 50) |
| 4 | 가변저항을 원위치 | **DEODORIZING** — 덮개 닫힘, DEODORIZE LED 1.5초 → IDLE |
| 5 | FLAME 버튼 짧게 (1.5초 미만) | **WARNING** — 빨강 깜박임 + 비프, 5초 후 IDLE |
| 6 | FLAME 버튼 **1.5초 이상** 누름 | **FIRE_DETECTED** — 덮개 닫힘, EXTINGUISH LED 0.5초 켜짐/1초 꺼짐 반복, 사이렌, OLED `! FIRE !` |
| 7 | 버튼 뗌 | 10초 뒤 DEODORIZING → IDLE |
| 8 | HC-SR04 거리를 5cm 이하로 | **ASH_FULL** — LED 주황 |
| 9 | 가변저항을 끝까지 돌림 (ADC 4095) | 5회 연속 포화 → **ERROR**(센서 고장) — LED 보라 |
| 10 | 아무것도 안 하고 1분 | **SLEEP** → 딥슬립. PIR 클릭 또는 60초 후 깨어남 |

## 시나리오 — SCA_Y

| # | 조작 | 기대 결과 |
|---|---|---|
| 1 | 시작 | LED 초록, OLED 표시 |
| 2 | 가스 가변저항 올림 | **SMOKING** — LED 노랑 |
| 3 | 흡연 중 DHT22 온도를 25 → 60℃로 한 번에 올림 | 온도 상승률 > 2℃/s → **WARNING** (빨강 깜박임, 부저) |
| 4 | 60℃ 유지 3초 | **FIRE_DETECTED** — PUMP LED 0.5초/1초 펄스 분사, 사이렌 |
| 5 | 온도 40℃ 미만으로 | IDLE |
| 6 | 플로트 스위치 전환 | **ERROR** — LED 보라, 펌프 잠금 |
| 7 | HC-SR04 5cm 이하 | **ASH_FULL** |

## 시뮬레이션의 한계
- **서버 연결(MQTT TLS)은 안 됨**: 시뮬레이터 플래시에는 인증서(LittleFS)가 없어 `Certs: ... MISSING` 로그가 나오고 MQTT 접속을 건너뜁니다. WiFi(`Wokwi-GUEST`) 접속까지만 확인됩니다. 서버 연동은 별도(가상 기기 프로그램 / 실서버)로 확인합니다.
- **설정 AP(포털)**: 시뮬레이션 빌드는 가상 WiFi로 바로 접속하므로 AP 모드는 확인하지 않습니다.
- **SCA_Y OLED**: 실제 부품은 SH1106인데 Wokwi에는 SSD1306만 있어, 화면이 약간 밀리거나 깨져 보일 수 있습니다 (실기기 문제 아님).
- **MQ-135**: 실제 센서 특성(예열, 습도 영향)은 재현되지 않습니다. 가변저항 값 = ADC 값입니다.
- **펌프 유량·서보 각도·초음파 보정** 같은 기구 특성은 실기기에서 확인해야 합니다.
