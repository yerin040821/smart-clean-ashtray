/**
 * @file hal_power.h
 * @brief Deep sleep (PIR ext0 wake + periodic safety timer wake)
 */

#ifndef HAL_POWER_H
#define HAL_POWER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { WAKE_POWER_ON = 0, WAKE_PIR, WAKE_TIMER, WAKE_OTHER } WakeReason_t;

/** @brief Call first thing at boot: releases GPIO holds from previous sleep, returns wake reason */
WakeReason_t HAL_Power_BootInit(void);

/**
 * @brief Enter deep sleep. Pumps are forced LOW and held (MOSFET gates must not float),
 *        wake on PIR HIGH or after timer_sec (safety re-check for smoldering butts).
 */
void HAL_Power_DeepSleep(uint32_t timer_sec);

const char *HAL_Power_WakeName(WakeReason_t r);

#ifdef __cplusplus
}
#endif

#endif /* HAL_POWER_H */
