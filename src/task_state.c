/**
 * @file task_state.c
 * @brief Brain FSM Task implementation (Priority 4 - Highest)
 * @details Receives SensorData_t via g_sensor_queue, executes system state machine according to
 *          the embedded state transition diagram, generates CANalyzer Trace logs, and posts ActuatorCmd_t.
 */

#include "task_state.h"
#include <stdio.h>

#if !defined(NATIVE_TEST)
  #include "task_sensor.h"
  #if defined(ESP_PLATFORM) || defined(ARDUINO)
    #include "freertos/FreeRTOS.h"
    #include "freertos/task.h"
  #endif
#endif

static SystemState_t s_current_state = SYS_STATE_IDLE;
static uint32_t s_warning_timer_ms = 0;
static SensorData_t s_latest_sensor_data = {0};

static const char* Get_State_Name(SystemState_t state)
{
    switch (state)
    {
        case SYS_STATE_IDLE:          return "IDLE";
        case SYS_STATE_SMOKING:       return "SMOKING";
        case SYS_STATE_ASH_FULL:      return "ASH_FULL";
        case SYS_STATE_WARNING:       return "WARNING";
        case SYS_STATE_FIRE_DETECTED: return "FIRE_DETECTED";
        case SYS_STATE_ERROR:         return "ERROR";
        default:                      return "UNKNOWN";
    }
}

static void Log_CANalyzer_Trace(uint32_t time_ms, SystemState_t state, const SensorData_t *data)
{
    printf("[%8lu ms] [STATE_TRACE] State: %-13s | Raw: %4u | Base: %4u | Diff: %4d | Temp: %4.1f C | Echo: %4lu us | Ash: %3u%%\n",
           (unsigned long)time_ms, Get_State_Name(state), data->mq135_raw, data->mq135_baseline,
           data->mq135_diff, data->temp_current, (unsigned long)data->ash_echo_us, data->ash_level_pct);
}

SystemState_t Process_FSM_State_Transition(SystemState_t current_state, const SensorData_t *data, uint32_t *warning_timer_ms)
{
    if (data == NULL) return current_state;

    /* 1. Emergency Error Condition (Water empty / float switch LOW) */
    if (data->water_empty)
    {
        return SYS_STATE_ERROR;
    }

    /* 2. Finite State Machine (FSM) Transitions */
    switch (current_state)
    {
        case SYS_STATE_IDLE:
            /* Priority: Fire/Smoke > Ash Full */
            if (data->mq135_diff > SMOKE_THRESHOLD)
            {
                return SYS_STATE_SMOKING;
            }
            /* Ash Full Condition: Ultrasonic Echo <= 300 us (or ash level >= 80%) */
            else if (data->ash_echo_us > 0 && data->ash_echo_us <= ASH_FULL_ECHO_US_THRESHOLD)
            {
                return SYS_STATE_ASH_FULL;
            }
            break;

        case SYS_STATE_ASH_FULL:
            /* If smoking occurs while full, prioritize smoking mode */
            if (data->mq135_diff > SMOKE_THRESHOLD)
            {
                return SYS_STATE_SMOKING;
            }
            /* Recovery condition with Hysteresis (echo > 350us and ash level < 70%) */
            else if (data->ash_echo_us > (ASH_FULL_ECHO_US_THRESHOLD + ASH_FULL_RECOVERY_US_HYST))
            {
                return SYS_STATE_IDLE;
            }
            break;

        case SYS_STATE_SMOKING:
            if (data->mq135_diff < (SMOKE_THRESHOLD - HYSTERESIS_LOW))
            {
                /* Check if ash is full upon returning from smoking */
                if (data->ash_echo_us > 0 && data->ash_echo_us <= ASH_FULL_ECHO_US_THRESHOLD)
                {
                    return SYS_STATE_ASH_FULL;
                }
                return SYS_STATE_IDLE;
            }
            else if (data->temp_rate > WARNING_RATE_THRESHOLD)
            {
                if (warning_timer_ms) *warning_timer_ms = 0;
                return SYS_STATE_WARNING;
            }
            break;

        case SYS_STATE_WARNING:
            if (data->temp_rate <= WARNING_RATE_THRESHOLD &&
                data->mq135_diff < (SMOKE_THRESHOLD - HYSTERESIS_LOW))
            {
                return SYS_STATE_IDLE;
            }
            else if (data->temp_current > FIRE_TEMP_THRESHOLD)
            {
                if (warning_timer_ms)
                {
                    *warning_timer_ms += SENSOR_SAMPLING_PERIOD_MS;
                    if (*warning_timer_ms >= FIRE_TIMEOUT_MS)
                    {
                        return SYS_STATE_FIRE_DETECTED;
                    }
                }
            }
            else
            {
                if (warning_timer_ms) *warning_timer_ms = 0;
            }
            break;

        case SYS_STATE_FIRE_DETECTED:
            if (data->temp_current < SAFE_TEMP_THRESHOLD)
            {
                return SYS_STATE_IDLE;
            }
            break;

        case SYS_STATE_ERROR:
            if (!data->water_empty)
            {
                return SYS_STATE_IDLE;
            }
            break;

        default:
            return SYS_STATE_IDLE;
    }

    return current_state;
}

#if !defined(NATIVE_TEST)
void Task_State(void *pvParameters)
{
    (void)pvParameters;

    SensorData_t sensor_data;

    printf("[TASK_STATE] Brain FSM Task started (Priority 4 - Highest)\n");

    while (1)
    {
        /* 1. Receive Sensor Data Event from Sensor Task */
        if (xQueueReceive(g_sensor_queue, &sensor_data, portMAX_DELAY) == pdTRUE)
        {
            SystemState_t prev_state = s_current_state;
            uint32_t current_time_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);

            /* 1-1. Update latest sensor data cache for telemetry task */
            s_latest_sensor_data = sensor_data;

            /* 2. Process FSM State Transition */
            s_current_state = Process_FSM_State_Transition(s_current_state, &sensor_data, &s_warning_timer_ms);

            /* 3. Log CANalyzer Trace output continuously on every cycle */
            Log_CANalyzer_Trace(current_time_ms, s_current_state, &sensor_data);

            if (s_current_state != prev_state)
            {
                /* Notify Sensor Task of new state for adaptive IIR baseline filtering */
                Task_Sensor_NotifyState(s_current_state);
            }

            /* 4. Dispatch Actuator Command to Actuator Task via g_actuator_queue */
            if (g_actuator_queue != NULL)
            {
                ActuatorCmd_t cmd;
                cmd.target_state = s_current_state;
                cmd.pump_pulse_en = (s_current_state == SYS_STATE_FIRE_DETECTED);
                cmd.pump_lock = (s_current_state == SYS_STATE_ERROR);
                cmd.ash_level_pct = sensor_data.ash_level_pct;
                cmd.ash_echo_us = sensor_data.ash_echo_us;
                cmd.gas_diff = sensor_data.mq135_diff;
                cmd.temp_current = sensor_data.temp_current;

                xQueueSend(g_actuator_queue, &cmd, 0);
            }
        }
    }
}
#endif

SystemState_t Task_State_GetCurrentState(void)
{
    return s_current_state;
}

void Task_State_GetLatestData(SensorData_t *out_data)
{
    if (out_data)
    {
        *out_data = s_latest_sensor_data;
    }
}
