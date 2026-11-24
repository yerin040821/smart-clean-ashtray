# 애플리케이션 및 서비스 계층 헤더 정의 (Include)

이 디렉토리는 스마트 클린 애쉬트레이(SCA) 펌웨어의 핵심 데이터 타입, FreeRTOS 태스크 간 통신(IPC) 큐 페이로드 구조체, 유한 상태 머신(FSM) 열거형, 파라미터 임계치를 선언하는 헤더 모음입니다.

---

## 📁 헤더 파일 구성 및 역할

| 헤더 파일 | 소속 계층 | 주요 정의 및 역할 |
| :--- | :--- | :--- |
| `state_machine.h` | **공통 정의 (Common)** | • 시스템 상태 열거형 (`SystemState_t`)<br/>• IPC 데이터 구조체 (`SensorData_t`, `ActuatorCmd_t`)<br/>• 연기/화재/비움 임계치 및 히스테리시스 파라미터 정의 |
| `task_state.h` | **애플리케이션 계층 (APL)** | • 두뇌 FSM 제어 태스크 (`Task_State`, 우선순위 4)<br/>• 순수 상태 전이 결정 함수 (`Process_FSM_State_Transition`)<br/>• 최신 센서 데이터 및 상태 획득 인터페이스 |
| `task_actuator.h` | **서비스 계층 (Service)** | • 액추에이터 제어 태스크 (`Task_Actuator`, 우선순위 3)<br/>• OLED UI 렌더링, 0.5초 펄스 살수, 부저 알림 인터페이스 |
| `task_sensor.h` | **서비스 계층 (Service)** | • 센서 취득 태스크 (`Task_Sensor`, 우선순위 2)<br/>• 적응형 IIR 베이스라인 갱신 및 상태 통보 인터페이스 |
| `task_mqtt.h` | **서비스 계층 (Service)** | • 백그라운드 클라우드 브리지 태스크 (`Task_MQTT`, 우선순위 1)<br/>• JSON 텔레메트리 및 긴급 이벤트 발행 인터페이스 |

---

## ⚙️ 시스템 상태 열거형 (`SystemState_t`)

SCA 시스템은 총 6가지 결정론적(Deterministic) 상태로 동작합니다:

1. **`SYS_STATE_IDLE (0)` [대기 모드]**:
   - 실내 공기질 양호 상태. 
   - MQ-135 센서의 $\alpha=0.1$ 적응형 IIR 필터가 실시간으로 동작하며 주변 공기질 베이스라인을 지속적으로 보정합니다. (OLED: GOOD, 녹색 LED)
2. **`SYS_STATE_SMOKING (1)` [흡연 모드]**:
   - 공기질 차분값($\text{Diff} = \text{Raw} - \text{Baseline}$)이 `SMOKE_THRESHOLD (50)`을 초과하여 흡연을 감지한 상태. 베이스라인 업데이트가 동결됩니다. (OLED: POOR, 노란색 LED)
3. **`SYS_STATE_ASH_FULL (2)` [재 비움 알림 모드]**:
   - 초음파 센서 에코 시간이 `ASH_FULL_ECHO_US_THRESHOLD (300us)` 이하로 꽁초가 가득 찼음을 감지한 상태. (OLED: PLEASE EMPTY TANK!, 비움 안내)
   - 재가 비워져 에코가 $350\mu s$ 이상으로 회복되면 대기 모드로 자동 복귀합니다.
4. **`SYS_STATE_WARNING (3)` [화재 위험 경보 모드]**:
   - 시간당 온도 변화율($dT/dt$)이 `WARNING_RATE_THRESHOLD (2.0°C/s)`를 초과하여 급격한 발화 징후를 감지한 상태. (단발 경고음 발생)
5. **`SYS_STATE_FIRE_DETECTED (4)` [화재 진압 분무 모드]**:
   - 온도가 $50^\circ\text{C}$ 이상인 상태가 3초(`FIRE_TIMEOUT_MS`) 이상 지속되어 실제 꽁초 화재로 판정된 상태.
   - **0.5초 분사 $\rightarrow$ 1.0초 대기 $\rightarrow$ 재측정** 방식의 펄스 살수를 수행하며 비상 사이렌을 가동합니다. 온도가 안전 기준($40^\circ\text{C}$) 이하로 냉각되면 대기 모드로 복귀합니다.
6. **`SYS_STATE_ERROR (5)` [수조 부족 안전 락아웃 모드]**:
   - 플로트 스위치가 동작하여 소화수 잔량 부족을 감지한 상태.
   - 펌프를 강제로 차단(Dry-Run 방지)하고 경고를 표시합니다. 물이 재충전되면 자동으로 정상 동작으로 복귀합니다.

---

## 📦 태스크 간 통신 (IPC) 데이터 구조체

### `SensorData_t` (`Task_Sensor` $\rightarrow$ `Task_State`)
```c
typedef struct {
    uint16_t mq135_raw;       // MQ-135 아날로그 원시 ADC 값 (0~4095)
    uint16_t mq135_baseline;  // 적응형 IIR 베이스라인 기준값
    int16_t  mq135_diff;      // 순수 공기질 변화량 (raw - baseline)
    float    temp_current;    // 현재 실내 온도 (°C)
    float    temp_rate;       // 시간당 온도 변화율 (dT/dt, °C/s)
    uint8_t  ash_level_pct;   // 꽁초 적재율 (0 ~ 100%)
    uint32_t ash_echo_us;     // 초음파 원시 에코 펄스 시간 (us)
    bool     water_empty;     // 소화수 부족 여부 (true: 부족, false: 정상)
} SensorData_t;
```

### `ActuatorCmd_t` (`Task_State` $\rightarrow$ `Task_Actuator`)
```c
typedef struct {
    SystemState_t target_state;  // FSM 판단에 따른 목표 제어 상태
    bool          pump_pulse_en; // 0.5초 소화수 펄스 분무 활성화 플래그
    bool          pump_lock;     // 공회전 방지 강제 펌프 락 플래그
    uint8_t       ash_level_pct; // OLED 표시용 최신 적재율 (%)
    uint32_t      ash_echo_us;   // OLED 표시용 최신 에코 펄스 시간 (us)
    int16_t       gas_diff;      // OLED 표시용 가스 변화량
    float         temp_current;  // OLED 표시용 현재 온도 (°C)
} ActuatorCmd_t;
```
