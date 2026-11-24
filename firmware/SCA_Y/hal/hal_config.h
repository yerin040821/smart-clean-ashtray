/**
 * @file hal_config.h
 * @brief Persistent device configuration in NVS (ESP32 Preferences)
 * @details Stores Wi-Fi credentials entered in the AP setup portal, the one-time pairing token,
 *          and the local_key issued by the server at registration (Local HTTP API auth).
 */

#ifndef HAL_CONFIG_H
#define HAL_CONFIG_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char pairing_token[512];  /**< JWT from web /devices/setup — cleared after registration attempt */
    char local_key[33];       /**< 32 hex chars, issued by POST /devices */
} DeviceConfig_t;

/** @brief Load config from NVS (missing keys → empty strings) */
void HAL_Config_Load(DeviceConfig_t *cfg);

/** @brief Save Wi-Fi credentials + pairing token (from setup portal) */
bool HAL_Config_SaveProvisioning(const char *ssid, const char *pass, const char *pairing_token);

/** @brief Save local_key and clear pairing token (after successful registration) */
bool HAL_Config_SaveLocalKey(const char *local_key);

/** @brief Clear pairing token only (expired / rejected) */
void HAL_Config_ClearPairingToken(void);

/** @brief Erase everything (factory reset from setup portal) */
void HAL_Config_FactoryReset(void);

#ifdef __cplusplus
}
#endif

#endif /* HAL_CONFIG_H */
