# SCA(Smart Clean Ashtray) 임베디드 소프트웨어 테스트 명세서

본 문서는 PlatformIO Unit Testing 프레임워크인 Unity를 활용하여 SCA 시스템의 핵심 C 3-Layer 상태 머신(FSM) 및 신호 처리 알고리즘을 기능별로 검증하기 위한 **유닛 테스트 명세서**입니다.

---

## 1. 테스트 디렉토리 구성

```
smart_clean_ashtray_v1/
├── platformio.ini               # Test build flags 및 include 설정
├── test/
│   ├── test_state_machine/       # FSM 상태 머신 전이 기능별 유닛 테스트
│   │   └── test_fsm.cpp
│   ├── test_services/            # IIR baseline, diff, 초음파 적재량 알고리즘 테스트
│   │   └── test_algorithms.cpp
│   └── test_plan.md
```

---

## 2. 테스트 명세 및 세부 검증 시나리오

### 2.1. 상태 머신 (FSM State Machine) 기능별 유닛 테스트 (`test/test_state_machine/test_fsm.cpp`)
- **대상 모듈:** `state_machine.h`, `task_state.h`, `task_state.c`
- **테스트 케이스:**
  1. `test_initial_state_is_idle`: 초기 상태가 `SYS_STATE_IDLE`로 평가되는지 검증
  2. `test_transition_idle_to_smoking`: `mq135_diff > SMOKE_THRESHOLD (50)` 발생 시 `IDLE` $\rightarrow$ `SMOKING` 전이 검증
  3. `test_transition_smoking_to_idle_hysteresis`: `mq135_diff < 30` 히스테리시스 회복 시 `SMOKING` $\rightarrow$ `IDLE` 복귀 검증
  4. `test_transition_smoking_to_warning_by_rate`: $dT/dt > 2.0^\circ\text{C/sec}$ 급증 시 `SMOKING` $\rightarrow$ `WARNING` 전이 검증
  5. `test_transition_warning_to_fire_detected_3s_timeout`: $\text{Temp} > 50^\circ\text{C}$ 조건이 3초간 지속될 때 `WARNING` $\rightarrow$ `FIRE_DETECTED` 펄스 소화 상태 전이 검증
  6. `test_fire_extinguished_recovery`: $\text{Temp} < 40^\circ\text{C}$ 소화 완료 확인 시 `FIRE_DETECTED` $\rightarrow$ `IDLE` 복귀 검증
  7. `test_water_empty_emergency_error_and_recovery`: 수위 스위치 `water_empty = true` 시 즉시 `SYS_STATE_ERROR` 진입 및 펌프 Force Lock, 보충 시 `IDLE` 복원 검증

### 2.2. 신호 처리 및 알고리즘 유닛 테스트 (`test/test_services/test_algorithms.cpp`)
- **대상 모듈:** `state_machine.h`, `hal_sensor.h`
- **테스트 케이스:**
  1. `test_iir_baseline_update`: IDLE 시 IIR 적응형 baseline 연산식 $\text{baseline} = \frac{\text{baseline} \times 9 + \text{raw}}{10}$ 정확성 검증
  2. `test_gas_diff_calculation`: Difference 연산 $\text{diff} = \text{raw} - \text{baseline}$ 검증
  3. `test_ultrasonic_ash_level_calculation`: HC-SR04 거리(cm) 기반 적재량 백분율(0% ~ 100%) 선형 변환 정확성 검증
