/**
 * @file hal_sensor.h
 * @brief Hardware Abstraction Layer for SCA Sensors (MQ-135, DHT22, HC-SR04, FloatSwitch)
 */

#ifndef HAL_SENSOR_H
#define HAL_SENSOR_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize all sensor hardware pins (ADC, GPIO inputs, Pull-ups)
 */
void HAL_Sensor_Init(void);

/**
 * @brief Read MQ135 Gas Sensor Raw ADC value (GPIO5)
 * @return uint16_t Raw ADC value (0 - 4095)
 */
uint16_t HAL_MQ135_Read(void);

/**
 * @brief Read DHT22 Temperature and Humidity sensor (GPIO11)
 * @param[out] temp Current temperature in Celsius
 * @param[out] humidity Current relative humidity in %
 */
void HAL_DHT22_Read(float *temp, float *humidity);

/**
 * @brief Read HC-SR04 Ultrasonic Sensor and calculate ash level percentage (GPIO6 Trig, GPIO7 Echo)
 * @return uint8_t Ash level percentage (0% - 100%)
 */
uint8_t HAL_Ultrasonic_ReadLevelPct(void);

/**
 * @brief Read HC-SR04 Ultrasonic Sensor raw echo pulse width in microseconds
 * @return uint32_t Echo pulse width (us), or 0 on timeout
 */
uint32_t HAL_Ultrasonic_ReadRawDurationUs(void);

/**
 * @brief Read Water Float Switch status (GPIO8, Active LOW)
 * @return true Water is empty (switch LOW / open)
 * @return false Water is sufficient (switch HIGH)
 */
bool HAL_FloatSwitch_Read(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_SENSOR_H */
