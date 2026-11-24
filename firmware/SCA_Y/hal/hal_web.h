/**
 * @file hal_web.h
 * @brief On-device HTTP server (port 80)
 *
 *  Setup AP mode (http://192.168.4.1, captive portal):
 *    GET  /               Wi-Fi + pairing token form
 *    POST /setup          save to NVS → reboot
 *    POST /factory-reset  erase NVS → reboot
 *
 *  Station mode (same Wi-Fi as phone app, docs/protocol.md §3):
 *    GET  /api/status     telemetry JSON           (header X-Local-Key required)
 *    POST /api/command    {"action","params"}      (header X-Local-Key required)
 */

#ifndef HAL_WEB_H
#define HAL_WEB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*HAL_WebStatusCb)(char *buf, size_t len);
typedef bool   (*HAL_WebCommandCb)(const char *json, size_t len, char *err, size_t err_len);

void HAL_Web_Begin(const char *device_id, HAL_WebStatusCb on_status, HAL_WebCommandCb on_command);

/** @brief Update key used to authenticate local API requests ("" disables local API) */
void HAL_Web_SetLocalKey(const char *local_key);

/** @brief Call frequently (≤ 50ms) from the network task */
void HAL_Web_Loop(void);

/** @brief Register mDNS hostname (e.g. "ashtray-sca_y" → ashtray-sca_y.local) */
void HAL_Web_StartMDNS(const char *hostname);

#ifdef __cplusplus
}
#endif

#endif /* HAL_WEB_H */
