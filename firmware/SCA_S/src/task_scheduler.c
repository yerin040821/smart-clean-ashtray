/**
 * @file task_scheduler.c
 * @brief Scheduler Task (Priority 1): auto-sleep, daily auto-clean, water-estimate persistence
 *
 * Auto-sleep (spec "PIR 딥슬립")
 *   IDLE with no presence for SLEEP_QUIET_S (10 min) → FSM SLEEP → final telemetry → deep sleep.
 *   Wake on PIR, or every SAFETY_WAKE_S (60s) to re-check flame/gas — a smoldering butt must not
 *   go unnoticed just because nobody is around. After a timer wake the device sleeps again after
 *   TIMER_WAKE_QUIET_S (20s) if everything is still quiet.
 */

#include "tasks.h"
#include "hal_config.h"
#include "water_model.h"
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_attr.h"

#ifndef SLEEP_QUIET_S
  #define SLEEP_QUIET_S        600U       /* 10 min */
#endif
#ifndef TIMER_WAKE_QUIET_S
  #define TIMER_WAKE_QUIET_S    20U
#endif
#ifndef SAFETY_WAKE_S
  #define SAFETY_WAKE_S         60U
#endif
#ifndef AUTO_CLEAN_INTERVAL_S
  #define AUTO_CLEAN_INTERVAL_S (24U * 3600U)
#endif

RTC_DATA_ATTR static uint32_t s_since_clean_s = 0;   /* approx. wall time since last clean (survives sleep) */
static volatile bool s_auto_sleep = true;
static WakeReason_t s_wake = WAKE_POWER_ON;

void Task_Scheduler_SetAutoSleep(bool enable)
{
    s_auto_sleep = enable;
    HAL_Config_SetInt("autosleep", enable ? 1 : 0);
    printf("[SCHED] auto-sleep %s\n", enable ? "ON" : "OFF");
}

bool         Task_Scheduler_GetAutoSleep(void)          { return s_auto_sleep; }
WakeReason_t Task_Scheduler_WakeReason(void)            { return s_wake; }
void         Task_Scheduler_SetWakeReason(WakeReason_t r) { s_wake = r; }

void Task_Scheduler(void *pv)
{
    (void)pv;
    s_auto_sleep = HAL_Config_GetInt("autosleep", 1) != 0;
    if (s_wake == WAKE_POWER_ON) s_since_clean_s = 0;
    printf("[TASK_SCHED] Scheduler started (Priority 1) wake=%s auto-sleep=%s\n",
           HAL_Power_WakeName(s_wake), s_auto_sleep ? "on" : "off");

    const uint32_t quiet_target = (s_wake == WAKE_TIMER) ? TIMER_WAKE_QUIET_S : SLEEP_QUIET_S;
    uint32_t quiet_s = 0;
    bool activity_seen = false;      /* after a timer wake, any activity → normal 10-min window */
    uint32_t tick = 0;
    TickType_t last = xTaskGetTickCount();

    while (1)
    {
        vTaskDelayUntil(&last, pdMS_TO_TICKS(1000));
        tick++;
        s_since_clean_s++;

        const SystemState_t st = Task_State_GetCurrentState();
        SensorData_t d;
        Task_State_GetLatestData(&d);

        /* ── quiet timer ── */
        if (st != SYS_STATE_IDLE || d.presence)
        {
            if (st != SYS_STATE_SLEEP) { quiet_s = 0; activity_seen = true; }
        }
        else
        {
            quiet_s++;
        }
        const uint32_t target = activity_seen ? SLEEP_QUIET_S : quiet_target;
        if (s_auto_sleep && st == SYS_STATE_IDLE && quiet_s >= target)
        {
            Task_State_Request(FSM_REQ_SLEEP);
        }

        /* ── enter deep sleep once FSM is in SLEEP and outputs settled ── */
        if (st == SYS_STATE_SLEEP && Task_State_GetStateMs() >= 1000U)
        {
            Task_Comm_PrepareSleep(3000);
            Water_Persist();
            s_since_clean_s += SAFETY_WAKE_S;
            HAL_Power_DeepSleep(SAFETY_WAKE_S);   /* does not return */
        }

        /* ── daily auto-clean ── */
        if (s_since_clean_s >= AUTO_CLEAN_INTERVAL_S && st == SYS_STATE_IDLE)
        {
            printf("[SCHED] daily auto-clean\n");
            Task_State_Request(FSM_REQ_CLEAN);
            s_since_clean_s = 0;
        }

        if (tick % 10U == 0U) Water_Persist();
    }
}
