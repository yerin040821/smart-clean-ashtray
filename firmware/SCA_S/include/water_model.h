/**
 * @file water_model.h
 * @brief Extinguishing-water level ESTIMATE (spec: "수위 추정") — no level sensor on SCA_S.
 * @details remaining_ml = tank - Σ(extinguish pump on-time × flow). Persisted in NVS so it survives
 *          deep sleep (timer/PIR wake).
 *          The tank is refilled MANUALLY and there is no refill button → after filling, power-cycle
 *          the device: a cold power-on resets the estimate to 100%.
 *          INFORMATIONAL ONLY — it never blocks the extinguishing pump (a stale estimate must not
 *          stop fire suppression). ≤ WATER_LOW_PCT → telemetry water_ok=0 → server "refill" alert.
 *          Calibrate WATER_TANK_ML / PUMP_FLOW_ML_PER_S with a measuring cup.
 */

#ifndef WATER_MODEL_H
#define WATER_MODEL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef WATER_TANK_ML
  #define WATER_TANK_ML        500U
#endif
#ifndef PUMP_FLOW_ML_PER_S
  #define PUMP_FLOW_ML_PER_S    20U    /**< mini mist pump ≈ 1.2 L/min */
#endif

void    Water_Init(bool cold_boot);       /**< cold power-on → 100%, deep-sleep wake → NVS value */
void    Water_Consume(uint32_t pump_ms);  /**< called by actuator task while extinguish pump runs */
uint8_t Water_GetPct(void);
uint32_t Water_GetMl(void);
void    Water_Persist(void);              /**< save if changed (called by scheduler) */

#ifdef __cplusplus
}
#endif

#endif /* WATER_MODEL_H */
