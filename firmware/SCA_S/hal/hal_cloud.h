/**
 * @file hal_cloud.h
 * @brief HTTPS calls to the platform API: device registration and OTA download
 */

#ifndef HAL_CLOUD_H
#define HAL_CLOUD_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief POST https://{api_host}/devices with the pairing token (docs/protocol.md §2)
 * @param[out] out_local_key 33-byte buffer for the issued local_key
 * @return HTTP status (201 = registered), or negative on network/TLS error
 */
int HAL_Cloud_Register(const char *api_host, const char *device_id, const char *type_id,
                       const char *pairing_token, const char *local_ip, const char *fw_version,
                       char *out_local_key, size_t key_len);

/**
 * @brief Download firmware over HTTPS, verify SHA-256 while streaming, then commit to the OTA partition.
 *        The running image is untouched unless the hash matches.
 * @return true when the new image is committed (caller should reboot)
 */
bool HAL_Cloud_Ota(const char *url, const char *sha256_hex, char *err, size_t err_len);

#ifdef __cplusplus
}
#endif

#endif /* HAL_CLOUD_H */
