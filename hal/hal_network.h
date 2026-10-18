/**
 * @file hal_network.h
 * @brief Network and MQTT Abstraction Layer for ESP32 Wi-Fi & PubSubClient
 */

#ifndef HAL_NETWORK_H
#define HAL_NETWORK_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize Wi-Fi connection with non-blocking FreeRTOS watchdog safety
 * @param ssid Wi-Fi SSID
 * @param password Wi-Fi Password
 */
void HAL_Network_Init(const char *ssid, const char *password);

/**
 * @brief Check if Wi-Fi is currently connected
 * @return true Connected, false Disconnected
 */
bool HAL_Network_IsWifiConnected(void);

/**
 * @brief Configure MQTT Broker connection parameters
 * @param host IP address or hostname of the MQTT Broker (e.g., Cafe24 VPS IP)
 * @param port Port number (standard 1883)
 * @param client_id Unique MQTT Client ID
 */
void HAL_Network_MQTT_Config(const char *host, uint16_t port, const char *client_id);

/**
 * @brief Check and maintain MQTT broker connection (auto-reconnect)
 * @return true MQTT Connected, false Disconnected
 */
bool HAL_Network_MQTT_Loop(void);

/**
 * @brief Publish MQTT message string to a specified topic
 * @param topic MQTT Topic
 * @param payload JSON string payload
 * @return true Publish success, false Publish failure
 */
bool HAL_Network_MQTT_Publish(const char *topic, const char *payload);

#ifdef __cplusplus
}
#endif

#endif /* HAL_NETWORK_H */
