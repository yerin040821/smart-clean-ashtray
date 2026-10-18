# 하드웨어 추상화 계층 (HAL, Hardware Abstraction Layer)

이 디렉토리는 스마트 클린 애쉬트레이(SCA) 펌웨어의 모든 하위 하드웨어 종속성, 주변장치 통신 버스 드라이버(I2C, ADC, Single-Wire), GPIO 핀 제어 로직을 캡슐화하여 상위 애플리케이션 계층과 분리하는 계층입니다.

---

## 📁 모듈 구성 및 역할

| 소스 파일 | 담당 역할 | 제어 하드웨어 및 인터페이스 |
| :--- | :--- | :--- |
| `hal_sensor.cpp` | 센서 데이터 취득, 하드웨어 타이밍 제어 및 원시 신호 전처리 | • **MQ-135**: 12-bit ADC1_CH4 (`GPIO 5`)<br/>• **AM2302 (DHT22)**: 마이크로초 정밀 단일선 비트뱅잉 (`GPIO 14`)<br/>• **HC-SR04**: 초음파 반사 펄스 측정 및 워치독 갱신 (`GPIO 6 Trig, GPIO 7 Echo`)<br/>• **수위 센서**: 플로트 스위치 내부 풀업 Digital Input (`GPIO 8`) |
| `hal_actuator.cpp` | 액추에이터 구동, 디스플레이 렌더링, 부팅 안전 제어 | • **OLED**: 1.3인치 SSH1106 128x64 화면 UI (`I2C 100kHz, GPIO 21 SDA, GPIO 47 SCL`)<br/>• **워터 펌프**: F5305S MOSFET 모듈 기반 0.5s 미스트 펄스 분무 (`GPIO 18`)<br/>• **부저**: 경고 비프음 및 화재 비상 사이렌 출력 (`GPIO 17`)<br/>• **상태 LED**: WS2812B RGB LED 상태 피드백 (`GPIO 4`) |
| `hal_network.cpp` | 무선 네트워크 통신 및 비동기 MQTT 메시지 브리지 | • ESP32 Wi-Fi Station 모드<br/>• PubSubClient 기반 MQTT 브로커(Cafe24 VPS / Mosquitto) 연결 및 텔레메트리 발행 |
| `hal_pins.h` | 회로도와 1:1 매핑되는 중앙 집중형 하드웨어 핀아웃 정의 | `platformio.ini` 빌드 플래그(`-D PIN_XX=YY`)를 통해 유연하게 오버라이드 지원 |

---

## 🛡️ HAL 계층의 핵심 안전 메커니즘 (Safety Design)

1. **스텝-0 부팅 즉시 펌프 강제 차단 (Power-on Lockout)**:
   - `HAL_Actuator_Init()` 실행 시 USB-CDC 시리얼이나 I2C 초기화보다 앞서 **`GPIO 18` 펌프 핀을 즉시 LOW로 강제 고정**합니다.
   - MCU 부팅 및 리셋 구간에서 GPIO가 고임피던스(Hi-Z) 상태일 때 MOSFET 게이트 잔류 전하로 인해 모터가 무한 공회전하는 물리적 폭주를 원천 차단합니다.

2. **마이크로초 정밀 AM2302 자체 단일선 프로토콜**:
   - ESP32-S3의 240MHz 고속 클럭 타이밍 이슈로 인해 타임아웃이 발생하던 기존 외부 라이브러리를 배제하고, 호스트 18ms Low 기동 펄스 및 비트별 시간폭($28\mu s = '0', 70\mu s = '1'$)을 직접 파싱하는 견고한 드라이버를 구현했습니다.

3. **1.3인치 SSH1106 OLED 전용 100kHz 표준 I2C 통신**:
   - 브레드보드 점퍼선의 배선 노이즈 및 감쇠를 방지하기 위해 I2C 클럭을 안정적인 100kHz로 설정하고, 1.3인치 SSH1106 패널 램 오프셋에 최적화된 초기화 시퀀스를 수행합니다. 연결 해제 시에도 3초마다 백그라운드 자동 복구(Auto-recovery)를 시도합니다.
