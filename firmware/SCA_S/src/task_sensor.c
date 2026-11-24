/**
 * @file task_sensor.c
 * @brief Sensor Task (Priority 2): 500ms sampling → g_sensor_queue
 *
 * Air quality — same algorithm as SCA_Y (firmware/SCA_Y/src/task_sensor.c):
 *   - baseline initialised from the first valid (non-saturated) reading
 *   - adaptive IIR baseline updated ONLY in IDLE so smoke never "becomes normal"
 *       SCA_Y: 1000ms, baseline = (b×9 + raw)/10   (α = 0.1/s)
 *       SCA_S:  500ms, baseline = (b×19 + raw)/20  (α = 0.05/sample ≈ same 0.1/s)
 *   - saturated ADC (4095) → baseline frozen; MQ135_SATURATE_MAX in a row → sensor_fault
 *   - gas_diff = raw − baseline; SMOKING > 50, cleared < 30 (shared thresholds)
 * Baseline lives in RTC slow memory so it survives deep sleep (SCA_S only).
 */

#include "tasks.h"
#include "hal_sensor.h"
#include "water_model.h"
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_attr.h"

RTC_DATA_ATTR static uint16_t s_baseline = 0;        /* 0 = not initialised */
static volatile SystemState_t s_state = SYS_STATE_IDLE;
static uint8_t s_sat_count = 0;

void Task_Sensor_NotifyState(SystemState_t st)
{
    s_state = st;
}

void Task_Sensor(void *pv)
{
    (void)pv;
    printf("[TASK_SENSOR] Sensor Task started (Priority 2, %ums)\n", SENSOR_PERIOD_MS);

    TickType_t last = xTaskGetTickCount();
    while (1)
    {
        SensorData_t d = {0};
        d.gas_raw = HAL_MQ135_Read();

        if (d.gas_raw >= MQ135_ADC_MAX)
        {
            if (s_sat_count < 255U) s_sat_count++;
            if (s_sat_count == 1U)
                printf("[HAL_ERROR] MQ-135 Raw=4095 (ADC MAX) — check wiring / 5V→3.3V divider on GPIO4\n");
            /* baseline frozen while saturated */
        }
        else
        {
            if (s_sat_count >= MQ135_SATURATE_MAX) printf("[HAL] MQ-135 recovered. Raw=%u\n", d.gas_raw);
            s_sat_count = 0;
            if (s_baseline == 0)
            {
                s_baseline = d.gas_raw ? d.gas_raw : 1U;
                printf("[SENSOR] MQ-135 baseline initialized: %u\n", s_baseline);
            }
            else if (s_state == SYS_STATE_IDLE)
            {
                s_baseline = (uint16_t)(((uint32_t)s_baseline * 19U + d.gas_raw) / 20U);
            }
        }
        d.sensor_fault = s_sat_count >= MQ135_SATURATE_MAX;
        d.gas_baseline = s_baseline;
        d.gas_diff = (int16_t)((int32_t)d.gas_raw - (int32_t)s_baseline);

        d.flame = HAL_Flame_Read();
        d.presence = HAL_PIR_Read();
        d.ash_distance_mm = HAL_Ultrasonic_ReadMm();
        d.ash_pct = HAL_AshPctFromMm(d.ash_distance_mm);
        d.water_pct = Water_GetPct();

        xQueueSend(g_sensor_queue, &d, 0);
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SENSOR_PERIOD_MS));
    }
}
