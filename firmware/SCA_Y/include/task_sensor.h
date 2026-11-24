/**
 * @file task_sensor.h
 * @brief Sensor Data Acquisition Task (0.5s period) header
 */

#ifndef TASK_SENSOR_H
#define TASK_SENSOR_H

#include "state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FreeRTOS Sensor Task function (Priority 2)
 * @param pvParameters Task context parameter (unused)
 */
void Task_Sensor(void *pvParameters);

/**
 * @brief Thread-safe update of current system state for baseline adaptive filtering
 * @param state Active system state from State Task
 */
void Task_Sensor_NotifyState(SystemState_t state);

#ifdef __cplusplus
}
#endif

#endif /* TASK_SENSOR_H */
