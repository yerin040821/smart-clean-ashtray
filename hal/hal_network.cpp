/**
 * @file hal_network.cpp
 * @brief Network & MQTT Implementation using WiFi.h and PubSubClient
 */

#include "hal_network.h"
#include <stdio.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <WiFi.h>
  #include <PubSubClient.h>

  static WiFiClient   s_wifi_client;
  static PubSubClient s_mqtt_client(s_wifi_client);

  static char s_mqtt_host[64] = "broker.hivemq.com"; // Fallback default
  static uint16_t s_mqtt_port = 1883;
  static char s_client_id[32] = "SCA_ESP32_DEV";
  static uint32_t s_last_reconnect_attempt = 0;
#endif

extern "C" void HAL_Network_Init(const char *ssid, const char *password)
{
#if defined(ARDUINO)
    if (ssid == NULL || strlen(ssid) == 0)
    {
        printf("[HAL_NET] Wi-Fi SSID empty, skipping network init.\n");
        return;
    }

    printf("[HAL_NET] Connecting to Wi-Fi SSID: %s ...\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    s_mqtt_client.setServer(s_mqtt_host, s_mqtt_port);
#else
    (void)ssid; (void)password;
#endif
}

extern "C" bool HAL_Network_IsWifiConnected(void)
{
#if defined(ARDUINO)
    return (WiFi.status() == WL_CONNECTED);
#else
    return false;
#endif
}

extern "C" void HAL_Network_MQTT_Config(const char *host, uint16_t port, const char *client_id)
{
#if defined(ARDUINO)
    if (host) strncpy(s_mqtt_host, host, sizeof(s_mqtt_host) - 1);
    s_mqtt_port = port;
    if (client_id) strncpy(s_client_id, client_id, sizeof(s_client_id) - 1);

    s_mqtt_client.setServer(s_mqtt_host, s_mqtt_port);
    printf("[HAL_NET] MQTT Target Configured: %s:%u (ClientID: %s)\n",
           s_mqtt_host, s_mqtt_port, s_client_id);
#else
    (void)host; (void)port; (void)client_id;
#endif
}

extern "C" bool HAL_Network_MQTT_Loop(void)
{
#if defined(ARDUINO)
    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    if (!s_mqtt_client.connected())
    {
        uint32_t now = millis();
        // Non-blocking reconnection rate limit (every 5 seconds)
        if (now - s_last_reconnect_attempt > 5000)
        {
            s_last_reconnect_attempt = now;
            printf("[HAL_NET] Attempting MQTT connection to %s...\n", s_mqtt_host);

            if (s_mqtt_client.connect(s_client_id))
            {
                printf("[HAL_NET] MQTT Connected Successfully!\n");
            }
            else
            {
                printf("[HAL_NET] MQTT Connect Failed, rc=%d. Will retry.\n", s_mqtt_client.state());
            }
        }
        return false;
    }

    s_mqtt_client.loop();
    return true;
#else
    return false;
#endif
}

extern "C" bool HAL_Network_MQTT_Publish(const char *topic, const char *payload)
{
#if defined(ARDUINO)
    if (!s_mqtt_client.connected())
    {
        return false;
    }

    bool ok = s_mqtt_client.publish(topic, payload);
    if (ok)
    {
        printf("[HAL_MQTT_TX] Topic: %s | Payload: %s\n", topic, payload);
    }
    else
    {
        printf("[HAL_MQTT_ERR] Failed to publish on topic: %s\n", topic);
    }
    return ok;
#else
    printf("[MOCK_MQTT_TX] Topic: %s | Payload: %s\n", topic, payload);
    return true;
#endif
}
