/**
 * @file task_state.c
 * @brief State Task (Priority 4 - Highest): runs FSM_Step on each sensor snapshot, dispatches outputs
 */

#include "tasks.h"
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

static FsmContext_t      s_ctx;
static SensorData_t      s_latest;
static volatile uint32_t s_requests = 0;
static portMUX_TYPE      s_mux = portMUX_INITIALIZER_UNLOCKED;

void Task_State_Request(uint32_t flags)
{
    portENTER_CRITICAL(&s_mux);
    s_requests |= flags;
    portEXIT_CRITICAL(&s_mux);
}

static uint32_t take_requests(void)
{
    portENTER_CRITICAL(&s_mux);
    const uint32_t r = s_requests;
    s_requests = 0;
    portEXIT_CRITICAL(&s_mux);
    return r;
}

SystemState_t Task_State_GetCurrentState(void) { return s_ctx.state; }
uint32_t      Task_State_GetStateMs(void)      { return s_ctx.state_ms; }

void Task_State_GetLatestData(SensorData_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_mux);
    *out = s_latest;
    portEXIT_CRITICAL(&s_mux);
}

void Task_State(void *pv)
{
    (void)pv;
    FSM_Init(&s_ctx);
    printf("[TASK_STATE] FSM Task started (Priority 4)\n");

    SensorData_t d;
    while (1)
    {
        if (xQueueReceive(g_sensor_queue, &d, portMAX_DELAY) != pdTRUE) continue;

        portENTER_CRITICAL(&s_mux);
        s_latest = d;
        portEXIT_CRITICAL(&s_mux);

        const SystemState_t prev = s_ctx.state;
        const SystemState_t now = FSM_Step(&s_ctx, &d, take_requests(), SENSOR_PERIOD_MS);

        if (now != prev)
        {
            printf("[STATE] %s → %s | gas %u(%+d) flame=%d pir=%d ash=%u%% water=%u%%\n",
                   FSM_StateName(prev), FSM_StateName(now), d.gas_raw, d.gas_diff,
                   d.flame, d.presence, d.ash_pct, d.water_pct);
            Task_Sensor_NotifyState(now);
        }

        ActuatorCmd_t cmd;
        FSM_Outputs(now, &cmd);
        cmd.data = d;
        xQueueSend(g_actuator_queue, &cmd, 0);
    }
}
