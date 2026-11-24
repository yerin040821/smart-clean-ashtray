/**
 * @file hal_cloud.cpp
 * @brief HTTPS registration + SHA-256 verified OTA (HTTPClient / Update / mbedTLS md)
 */

#include "hal_cloud.h"
#include "hal_network.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <WiFiClientSecure.h>
  #include <HTTPClient.h>
  #include <Update.h>
  #include <ArduinoJson.h>
  #include "mbedtls/md.h"

  static void set_err(char *err, size_t len, const char *msg)
  {
      if (err && len) snprintf(err, len, "%s", msg);
      printf("[HAL_CLOUD] %s\n", msg);
  }
#endif

extern "C" int HAL_Cloud_Register(const char *api_host, const char *device_id, const char *type_id,
                                  const char *pairing_token, const char *local_ip, const char *fw_version,
                                  char *out_local_key, size_t key_len)
{
#if defined(ARDUINO)
    const char *ca = HAL_Network_ApiRootCA();
    if (!ca)
    {
        printf("[HAL_CLOUD] /api_ca.pem missing — cannot verify https://%s\n", api_host);
        return -1;
    }
    WiFiClientSecure tls;
    tls.setCACert(ca);
    HTTPClient http;
    char url[128];
    snprintf(url, sizeof(url), "https://%s/devices", api_host);
    if (!http.begin(tls, url)) return -2;
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(15000);

    JsonDocument req;
    req["device_id"] = device_id;
    req["type_id"] = type_id;
    req["pairing_token"] = pairing_token;
    req["local_ip"] = local_ip;
    req["fw_version"] = fw_version;
    String body;
    serializeJson(req, body);

    const int code = http.POST(body);
    if (code == 201)
    {
        JsonDocument res;
        if (deserializeJson(res, http.getString()) == DeserializationError::Ok)
        {
            const char *k = res["local_key"] | "";
            snprintf(out_local_key, key_len, "%s", k);
        }
    }
    else
    {
        printf("[HAL_CLOUD] register HTTP %d: %s\n", code, code > 0 ? http.getString().c_str() : "");
    }
    http.end();
    return code;
#else
    (void)api_host; (void)device_id; (void)type_id; (void)pairing_token; (void)local_ip; (void)fw_version;
    if (out_local_key && key_len) out_local_key[0] = '\0';
    return -1;
#endif
}

extern "C" bool HAL_Cloud_Ota(const char *url, const char *sha256_hex, char *err, size_t err_len)
{
#if defined(ARDUINO)
    if (!url || strncmp(url, "https://", 8) != 0) { set_err(err, err_len, "OTA url must be https"); return false; }
    if (!sha256_hex || strlen(sha256_hex) != 64)  { set_err(err, err_len, "OTA sha256 missing"); return false; }
    const char *ca = HAL_Network_ApiRootCA();
    if (!ca) { set_err(err, err_len, "api_ca.pem missing"); return false; }

    WiFiClientSecure tls;
    tls.setCACert(ca);
    HTTPClient http;
    if (!http.begin(tls, url)) { set_err(err, err_len, "http begin failed"); return false; }
    http.setTimeout(20000);
    const int code = http.GET();
    if (code != 200) { http.end(); set_err(err, err_len, "download HTTP error"); return false; }

    const int total = http.getSize();
    if (!Update.begin(total > 0 ? (size_t)total : UPDATE_SIZE_UNKNOWN))
    {
        http.end(); set_err(err, err_len, "no OTA partition space"); return false;
    }

    mbedtls_md_context_t md;
    mbedtls_md_init(&md);
    mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
    mbedtls_md_starts(&md);

    auto *stream = http.getStreamPtr();   /* NetworkClient* on core 3.x, WiFiClient* on 2.x */
    static uint8_t chunk[2048];
    size_t written = 0;
    uint32_t last_data = millis();
    while (http.connected() && (total <= 0 || written < (size_t)total))
    {
        const size_t avail = stream->available();
        if (avail)
        {
            const int n = stream->readBytes(chunk, avail > sizeof(chunk) ? sizeof(chunk) : avail);
            if (n <= 0) break;
            mbedtls_md_update(&md, chunk, (size_t)n);
            if (Update.write(chunk, (size_t)n) != (size_t)n) break;
            written += (size_t)n;
            last_data = millis();
        }
        else if (millis() - last_data > 15000)
        {
            break;   /* stalled */
        }
        else
        {
            delay(5);
        }
    }
    http.end();

    uint8_t digest[32];
    mbedtls_md_finish(&md, digest);
    mbedtls_md_free(&md);

    char hex[65];
    for (int i = 0; i < 32; i++) snprintf(&hex[i * 2], 3, "%02x", digest[i]);
    char expect[65];
    for (int i = 0; i < 64; i++) expect[i] = (char)tolower((unsigned char)sha256_hex[i]);
    expect[64] = '\0';

    if ((total > 0 && written != (size_t)total) || strcmp(hex, expect) != 0)
    {
        Update.abort();   /* keep running image */
        set_err(err, err_len, total > 0 && written != (size_t)total ? "download incomplete" : "sha256 mismatch");
        return false;
    }
    if (!Update.end(true))
    {
        set_err(err, err_len, "OTA finalize failed");
        return false;
    }
    printf("[HAL_CLOUD] OTA image verified (%u bytes, sha256 ok) — reboot required\n", (unsigned)written);
    return true;
#else
    (void)url; (void)sha256_hex; (void)err; (void)err_len;
    return false;
#endif
}
