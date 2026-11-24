/**
 * @file task_actuator.c
 * @brief Actuator Task (Priority 3): single owner of pumps / lid / buzzer / LED / OLED
 * @details Non-blocking 50ms service loop (no vTaskDelay inside pump sequences), so a FIRE
 *          command is applied within one tick even while a manual spray is running.
 *
 * Safety rules
 *   - the extinguish pump is NEVER blocked by the water estimate (manual refill, no level sensor:
 *     a stale estimate must not stop fire suppression)
 *   - lid is forced CLOSED in FIRE / WARNING / CLEANING / DEODORIZING / SLEEP (remote override ignored)
 *   - remote spray is ignored during FIRE (FSM pulse loop already active)
 */

#include "tasks.h"
#include "hal_actuator.h"
#include "app_protocol.h"
#include "water_model.h"
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define TICK_MS                50U
#define PULSE_ON_MS           500U    /* FIRE: 0.5s mist */
#define PULSE_OFF_MS         1000U    /*       1.0s pause */
#define DEODORIZE_PUMP_MS    1500U    /* auto deodorize burst on DEODORIZING entry */
#define CLEAN_PUMP_MS        3000U    /* cleaning liquid burst on CLEANING entry */
#define LID_OVERRIDE_MS     60000U

static uint32_t now_ms(void) { return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS); }

static bool lid_forced_closed(SystemState_t s)
{
    return s == SYS_STATE_FIRE_DETECTED || s == SYS_STATE_WARNING || s == SYS_STATE_CLEANING ||
           s == SYS_STATE_DEODORIZING || s == SYS_STATE_SLEEP;
}

void Task_Actuator(void *pv)
{
    (void)pv;
    printf("[TASK_ACTUATOR] Actuator Task started (Priority 3)\n");

    ActuatorCmd_t cmd = {0};
    FSM_Outputs(SYS_STATE_IDLE, &cmd);
    cmd.data.water_pct = 100;
    SystemState_t prev_state = SYS_STATE_IDLE;

    uint32_t pulse_phase_start = 0; bool pulse_on = false;
    uint32_t deo_until = 0, clean_until = 0;          /* auto bursts */
    uint32_t man_spray_until = 0, man_deo_until = 0;  /* remote */
    uint32_t lid_override_until = 0; bool lid_override_open = false;
    uint32_t buzz_phase_start = 0; bool buzz_on = false;
    uint32_t last = now_ms();

    while (1)
    {
        const bool fresh = (xQueueReceive(g_actuator_queue, &cmd, pdMS_TO_TICKS(TICK_MS)) == pdTRUE);
        const uint32_t t = now_ms();
        const uint32_t dt = t - last;
        last = t;
        const SystemState_t st = cmd.state;

        /* ── state entry edges ── */
        if (st != prev_state)
        {
            if (st == SYS_STATE_DEODORIZING) deo_until = t + DEODORIZE_PUMP_MS;
            if (st == SYS_STATE_CLEANING)    clean_until = t + CLEAN_PUMP_MS;
            if (st == SYS_STATE_FIRE_DETECTED) { pulse_on = false; pulse_phase_start = t - PULSE_OFF_MS; man_spray_until = 0; }
            lid_override_until = 0;    /* any state change cancels lid override */
            prev_state = st;
        }

        /* ── remote commands ── */
        RemoteCmd_t rc;
        while (g_remote_queue && xQueueReceive(g_remote_queue, &rc, 0) == pdTRUE)
        {
            switch (rc.type)
            {
                case REMOTE_CMD_SPRAY:
                    if (st == SYS_STATE_FIRE_DETECTED) printf("[ACT] remote spray ignored: FIRE loop active\n");
                    else                               man_spray_until = t + rc.duration_ms;
                    break;
                case REMOTE_CMD_DEODORIZE:
                    if (st == SYS_STATE_SLEEP) printf("[ACT] remote deodorize ignored while sleeping\n");
                    else                       man_deo_until = t + rc.duration_ms;
                    break;
                case REMOTE_CMD_DOOR:
                    if (lid_forced_closed(st)) printf("[ACT] lid override ignored in %s\n", FSM_StateName(st));
                    else { lid_override_open = rc.open; lid_override_until = t + LID_OVERRIDE_MS; }
                    break;
                case REMOTE_CMD_LED:
                    HAL_LED_SetIdleColor(rc.r, rc.g, rc.b, rc.brightness);
                    break;
            }
        }

        /* ── extinguish pump: FIRE pulse loop or manual spray ── */
        bool ext_on = false;
        if (cmd.extinguish_pulse)
        {
            const uint32_t phase = t - pulse_phase_start;
            if (pulse_on && phase >= PULSE_ON_MS)        { pulse_on = false; pulse_phase_start = t; }
            else if (!pulse_on && phase >= PULSE_OFF_MS) { pulse_on = true;  pulse_phase_start = t; }
            ext_on = pulse_on;
        }
        else if ((int32_t)(man_spray_until - t) > 0)
        {
            ext_on = true;
        }
        if (HAL_Pump_Get(PUMP_EXTINGUISH)) Water_Consume(dt);
        HAL_Pump_Set(PUMP_EXTINGUISH, ext_on);

        /* ── deodorize / clean pumps ── */
        const bool locked = (st == SYS_STATE_SLEEP);
        HAL_Pump_Set(PUMP_DEODORIZE, !locked && ((cmd.deodorize_on && (int32_t)(deo_until - t) > 0) ||
                                                  (int32_t)(man_deo_until - t) > 0));
        HAL_Pump_Set(PUMP_CLEAN, !locked && cmd.clean_on && (int32_t)(clean_until - t) > 0);

        /* ── lid ── */
        bool lid = cmd.lid_open;
        if (lid_forced_closed(st)) lid = false;
        else if (lid_override_until && (int32_t)(lid_override_until - t) > 0) lid = lid_override_open;
        HAL_Lid_Set(lid);

        /* ── buzzer cadence ── */
        if (cmd.buzzer_mode == 0) { buzz_on = false; }
        else
        {
            const uint32_t on_ms = cmd.buzzer_mode == 2 ? 250U : 150U;
            const uint32_t off_ms = cmd.buzzer_mode == 2 ? 250U : 850U;
            const uint32_t phase = t - buzz_phase_start;
            if (buzz_on && phase >= on_ms)        { buzz_on = false; buzz_phase_start = t; }
            else if (!buzz_on && phase >= off_ms) { buzz_on = true;  buzz_phase_start = t; }
        }
        HAL_Buzzer_SetMode(buzz_on ? 1 : 0);

        /* ── LED / OLED (on each FSM cycle) ── */
        if (fresh)
        {
            if (st == SYS_STATE_SLEEP) { HAL_LED_Off(); HAL_OLED_Off(); }
            else { HAL_LED_SetStateColor(st); HAL_OLED_Update(st, &cmd.data); }
        }
    }
}
