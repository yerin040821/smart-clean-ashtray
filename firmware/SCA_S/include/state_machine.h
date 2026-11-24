/**
 * @file state_machine.h
 * @brief SCA_S 10-State FSM — types, thresholds, pure transition/output functions
 *
 *            ┌──────────── presence ────────────┐
 *   SLEEP ◀─ IDLE ─ presence ─▶ OCCUPIED (lid open)
 *    (deep)   │  ▲                 │ gas ↑
 *             │  │                 ▼
 *             │  └── done ── DEODORIZING ◀── gas ↓ ── SMOKING (lid open)
 *             ├─ ash ≥ 80% ─▶ ASH_FULL
 *             └─ clean req ─▶ CLEANING
 *   any ─ flame / gas spike ─▶ WARNING ─ flame ≥ 1.5s ─▶ FIRE_DETECTED (lid closed, pulse spray)
 *   any ─ MQ-135 saturated (sensor fault) ─▶ ERROR     (fire has priority over everything)
 */

#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <stdint.h>
#include <stdbool.h>

#if defined(NATIVE_TEST)
  typedef void *QueueHandle_t;
#elif defined(ESP_PLATFORM) || defined(ARDUINO)
  #include "freertos/FreeRTOS.h"
  #include "freertos/queue.h"
#else
  typedef void *QueueHandle_t;
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SYS_STATE_IDLE = 0,       /**< Standby, lid closed, green LED, gas baseline learning */
    SYS_STATE_OCCUPIED,       /**< Person present (PIR), lid open, cyan LED */
    SYS_STATE_SMOKING,        /**< Gas rising, lid open, yellow LED */
    SYS_STATE_DEODORIZING,    /**< Smoking ended → lid closed + deodorizer pump, blue LED */
    SYS_STATE_ASH_FULL,       /**< Ash ≥ 80%, orange LED, notify */
    SYS_STATE_CLEANING,       /**< Lid closed + cleaning pump cycle, white LED */
    SYS_STATE_WARNING,        /**< Flame flicker or gas spike, red blink + beep */
    SYS_STATE_FIRE_DETECTED,  /**< Flame confirmed: lid closed (smother) + 0.5s/1s pulse spray + siren */
    SYS_STATE_ERROR,          /**< Gas sensor fault (MQ-135 saturated ≥5 samples): purple LED, notify. Fire response stays active */
    SYS_STATE_SLEEP,          /**< No presence: lid closed, outputs off → deep sleep (PIR / timer wake) */
    SYS_STATE_COUNT
} SystemState_t;

/** Sensor snapshot (Sensor Task → State Task, every SENSOR_PERIOD_MS) */
typedef struct {
    uint16_t gas_raw;         /**< MQ-135 raw ADC (0~4095) */
    uint16_t gas_baseline;    /**< Adaptive IIR baseline */
    int16_t  gas_diff;        /**< raw - baseline */
    bool     flame;           /**< Flame sensor active */
    bool     presence;        /**< PIR active */
    uint8_t  ash_pct;         /**< Ash level 0~100% (HC-SR04) */
    uint16_t ash_distance_mm; /**< Raw distance, 0 = timeout */
    uint8_t  water_pct;       /**< Extinguishing water ESTIMATE 0~100% — informational only, never blocks spraying */
    bool     sensor_fault;    /**< MQ-135 saturated (ADC 4095) for MQ135_SATURATE_MAX consecutive samples */
} SensorData_t;

/** Requests from Scheduler / remote commands (bit flags, consumed by FSM) */
#define FSM_REQ_CLEAN   (1U << 0)
#define FSM_REQ_SLEEP   (1U << 1)
#define FSM_REQ_WAKE    (1U << 2)

/** FSM context (owned by State Task; pure functions for unit tests) */
typedef struct {
    SystemState_t state;
    uint32_t state_ms;        /**< time spent in current state */
    uint32_t flame_ms;        /**< continuous flame time */
    uint32_t clear_ms;        /**< continuous "no flame & gas normal" time (WARNING/FIRE exit) */
    uint32_t absent_ms;       /**< continuous no-presence time (OCCUPIED exit) */
} FsmContext_t;

/** Actuator outputs derived from state (State Task → Actuator Task) */
typedef struct {
    SystemState_t state;
    bool    lid_open;
    bool    extinguish_pulse; /**< 0.5s on / 1.0s off loop */
    bool    deodorize_on;
    bool    clean_on;
    uint8_t buzzer_mode;      /**< 0 off, 1 beep, 2 siren */
    SensorData_t data;        /**< latest snapshot for OLED */
} ActuatorCmd_t;

/* ── Thresholds ────────────────────────────────────────────────────── */
#define SENSOR_PERIOD_MS          500U
/* Air quality — identical to SCA_Y (state_machine.h SMOKE_THRESHOLD / HYSTERESIS_LOW) */
#define SMOKE_THRESHOLD            50     /**< gas_diff → SMOKING */
#define SMOKE_HYSTERESIS           20     /**< gas_diff < 30 → smoking ended */
#define MQ135_ADC_MAX            4095U    /**< saturation → no baseline update (SCA_Y) */
#define MQ135_SATURATE_MAX          5U    /**< consecutive saturated samples → sensor fault */
#define GAS_SPIKE_THRESHOLD       400     /**< gas_diff → WARNING (dense smoke / smoldering) */
#define FIRE_CONFIRM_MS          1500U    /**< continuous flame → FIRE_DETECTED */
#define FIRE_CLEAR_MS           10000U    /**< no flame & gas < spike for 10s → exit FIRE */
#define WARNING_CLEAR_MS         5000U
#define OCCUPIED_ABSENT_MS      30000U    /**< lid closes 30s after person leaves */
#define DEODORIZE_MS             5000U
#define CLEANING_MS              8000U
#define ASH_FULL_PCT               80U
#define ASH_CLEAR_PCT              70U
#define WATER_LOW_PCT              10U    /**< estimate ≤ 10% → telemetry water_ok=0 → "refill" notification */

/**
 * @brief One FSM step (pure — no I/O). Call every SENSOR_PERIOD_MS.
 * @param requests  FSM_REQ_* flags (consumed)
 * @return new state (ctx updated)
 */
SystemState_t FSM_Step(FsmContext_t *ctx, const SensorData_t *d, uint32_t requests, uint32_t dt_ms);

/** @brief Map state → actuator outputs (pure) */
void FSM_Outputs(SystemState_t state, ActuatorCmd_t *out);

/** @brief State name for logs / telemetry */
const char *FSM_StateName(SystemState_t state);

void FSM_Init(FsmContext_t *ctx);

/* Inter-task queues (created in main.c) */
extern QueueHandle_t g_sensor_queue;
extern QueueHandle_t g_actuator_queue;
extern QueueHandle_t g_remote_queue;

#ifdef __cplusplus
}
#endif

#endif /* STATE_MACHINE_H */
