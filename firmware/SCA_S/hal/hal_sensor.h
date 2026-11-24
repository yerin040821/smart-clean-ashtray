/**
 * @file hal_sensor.h
 * @brief SCA_S sensor HAL — MQ-135, flame sensor, PIR, HC-SR04
 */

#ifndef HAL_SENSOR_H
#define HAL_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Ash bin geometry (sensor at lid, measuring down) — calibrate per enclosure */
#ifndef ASH_EMPTY_MM
  #define ASH_EMPTY_MM 150U   /**< distance when bin is empty */
#endif
#ifndef ASH_FULL_MM
  #define ASH_FULL_MM   30U   /**< distance when bin is full */
#endif

void     HAL_Sensor_Init(void);
uint16_t HAL_MQ135_Read(void);           /**< averaged raw ADC 0~4095 */
bool     HAL_Flame_Read(void);           /**< true = flame detected */
bool     HAL_PIR_Read(void);             /**< true = presence */
uint16_t HAL_Ultrasonic_ReadMm(void);    /**< 0 on timeout */
uint8_t  HAL_AshPctFromMm(uint16_t mm);  /**< pure conversion (0 mm → 0%) */

#ifdef __cplusplus
}
#endif

#endif /* HAL_SENSOR_H */
