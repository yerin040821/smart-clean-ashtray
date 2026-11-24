# 검증 및 자동화 단위 테스트 스위트 (Test, ASPICE SWE.4)

이 디렉토리는 V-모델 검증 체계(ASPICE SWE.4)에 따라 스마트 클린 애쉬트레이(SCA)의 애플리케이션 계층(APL) FSM 상태 머신 전이 로직과 신호 처리 알고리즘을 물리적 하드웨어 없이도 정밀 검증할 수 있도록 구축된 자동화 테스트 스위트입니다.

---

## 📁 테스트 계층 구조

```text
test/
├── test_state_machine/          # [APL UT] 애플리케이션 FSM 검증 스위트
│   ├── test_fsm_common.h        # 선언형 파라미터 테스트 매크로 (APL_VERIFY_STEP) 및 컨텍스트
│   ├── test_sub_transitions.cpp # [Sub UT] 상태별 임계치/경계값(Boundary) 및 히스테리시스 단위 검증
│   ├── test_top_scenarios.cpp   # [Top UT] 실사용 미션 프로파일 라이프사이클 시나리오 검증
│   └── test_fsm.cpp             # Unity 기반 통합 테스트 러너 (Runner)
│
└── test_services/               # [SVC UT] 신호 처리 수학 알고리즘 단위 검증
    └── test_algorithms.cpp      # IIR 베이스라인 필터 수식, Diff 계산, 초음파 거리 환산식
```

---

## 🎯 계층형 테스트 방법론 (Hierarchical Test Design)

### 1. Sub UT (단위 경계값 및 히스테리시스 매트릭스 검증)
* **`test_sub_idle_boundaries`**: 연기 감지 임계치 정밀 경계값 검증 (차분값 50은 IDLE 유지, 51에서 정확히 SMOKING 전이)
* **`test_sub_smoking_hysteresis`**: 히스테리시스 불감대(Deadband) 검증 (차분값 30 이상 흡연 유지, 29 이하 IDLE 복귀하여 채터링 방지)
* **`test_sub_ash_full_boundaries`**: 초음파 에코 $300\mu s$ 이하 `ASH_FULL` 전이 및 $350\mu s$ 이상 비움 복귀 히스테리시스 검증
* **`test_sub_safety_interlock_priority`**: 흡연 중 갑작스러운 수조 결핍 시 최우선 순위로 `SYS_STATE_ERROR` 전이 인터록 검증

### 2. Top UT (미션 프로파일 시나리오 검증)
* **`test_top_scenario_normal_smoking`**: 정상 흡연 라이프사이클 ($T=0s$ 대기 $\rightarrow$ 연기 감지 $\rightarrow$ 흡연 $\rightarrow$ 연기 소산 $\rightarrow$ 대기 복귀)
* **`test_top_scenario_fire_ignition_and_extinguish`**: 미소화 꽁초 발화 시나리오 (급격한 온도 상승 $dT/dt = 2.8$ 감지 $\rightarrow$ `WARNING` $\rightarrow$ 50°C 이상 3초 지속 $\rightarrow$ `FIRE_DETECTED` 펄스 살수 $\rightarrow$ 냉각 후 정상 복귀)
* **`test_top_scenario_ash_full_and_water_fault`**: 다중 고장 경합 시나리오 (재 꽉 참 상태에서 화재 발생 및 물 부족 다중 발생 시 안전 우선순위 자동 방어)

---

## 🚀 테스트 실행 방법

### 1. PC Native 고속 단위 테스트 실행 (강력 추천)
FreeRTOS OS 없이 호스트 PC(Windows) 환경에서 **1초 만에** 전체 테스트 스위트를 빌드하고 검증 결과를 출력합니다:
```powershell
& "C:\Users\yerin\.platformio\penv\Scripts\pio.exe" test -e native -f test_state_machine
```

### 2. 실제 ESP32-S3 타겟 보드 검증
실제 보드 위에서 하드웨어 타겟 검증을 수행할 때 실행합니다:
```powershell
& "C:\Users\yerin\.platformio\penv\Scripts\pio.exe" test -e esp32-s3-devkitc-1 -f test_state_machine
```
