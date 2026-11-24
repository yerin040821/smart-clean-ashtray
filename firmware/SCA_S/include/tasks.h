/**
 * @file tasks.h
 * @brief SCA_S 5-Task FreeRTOS architecture (spec §1-2)
 *
 *   P4  Task_State      FSM brain (consumes sensor snapshots, emits actuator commands)
 *   P3  Task_Actuator   single owner of pumps / lid / LED / buzzer / OLED
 *   P2  Task_Sensor     500ms sampling: MQ-135 (adaptive baseline), flame, PIR, HC-SR04
 *   P1  Task_Comm       Wi-Fi provisioning, registration, MQTT/TLS, Local API, OTA
 *   P1  Task_Scheduler  auto-sleep, daily auto-clean, water-estimate persistence
 */

#ifndef TASKS_H
#define TASKS_H

#include <stdint.h>
#include <stdbool.h>
#include "state_machine.h"
#include "hal_power.h"

#ifdef __cplusplus
extern "C" {
#endif

void Task_State(void *pv);
void Task_Actuator(void *pv);
void Task_Sensor(void *pv);
void Task_Comm(void *pv);
void Task_Scheduler(void *pv);

/* ── State task API ──────────────────────────────────────────────── */
SystemState_t Task_State_GetCurrentState(void);
uint32_t      Task_State_GetStateMs(void);
void          Task_State_GetLatestData(SensorData_t *out);
void          Task_State_Request(uint32_t fsm_req_flags);   /**< FSM_REQ_* (thread-safe) */

/* ── Sensor task API ─────────────────────────────────────────────── */
void Task_Sensor_NotifyState(SystemState_t st);   /**< baseline learns only in IDLE/SLEEP */

/* ── Comm task API ───────────────────────────────────────────────── */
/** Ask comm task to publish final telemetry + "sleeping" status; returns true once done (or offline) */
bool Task_Comm_PrepareSleep(uint32_t timeout_ms);

/* ── Scheduler API ───────────────────────────────────────────────── */
void         Task_Scheduler_SetAutoSleep(bool enable);
bool         Task_Scheduler_GetAutoSleep(void);
WakeReason_t Task_Scheduler_WakeReason(void);
void         Task_Scheduler_SetWakeReason(WakeReason_t r);

#ifdef __cplusplus
}
#endif

#endif /* TASKS_H */
