/**
 * @file task_actuator.h
 * @brief Actuator Driving Task (Priority 3) header
 */

#ifndef TASK_ACTUATOR_H
#define TASK_ACTUATOR_H

#include "state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FreeRTOS Actuator Driving Task function (Priority 3)
 * @param pvParameters Task context parameter (unused)
 */
void Task_Actuator(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* TASK_ACTUATOR_H */
