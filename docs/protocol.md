# 기기 ↔ 서버 프로토콜 (SCA_S / SCA_Y 공통)

펌웨어가 지켜야 하는 계약. 백엔드 `app/services/mqtt_bridge.py`, `capability_definitions` 시드와 일치해야 함.

## 1. MQTT (mqtt.smart-clean-ashtray.com:8883, TLS 상호 인증)

- 클라이언트 인증서 CN = `device_id` (예: `SCA_S`). Mosquitto ACL이 `ashtray/{CN}/...` 만 허용
- client_id 권장: `device_id`
- LWT: `ashtray/{id}/status` ← `{"status":"offline"}` (retain)

| 토픽 | 방향 | QoS | 페이로드 |
|---|---|---|---|
| `ashtray/{id}/telemetry` | 기기→서버 | 0~1 | 아래 표의 키 (있는 것만). 5~10초 주기 권장 |
| `ashtray/{id}/event` | 기기→서버 | 1 | `{"from":"IDLE","to":"FIRE_DETECTED","reason":"flame"}` |
| `ashtray/{id}/status` | 기기→서버 | 1, retain | `{"status":"online","ip":"192.168.0.23","fw":"1.2.0"}` |
| `ashtray/{id}/command` | 서버→기기 | 1 | `{"req_id":"a1b2..","action":"spray","params":{"duration_ms":1000},"ts":1764000000}` |

### Telemetry 키 (capability `telemetry_keys`)
| 키 | 타입 | capability | SCA_S | SCA_Y |
|---|---|---|---|---|
| `gas_diff` | number | air_quality_monitor | MQ-135 적응형 기준선 대비 변화량 (>50 흡연, <30 해소) | 동일 |
| `gas_raw` | number | air_quality_monitor | MQ-135 ADC 원시값 | 동일 |
| `temperature`, `humidity` | number | temperature_humidity | — | AM2302 |
| `fire` | 0/1 | fire_detect | 플레임 센서 | 가스변화량+온도상승률 |
| `pump_on` | 0/1 | water_spray | ✅ | ✅ |
| `door_open` | 0/1 | door_control | 서보 | — |
| `ash_level` | 0~100 (%) | ash_level | HC-SR04 | HC-SR04 |
| `water_level` | 0~100 (%) | water_level | 수위 **추정**(펌프 가동 시간, 참고용 — 분사를 막지 않음) | — |
| `water_ok` | 0/1 | water_level | 추정 ≤10% → 0 (수동 보충 후 전원 재시작) | Float Switch |
| `sleeping` | 0/1 | sleep_mode | PIR | — |
| `led_color` | "#rrggbb" | rgb_led | ✅ | ✅ |
| `state` | string | (공통) FSM 상태명 | 10-State | 펄스 소화 포함 |

> SCA_Y 레거시 `ashtray/v1/telemetry` 도 payload 에 `device_id` 가 있으면 서버가 라우팅함. 펌웨어 업데이트 시 위 규칙으로 통일.

### Command action
| action | params | 대상 |
|---|---|---|
| `spray` | `duration_ms` 100~10000 | SCA_S 소화수 펌프 / SCA_Y 미스트 |
| `deodorize` | `duration_ms` | SCA_S 탈취 펌프 (**GPIO16**) |
| `clean` | — | SCA_S 청소액 펌프 |
| `door` | `open` bool | SCA_S 서보 (**GPIO17**) |
| `sleep` | `enable` bool | SCA_S |
| `buzzer` | `duration_ms` | SCA_Y |
| `led` | `color` "#rrggbb", `brightness` 0~255 | 공통 |
| `ota` | `url`, `version`, `sha256` | 공통 — 다운로드 후 SHA-256 검증 실패 시 적용 금지 |

> 안전: 화재 감지 시 소화 동작은 **기기 로컬 FSM이 자율 수행**. 서버 명령은 보조 수단 (네트워크 단절 대비).

## 2. 초기 등록 (AP 모드)

1. NVS에 WiFi 정보 없음 → SoftAP `AshtraySetup_{deviceId}` + 192.168.4.1 설정 페이지 (펌웨어 제공)
2. 사용자 입력: 집 WiFi SSID/PW, **연동 토큰**(웹 `/devices/setup` 에서 발급, 10분 유효)
3. STA 연결 성공 후:
   ```
   POST https://api.smart-clean-ashtray.com/devices
   {"device_id":"SCA_S","type_id":"SCA_S","pairing_token":"...","local_ip":"192.168.0.23","fw_version":"1.2.0"}
   → 201 {"device_id":"SCA_S","local_key":"<32 hex>","mqtt_host":"...","mqtt_port":8883}
   ```
4. `local_key` 를 NVS에 저장 (로컬 HTTP 인증용). 재등록 시 새 키로 교체됨.
5. mDNS `ashtray-{deviceid 소문자}.local` 등록

## 3. 로컬 HTTP API (같은 WiFi, 앱 전용)

모든 요청에 `X-Local-Key: <local_key>` 헤더 필수 — 불일치 시 401. (없으면 같은 WiFi의 누구나 기기 조작 가능)

| Method | Path | 응답 |
|---|---|---|
| GET | `/api/status` | telemetry 와 같은 JSON |
| POST | `/api/command` | body `{"action":"spray","params":{...}}` → `{"ok":true}` |

앱은 500ms 타임아웃으로 로컬 시도 → 실패 시 클라우드(`POST /devices/{id}/command`)로 폴백.
웹(HTTPS)은 혼합 콘텐츠 차단 때문에 로컬 모드 불가 → 항상 클라우드.
