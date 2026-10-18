/**
 * @file task_mqtt.c
 * @brief Background MQTT Telemetry Task implementation (Priority 1 - Lowest)
 * @details Transmits telemetry data (AQI, water level, ash level, state) to web dashboard
 *          without blocking hardware fire extinguishing logic.
 *
 * TOPICS:
 *  - ashtray/v1/telemetry : Periodic state & sensor metrics (every 2s)
 *  - ashtray/v1/event     : Emergency event trigger (FIRE, WARNING, ASH_FULL, ERROR)
 */

#include "task_mqtt.h"
#include "task_state.h"
#include "hal_network.h"
#include <stdio.h>
#include <string.h>

#if defined(ESP_PLATFORM) || defined(ARDUINO)
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
#else
  #include "FreeRTOS.h"
  #include "task.h"
#endif

/* --------------------------------------------------------------------------
 * USER NETWORK CONFIGURATION (Can be changed anytime)
 * -------------------------------------------------------------------------- */
#define CONFIG_WIFI_SSID         "YOUR_WIFI_SSID"      /**< Change to your home Wi-Fi SSID */
#define CONFIG_WIFI_PASS         "YOUR_WIFI_PASSWORD"  /**< Change to your home Wi-Fi Password */

#define CONFIG_MQTT_BROKER_HOST  "broker.hivemq.com"   /**< Default public broker, change to Cafe24 IP later */
#define CONFIG_MQTT_BROKER_PORT  1883                  /**< Standard MQTT Port */
#define CONFIG_MQTT_CLIENT_ID    "SCA_NODE_DEV_01"

/* MQTT Topics */
#define TOPIC_TELEMETRY          "ashtray/v1/telemetry"
#define TOPIC_EVENT              "ashtray/v1/event"

static const char* Get_State_Str(SystemState_t state)
{
    switch (state)
    {
        case SYS_STATE_IDLE:          return "IDLE";
        case SYS_STATE_SMOKING:       return "SMOKING";
        case SYS_STATE_ASH_FULL:      return "ASH_FULL";
        case SYS_STATE_WARNING:       return "WARNING";
        case SYS_STATE_FIRE_DETECTED: return "FIRE_DETECTED";
        case SYS_STATE_ERROR:         return "ERROR";
        default:                      return "UNKNOWN";
    }
}

void Task_MQTT(void *pvParameters)
{
    (void)pvParameters;

    printf("[TASK_MQTT] Background Telemetry Task started (Priority 1 - Lowest)\n");

    /* 1. Configure and start Wi-Fi & MQTT Client */
    HAL_Network_MQTT_Config(CONFIG_MQTT_BROKER_HOST, CONFIG_MQTT_BROKER_PORT, CONFIG_MQTT_CLIENT_ID);
    HAL_Network_Init(CONFIG_WIFI_SSID, CONFIG_WIFI_PASS);

    SystemState_t prev_state = SYS_STATE_IDLE;
    char payload_buf[256];
    uint32_t uptime_sec = 0;

    while (1)
    {
        /* 2. Maintain MQTT connection loop (handles keep-alive and non-blocking reconnect) */
        bool mqtt_ready = HAL_Network_MQTT_Loop();

        /* 3. Fetch latest FSM state and sensor readings */
        SystemState_t current_state = Task_State_GetCurrentState();
        SensorData_t sensor_data;
        Task_State_GetLatestData(&sensor_data);

        if (mqtt_ready)
        {
            /* 4-A. Event Publish on State Transition */
            if (current_state != prev_state)
            {
                if (current_state == SYS_STATE_FIRE_DETECTED ||
                    current_state == SYS_STATE_WARNING ||
                    current_state == SYS_STATE_ASH_FULL ||
                    current_state == SYS_STATE_ERROR)
                {
                    snprintf(payload_buf, sizeof(payload_buf),
                             "{\"dev_id\":\"%s\",\"event\":\"%s\",\"temp\":%.1f,\"ash_pct\":%u}",
                             CONFIG_MQTT_CLIENT_ID, Get_State_Str(current_state),
                             sensor_data.temp_current, sensor_data.ash_level_pct);

                    HAL_Network_MQTT_Publish(TOPIC_EVENT, payload_buf);
                }
                prev_state = current_state;
            }

            /* 4-B. Periodic Telemetry Publish (every 2 seconds) */
            snprintf(payload_buf, sizeof(payload_buf),
                     "{\"dev_id\":\"%s\",\"state\":\"%s\",\"gas_raw\":%u,\"gas_diff\":%d,\"temp\":%.1f,\"ash_pct\":%u,\"echo_us\":%lu,\"water_empty\":%s,\"uptime\":%lu}",
                     CONFIG_MQTT_CLIENT_ID,
                     Get_State_Str(current_state),
                     sensor_data.mq135_raw,
                     sensor_data.mq135_diff,
                     sensor_data.temp_current,
                     sensor_data.ash_level_pct,
                     (unsigned long)sensor_data.ash_echo_us,
                     sensor_data.water_empty ? "true" : "false",
                     (unsigned long)uptime_sec);

            HAL_Network_MQTT_Publish(TOPIC_TELEMETRY, payload_buf);
        }

        uptime_sec += 2;

        /* 5. Telemetry transmission period: 2000ms (2 seconds) */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
