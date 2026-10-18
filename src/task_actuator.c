/**
 * @file task_actuator.c
 * @brief Actuator Driving Task implementation (Priority 3)
 * @details Receives ActuatorCmd_t, drives OLED, WS2812B, Buzzer, and executes
 *          0.5s Pulse Spray pump control loop with Dry-run lock protection.
 */

#include "task_actuator.h"
#include "hal_actuator.h"
#include <stdio.h>

#if defined(ESP_PLATFORM) || defined(ARDUINO)
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
#else
  #include "FreeRTOS.h"
  #include "task.h"
#endif

void Task_Actuator(void *pvParameters)
{
    (void)pvParameters;

    ActuatorCmd_t cmd;

    printf("[TASK_ACTUATOR] Actuator Driving Task started (Priority 3)\n");

    while (1)
    {
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
