/**
 * @file main.c
 * @brief Smart Clean Ashtray (SCA) Main Firmware Entry Point
 * @details Initializes hardware HAL layers, creates IPC queues, and spawns the 4-Task
 *          FreeRTOS architecture with exact task priorities (4, 3, 2, 1).
 */

#include <stdio.h>
#include "hal_sensor.h"
#include "hal_actuator.h"
#include "state_machine.h"
#include "task_state.h"
#include "task_actuator.h"
#include "task_sensor.h"
#include "task_mqtt.h"

#if defined(ESP_PLATFORM) || defined(ARDUINO)
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "freertos/queue.h"
  #include <Arduino.h>
#else
  #include "FreeRTOS.h"
  #include "task.h"
  #include "queue.h"
#endif

/* Global FreeRTOS Inter-Task Communication Queue Handles */
QueueHandle_t g_sensor_queue = NULL;
QueueHandle_t g_actuator_queue = NULL;

#ifndef UNIT_TEST
/* Declare setup and loop with C++ symbol names expected by Arduino core main.cpp */
void setup(void) asm("_Z5setupv");
void loop(void) asm("_Z4loopv");

void setup(void)
{
    /* 1. System Banner */
    printf("\n");
    printf("=================================================================\n");
    printf(" Smart Clean Ashtray (SCA) Firmware v1.0\n");
    printf(" Architecture: 3-Layer (HAL, Service, Application FSM)\n");
    printf(" FreeRTOS 4-Task Prioritized Execution Engine Started\n");
    printf("=================================================================\n\n");

    /* 2. Initialize Hardware Abstraction Layer (HAL) */
    HAL_Sensor_Init();
    HAL_Actuator_Init();

    /* 3. Create FreeRTOS IPC Queues */
    g_sensor_queue = xQueueCreate(10, sizeof(SensorData_t));
    g_actuator_queue = xQueueCreate(10, sizeof(ActuatorCmd_t));

    if (g_sensor_queue == NULL || g_actuator_queue == NULL)
    {
        printf("[SYS_FATAL] Failed to create FreeRTOS Queues!\n");
        return;
    }

    /* 4. Create 4 FreeRTOS Tasks
     *
     * Stack sizing rationale (words, 1 word = 4 bytes on ESP32-S3):
     *   Task_State    : 6144B - FSM logic + printf formatting
     *   Task_Actuator : 8192B - Adafruit OLED C++ library (deep call chain: GFX->I2C->Wire)
     *                           Stack overflow here caused Interrupt WDT crash!
     *   Task_Sensor   : 6144B - DHT22 lib + pulseIn + printf
     *   Task_MQTT     : 4096B - vTaskDelay only, minimal stack needed
     */

    /* Priority 4 (HIGHEST): State Task - Brain FSM Engine */
    BaseType_t r1 = xTaskCreate(Task_State,    "Task_State",    6144, NULL, 4, NULL);

    /* Priority 3: Actuator Task - OLED(Adafruit) + Pump/LED/Buzzer */
    BaseType_t r2 = xTaskCreate(Task_Actuator, "Task_Actuator", 8192, NULL, 3, NULL);

    /* Priority 2: Sensor Task - DHT22 + HC-SR04 + MQ135 acquisition */
    BaseType_t r3 = xTaskCreate(Task_Sensor,   "Task_Sensor",   6144, NULL, 2, NULL);

    /* Priority 1 (LOWEST): MQTT Task - Background Telemetry */
    BaseType_t r4 = xTaskCreate(Task_MQTT,     "Task_MQTT",     4096, NULL, 1, NULL);

    if (r1 == pdPASS && r2 == pdPASS && r3 == pdPASS && r4 == pdPASS)
    {
        printf("[SYS_INIT] All 4 FreeRTOS Tasks Created Successfully.\n\n");
    }
    else
    {
        printf("[SYS_ERROR] FreeRTOS Task Creation Failed!\n\n");
    }
}

void loop(void)
{
    /* Yield main CPU thread to FreeRTOS scheduler */
    vTaskDelay(pdMS_TO_TICKS(1000));
}
#endif
