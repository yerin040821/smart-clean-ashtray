/**
 * @file water_model.c
 * @brief Extinguishing-water estimate (µL resolution, NVS persisted) — informational only
 */

#include "water_model.h"
#include "hal_config.h"
#include <stdio.h>

#define TANK_UL ((uint32_t)WATER_TANK_ML * 1000U)

static volatile uint32_t s_ul = TANK_UL;
static uint32_t s_saved_ul = TANK_UL;

void Water_Init(bool cold_boot)
{
    if (cold_boot)
    {
        /* manual refill workflow: fill tank → power-cycle → estimate restarts at 100% */
        s_ul = TANK_UL;
        HAL_Config_SetInt("water_ul", (int32_t)TANK_UL);
    }
    else
    {
        const int32_t v = HAL_Config_GetInt("water_ul", (int32_t)TANK_UL);
        s_ul = (v < 0) ? 0U : ((uint32_t)v > TANK_UL ? TANK_UL : (uint32_t)v);
    }
    s_saved_ul = s_ul;
    printf("[WATER] estimate %lu / %u mL (%u%%)%s\n", (unsigned long)(s_ul / 1000U), WATER_TANK_ML,
           Water_GetPct(), cold_boot ? " — reset on power-on" : "");
}

void Water_Consume(uint32_t pump_ms)
{
    const uint32_t used = pump_ms * PUMP_FLOW_ML_PER_S;   /* ms × mL/s = µL */
    const uint32_t cur = s_ul;
    s_ul = (used >= cur) ? 0U : cur - used;
}

uint8_t Water_GetPct(void)
{
    return (uint8_t)(((uint64_t)s_ul * 100U) / TANK_UL);
}

uint32_t Water_GetMl(void)
{
    return s_ul / 1000U;
}

void Water_Persist(void)
{
    const uint32_t cur = s_ul;
    const uint32_t delta = cur > s_saved_ul ? cur - s_saved_ul : s_saved_ul - cur;
    if (delta == 0 || (delta < TANK_UL / 100U && cur != 0U)) return;   /* ≥1% steps (flash wear) */
    HAL_Config_SetInt("water_ul", (int32_t)cur);
    s_saved_ul = cur;
}
