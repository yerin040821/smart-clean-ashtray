/**
 * @file task_actuator.c
 * @brief Actuator Driving Task implementation (Priority 3)
 * @details Receives ActuatorCmd_t, drives OLED, WS2812B, Buzzer, and executes
 *          0.5s Pulse Spray pump control loop with Dry-run lock protection.
 */

#include "task_actuator.h"
#include "hal_actuator.h"
#include "task_state.h"
#include "app_protocol.h"
#include <stdio.h>

#if defined(ESP_PLATFORM) || defined(ARDUINO)
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
#else
  #include "FreeRTOS.h"
  #include "task.h"
#endif

static uint8_t buzzer_mode_for(SystemState_t st)
{
    return st == SYS_STATE_WARNING ? 1 : (st == SYS_STATE_FIRE_DETECTED ? 2 : 0);
}

/**
 * @brief Execute a remote command (MQTT / Local API) — this task is the single hardware owner.
 * @details Safety: manual spray is refused while water is empty (dry-run) or while the FSM is
 *          already running its own FIRE pulse loop; it aborts immediately if the tank runs dry.
 */
static void Handle_Remote_Command(const RemoteCmd_t *rc)
{
    const SystemState_t st = Task_State_GetCurrentState();
    SensorData_t d;
    Task_State_GetLatestData(&d);

    switch (rc->type)
    {
        case REMOTE_CMD_SPRAY:
            if (st == SYS_STATE_ERROR || d.water_empty)
            {
                printf("[TASK_ACTUATOR] Remote spray refused: water empty (dry-run lock)\n");
                break;
            }
            if (st == SYS_STATE_FIRE_DETECTED)
            {
                printf("[TASK_ACTUATOR] Remote spray ignored: FIRE pulse loop already active\n");
                break;
            }
            printf("[TASK_ACTUATOR] Remote spray %u ms\n", rc->duration_ms);
            HAL_Pump_SetOutput(true);
            for (uint16_t t = 0; t < rc->duration_ms; t += 100)
            {
                vTaskDelay(pdMS_TO_TICKS(100));
                Task_State_GetLatestData(&d);
                if (d.water_empty) { printf("[TASK_ACTUATOR] Water empty — spray aborted\n"); break; }
            }
            HAL_Pump_SetOutput(false);
            break;

        case REMOTE_CMD_BUZZER:
            HAL_Buzzer_SetMode(1);
            vTaskDelay(pdMS_TO_TICKS(rc->duration_ms));
            HAL_Buzzer_SetMode(buzzer_mode_for(Task_State_GetCurrentState()));
            break;

        case REMOTE_CMD_LED:
            HAL_LED_SetIdleColor(rc->r, rc->g, rc->b, rc->brightness);
            if (st == SYS_STATE_IDLE) HAL_LED_SetStateColor(SYS_STATE_IDLE);
            break;

        default:
            break;
    }
}

void Task_Actuator(void *pvParameters)
{
    (void)pvParameters;

    ActuatorCmd_t cmd;
    RemoteCmd_t remote;

    printf("[TASK_ACTUATOR] Actuator Driving Task started (Priority 3)\n");

    while (1)
    {
        /* 0. Remote commands (non-blocking poll) */
        if (g_remote_queue != NULL && xQueueReceive(g_remote_queue, &remote, 0) == pdTRUE)
        {
            Handle_Remote_Command(&remote);
        }

        /* 1. Receive Actuator Command payload from State Task */
        if (xQueueReceive(g_actuator_queue, &cmd, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            /* 2. Update WS2812B LED status color */
            HAL_LED_SetStateColor(cmd.target_state);

            /* 3. Update OLED Display UI */
            HAL_OLED_UpdateUI(cmd.target_state, cmd.gas_diff, cmd.temp_current, cmd.ash_level_pct);

            /* 4. Update Buzzer alarm mode */
            if (cmd.target_state == SYS_STATE_WARNING)
            {
                HAL_Buzzer_SetMode(1); // Short warning beep
            }
            else if (cmd.target_state == SYS_STATE_FIRE_DETECTED)
            {
                HAL_Buzzer_SetMode(2); // Emergency siren
            }
            else
            {
                HAL_Buzzer_SetMode(0); // OFF
            }

            /* 5. Handle Water Pump Pulse Control vs Dry-run Lock */
            if (cmd.pump_lock || cmd.target_state == SYS_STATE_ERROR)
            {
                /* Dry-run protection: Force Lock Pump */
                HAL_Pump_SetOutput(false);
            }
            else if (cmd.pump_pulse_en || cmd.target_state == SYS_STATE_FIRE_DETECTED)
            {
                /* Pulse Control: 0.5s Pulse Spray -> 1.0s Wait loop */
                HAL_Pump_SetOutput(true);
                vTaskDelay(pdMS_TO_TICKS(500)); // 0.5s mist spray

                HAL_Pump_SetOutput(false);
                vTaskDelay(pdMS_TO_TICKS(1000)); // 1.0s pause for re-measurement
            }
            else
            {
                HAL_Pump_SetOutput(false);
            }
        }
    }
}
