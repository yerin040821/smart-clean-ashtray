/**
 * @file hal_network.h
 * @brief Network Abstraction Layer — Wi-Fi (STA / setup AP) and MQTT over TLS (mutual auth)
 *
 * Certificates are read from LittleFS (upload with `pio run -t uploadfs`, see data/README.md):
 *   /ca.crt      MQTT broker CA (self-signed SCA-MQTT-CA)
 *   /client.crt  this device's client certificate (CN = device_id)
 *   /client.key  this device's private key
 *   /api_ca.pem  Root CA for https://api.* (Let's Encrypt ISRG Root X1) — registration & OTA
 */

#ifndef HAL_NETWORK_H
#define HAL_NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*HAL_MqttMessageCb)(const char *topic, const uint8_t *payload, unsigned int len);
typedef void (*HAL_MqttConnectCb)(void);

/* ── Wi-Fi ─────────────────────────────────────────────────────────────── */

/** @brief Start station connection (non-blocking) */
void HAL_Network_BeginSTA(const char *ssid, const char *password);

/** @brief Check if Wi-Fi station is connected */
bool HAL_Network_IsWifiConnected(void);

/** @brief Start setup access point (keeps STA running if already configured → AP+STA) */
void HAL_Network_StartAP(const char *ap_ssid, const char *ap_password);

/** @brief Stop setup access point */
void HAL_Network_StopAP(void);

/** @brief Whether setup AP is active */
bool HAL_Network_IsAPActive(void);

/** @brief Station IPv4 address as text ("" when disconnected) */
void HAL_Network_GetIP(char *out, size_t len);

/* ── Certificates ──────────────────────────────────────────────────────── */

/** @brief Mount LittleFS and load TLS material. @return true if MQTT certs are present */
bool HAL_Network_LoadCerts(void);

/** @brief Root CA PEM for HTTPS API calls (NULL if /api_ca.pem missing) */
const char *HAL_Network_ApiRootCA(void);

/* ── MQTT (TLS 8883, client certificate) ───────────────────────────────── */

/**
 * @brief Configure broker and Last-Will (retained) message
 * @param will_topic   e.g. "ashtray/SCA_Y/status"
 * @param will_payload e.g. "{\"status\":\"offline\"}"
 */
void HAL_Network_MQTT_Config(const char *host, uint16_t port, const char *client_id,
                             const char *will_topic, const char *will_payload);

void HAL_Network_MQTT_SetCallbacks(HAL_MqttMessageCb on_message, HAL_MqttConnectCb on_connect);

/** @brief Maintain connection (non-blocking reconnect every 5s). @return true when connected */
bool HAL_Network_MQTT_Loop(void);

bool HAL_Network_MQTT_Publish(const char *topic, const char *payload, bool retained);

bool HAL_Network_MQTT_Subscribe(const char *topic);

#ifdef __cplusplus
}
#endif

#endif /* HAL_NETWORK_H */
