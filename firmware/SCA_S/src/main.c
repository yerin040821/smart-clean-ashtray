/**
 * @file main.c
 * @brief SCA_S firmware entry — HAL init, IPC queues, 5-Task FreeRTOS (P4/P3/P2/P1/P1)
 */

#include <stdio.h>
#include "tasks.h"
#include "app_protocol.h"
#include "hal_sensor.h"
#include "hal_actuator.h"
#include "hal_power.h"
#include "water_model.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

QueueHandle_t g_sensor_queue   = NULL;
QueueHandle_t g_actuator_queue = NULL;
QueueHandle_t g_remote_queue   = NULL;

#ifndef UNIT_TEST
/* Arduino core (C++) expects setup()/loop() with C++ mangled names */
void setup(void) asm("_Z5setupv");
void loop(void) asm("_Z4loopv");

void setup(void)
{
    /* 0. Release deep-sleep GPIO holds first, then force every output to a safe state */
    const WakeReason_t wake = HAL_Power_BootInit();
    Task_Scheduler_SetWakeReason(wake);
    HAL_Actuator_Init();
    HAL_Sensor_Init();
    Water_Init(wake == WAKE_POWER_ON);

    printf("\n=================================================================\n");
    printf(" Smart Clean Ashtray Firmware v%s [%s]  wake=%s\n", SCA_FW_VERSION, SCA_DEVICE_ID, HAL_Power_WakeName(wake));
    printf(" 10-State FSM / FreeRTOS 5-Task (State4 Actuator3 Sensor2 Comm1 Scheduler1)\n");
    printf("=================================================================\n");

    g_sensor_queue   = xQueueCreate(8, sizeof(SensorData_t));
    g_actuator_queue = xQueueCreate(8, sizeof(ActuatorCmd_t));
    g_remote_queue   = xQueueCreate(4, sizeof(RemoteCmd_t));
    if (!g_sensor_queue || !g_actuator_queue || !g_remote_queue)
    {
        printf("[SYS_FATAL] queue creation failed\n");
        return;
    }

    /* Stack sizes in bytes (ESP32 xTaskCreate takes bytes):
     *   State 4096   pure FSM + printf
     *   Actuator 8192 Adafruit GFX/SSD1306 call depth
     *   Sensor 4096  ADC / pulseIn
     *   Comm 12288   mbedTLS handshake, HTTPS, WebServer, ArduinoJson
     *   Scheduler 4096 */
    const BaseType_t ok =
        (xTaskCreate(Task_State,     "Task_State",     4096,  NULL, 4, NULL) == pdPASS) &
        (xTaskCreate(Task_Actuator,  "Task_Actuator",  8192,  NULL, 3, NULL) == pdPASS) &
        (xTaskCreate(Task_Sensor,    "Task_Sensor",    4096,  NULL, 2, NULL) == pdPASS) &
        (xTaskCreate(Task_Comm,      "Task_Comm",      12288, NULL, 1, NULL) == pdPASS) &
        (xTaskCreate(Task_Scheduler, "Task_Scheduler", 4096,  NULL, 1, NULL) == pdPASS);
    printf(ok ? "[SYS_INIT] 5 tasks created\n" : "[SYS_ERROR] task creation failed\n");
}

void loop(void)
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
#endif
