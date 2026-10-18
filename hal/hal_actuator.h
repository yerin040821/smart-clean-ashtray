/**
 * @file hal_actuator.h
 * @brief Hardware Abstraction Layer for SCA Actuators (WS2812B, MOSFET Pump, Buzzer, OLED)
 */

#ifndef HAL_ACTUATOR_H
#define HAL_ACTUATOR_H

#include <stdint.h>
#include <stdbool.h>
#include "state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all actuator hardware pins (PWM, GPIO outputs, OLED I2C)
 */
void HAL_Actuator_Init(void);

/**
 * @brief Set WS2812B RGB LED color based on system state
 * @param state SystemState_t active state
 */
void HAL_LED_SetStateColor(SystemState_t state);

/**
 * @brief Control Water Pump MOSFET Output
 * @param enable true to turn pump ON, false to turn pump OFF
 */
void HAL_Pump_SetOutput(bool enable);

/**
 * @brief Control Warning Alarm Buzzer
 * @param mode 0: OFF, 1: Short Warning Beep, 2: Emergency Siren
 */
void HAL_Buzzer_SetMode(uint8_t mode);

/**
 * @brief Update OLED display UI screen according to system state and metrics
 * @param state Active system state
 * @param gas_diff Difference value for AQI graph
 * @param temp_current Current temperature
 * @param ash_pct Ash storage level percentage
 */
void HAL_OLED_UpdateUI(SystemState_t state, int16_t gas_diff, float temp_current, uint8_t ash_pct);

#ifdef __cplusplus
}
#endif

#endif /* HAL_ACTUATOR_H */
