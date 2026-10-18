/**
 * @file task_sensor.c
 * @brief Sensor Data Acquisition Task (Priority 2, 1000ms period)
 *
 * FIX LOG:
 *  - MQ-135 saturation guard: if raw == 4095 (ADC max), skip baseline init/IIR update
 *    and log hardware warning. Prevents baseline from converging to 4095 and masking smoke.
 *  - HC-SR04 3-sample median filter: rejects single-cycle outliers from vibration/no target.
 */

#include "task_sensor.h"
#include "hal_sensor.h"
#include <stdio.h>

#if defined(ESP_PLATFORM) || defined(ARDUINO)
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
#else
  #include "FreeRTOS.h"
  #include "task.h"
#endif

/* ADC 12-bit maximum value — used to detect MQ-135 saturation / wiring fault */
#define MQ135_ADC_MAX       4095U
/* If raw == ADC_MAX for this many consecutive cycles, flag hardware error */
#define MQ135_SATURATE_MAX  5U

static volatile SystemState_t s_reported_state    = SYS_STATE_IDLE;
static uint16_t               s_baseline          = 300;   /* Sane default clean-air baseline */
static bool                   s_baseline_init     = false;
static float                  s_prev_temp         = 25.0f;
static uint8_t                s_mq135_sat_count   = 0;     /* Consecutive saturated read counter */

void Task_Sensor_NotifyState(SystemState_t state)
{
    s_reported_state = state;
}

/* --------------------------------------------------------
 * Simple 3-element median for uint8_t
 * -------------------------------------------------------- */
static uint8_t median3_u8(uint8_t a, uint8_t b, uint8_t c)
{
    if ((a <= b && b <= c) || (c <= b && b <= a)) return b;
    if ((b <= a && a <= c) || (c <= a && a <= b)) return a;
    return c;
}

void Task_Sensor(void *pvParameters)
{
    (void)pvParameters;

    TickType_t       xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency    = pdMS_TO_TICKS(SENSOR_SAMPLING_PERIOD_MS);

    /* HC-SR04 3-sample ring buffer for median filter */
    uint8_t ash_ring[3] = {0, 0, 0};
    uint8_t ash_ring_idx = 0;

    while (1)
    {
        SensorData_t sensor_data;

        /* ── 1. HAL Acquisition ─────────────────────────────────── */
        uint16_t raw_mq135       = HAL_MQ135_Read();
        float    temp_current    = 25.0f;
        float    humidity_current= 50.0f;
        HAL_DHT22_Read(&temp_current, &humidity_current);
        bool     water_empty     = HAL_FloatSwitch_Read();

        /* HC-SR04 with 3-sample median filter */
        uint8_t raw_ash = HAL_Ultrasonic_ReadLevelPct();
        uint32_t echo_us = HAL_Ultrasonic_ReadRawDurationUs();
        ash_ring[ash_ring_idx] = raw_ash;
        ash_ring_idx = (ash_ring_idx + 1) % 3;
        uint8_t ash_pct = median3_u8(ash_ring[0], ash_ring[1], ash_ring[2]);

        /* ── 2. MQ-135 Saturation Guard ─────────────────────────── */
        bool mq135_saturated = (raw_mq135 >= MQ135_ADC_MAX);

        if (mq135_saturated)
        {
            s_mq135_sat_count++;
            if (s_mq135_sat_count == 1U)
            {
                printf("[HAL_ERROR] MQ-135 Raw=4095 (ADC MAX)! "
                       "Check GPIO%d wiring: AOUT->LevelShift->GPIO5, VCC=5V, GND.\n",
                       5 /* PIN_MQ135_ANALOG */);
            }
            if (s_mq135_sat_count >= MQ135_SATURATE_MAX)
            {
                /* Print reminder every 30 cycles after first alert */
                if ((s_mq135_sat_count % 30) == 0)
                {
                    printf("[HAL_ERROR] MQ-135 still saturated (%u cycles). Hardware fault!\n",
                           (unsigned)s_mq135_sat_count);
                }
            }
            /* Do NOT update baseline when saturated — preserve last valid baseline */
        }
        else
        {
            /* Valid reading: reset saturation counter */
            if (s_mq135_sat_count > 0)
            {
                printf("[HAL] MQ-135 recovered. Raw=%u\n", (unsigned)raw_mq135);
            }
            s_mq135_sat_count = 0;

            /* ── 3. Baseline Init (first valid non-saturated reading) ── */
            if (!s_baseline_init)
            {
                s_baseline      = raw_mq135;
                s_baseline_init = true;
                printf("[SENSOR] MQ-135 baseline initialized: %u\n", (unsigned)s_baseline);
            }

            /* ── 4. Adaptive IIR Baseline (IDLE state only) ──────── */
            if (s_reported_state == SYS_STATE_IDLE)
            {
                /* IIR: baseline = (baseline * 9 + raw) / 10  (alpha = 0.1) */
                s_baseline = (uint16_t)(((uint32_t)s_baseline * 9U + raw_mq135) / 10U);
            }
        }

        /* ── 5. Diff & Temperature Rate of Change ───────────────── */
        int16_t diff      = (int16_t)raw_mq135 - (int16_t)s_baseline;
        float   temp_rate = (temp_current - s_prev_temp) / (SENSOR_SAMPLING_PERIOD_MS / 1000.0f);
        s_prev_temp       = temp_current;

        /* ── 6. Pack SensorData_t ────────────────────────────────── */
        sensor_data.mq135_raw      = raw_mq135;
        sensor_data.mq135_baseline = s_baseline;
        sensor_data.mq135_diff     = diff;
        sensor_data.temp_current   = temp_current;
        sensor_data.temp_rate      = temp_rate;
        sensor_data.ash_level_pct  = ash_pct;
        sensor_data.ash_echo_us    = echo_us;
        sensor_data.water_empty    = water_empty;

        /* ── 7. Send to State Task via IPC Queue ─────────────────── */
        if (g_sensor_queue != NULL)
        {
            xQueueSend(g_sensor_queue, &sensor_data, 0);
        }

        /* ── 8. Periodic timing (vTaskDelayUntil for precise period) */
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}
