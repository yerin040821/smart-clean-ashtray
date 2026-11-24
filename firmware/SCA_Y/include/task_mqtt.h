/**
 * @file task_mqtt.h
 * @brief Background MQTT Telemetry Task (Priority 1 - Lowest) header
 */

#ifndef TASK_MQTT_H
#define TASK_MQTT_H

#include "state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FreeRTOS Background MQTT Telemetry Task (Priority 1)
 * @param pvParameters Task context parameter (unused)
 */
void Task_MQTT(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* TASK_MQTT_H */
