/**
 * @file hal_actuator.h
 * @brief SCA_S actuator HAL — extinguish / deodorize / clean pumps, lid servo, WS2812B, buzzer, SSD1306
 */

#ifndef HAL_ACTUATOR_H
#define HAL_ACTUATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { PUMP_EXTINGUISH = 0, PUMP_DEODORIZE, PUMP_CLEAN, PUMP_COUNT } Pump_t;

/* Lid servo pulse widths (µs) — calibrate per mechanism */
#ifndef SERVO_CLOSED_US
  #define SERVO_CLOSED_US 1000U
#endif
#ifndef SERVO_OPEN_US
  #define SERVO_OPEN_US   2000U
#endif

void HAL_Actuator_Init(void);

void HAL_Pump_Set(Pump_t pump, bool on);
bool HAL_Pump_Get(Pump_t pump);
bool HAL_Pump_AnyOn(void);
void HAL_Pump_AllOff(void);

void HAL_Lid_Set(bool open);
bool HAL_Lid_IsOpen(void);

void HAL_Buzzer_SetMode(uint8_t mode);   /**< 0 off, 1 beep, 2 siren (toggled by caller cadence) */

void HAL_LED_SetStateColor(SystemState_t state);
void HAL_LED_SetIdleColor(uint8_t r, uint8_t g, uint8_t b, uint8_t brightness);
void HAL_LED_Off(void);
void HAL_LED_GetColorHex(char out[8]);

void HAL_OLED_Update(SystemState_t state, const SensorData_t *d);
void HAL_OLED_Off(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ACTUATOR_H */
