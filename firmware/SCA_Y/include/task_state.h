/**
 * @file task_state.h
 * @brief Brain FSM Task (Priority 4 - Highest) header
 */

#ifndef TASK_STATE_H
#define TASK_STATE_H

#include "state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FreeRTOS State Machine Control Task function (Priority 4)
 * @param pvParameters Task context parameter (unused)
 */
void Task_State(void *pvParameters);

/**
 * @brief Get current active FSM system state
 * @return SystemState_t Current state
 */
SystemState_t Task_State_GetCurrentState(void);

/**
 * @brief Get latest captured sensor payload for telemetry export
 * @param[out] out_data Destination buffer
 */
void Task_State_GetLatestData(SensorData_t *out_data);

/**
 * @brief Pure FSM State Transition Processor for Unit Testing and State Evaluation
 * @param current_state Active state before update
 * @param data Input sensor payload
 * @param[in,out] warning_timer_ms Pointer to warning timeout counter (in ms)
 * @return SystemState_t Evaluated next system state
 */
SystemState_t Process_FSM_State_Transition(SystemState_t current_state, const SensorData_t *data, uint32_t *warning_timer_ms);

#ifdef __cplusplus
}
#endif

#endif /* TASK_STATE_H */
