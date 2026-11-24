/**
 * @file hal_config.cpp
 * @brief NVS-backed device configuration (Preferences namespace "sca")
 */

#include "hal_config.h"
#include <string.h>
#include <stdio.h>

#if defined(ARDUINO)
  #include <Preferences.h>
  static const char *NS = "sca";
#endif

extern "C" void HAL_Config_Load(DeviceConfig_t *cfg)
{
    if (!cfg) return;
    memset(cfg, 0, sizeof(*cfg));
#if defined(ARDUINO)
    Preferences p;
    p.begin(NS, true);
    p.getString("ssid", cfg->wifi_ssid, sizeof(cfg->wifi_ssid));
    p.getString("pass", cfg->wifi_pass, sizeof(cfg->wifi_pass));
    p.getString("ptoken", cfg->pairing_token, sizeof(cfg->pairing_token));
    p.getString("lkey", cfg->local_key, sizeof(cfg->local_key));
    p.end();
#endif
    printf("[HAL_CFG] ssid=%s paired=%s pending_token=%s\n",
           cfg->wifi_ssid[0] ? cfg->wifi_ssid : "(none)",
           cfg->local_key[0] ? "yes" : "no",
           cfg->pairing_token[0] ? "yes" : "no");
}

extern "C" bool HAL_Config_SaveProvisioning(const char *ssid, const char *pass, const char *pairing_token)
{
#if defined(ARDUINO)
    Preferences p;
    if (!p.begin(NS, false)) return false;
    p.putString("ssid", ssid ? ssid : "");
    p.putString("pass", pass ? pass : "");
    if (pairing_token && pairing_token[0]) p.putString("ptoken", pairing_token);
    p.end();
    return true;
#else
    (void)ssid; (void)pass; (void)pairing_token;
    return true;
#endif
}

extern "C" bool HAL_Config_SaveLocalKey(const char *local_key)
{
#if defined(ARDUINO)
    Preferences p;
    if (!p.begin(NS, false)) return false;
    p.putString("lkey", local_key ? local_key : "");
    p.remove("ptoken");
    p.end();
    return true;
#else
    (void)local_key;
    return true;
#endif
}

extern "C" void HAL_Config_ClearPairingToken(void)
{
#if defined(ARDUINO)
    Preferences p;
    if (p.begin(NS, false)) { p.remove("ptoken"); p.end(); }
#endif
}

extern "C" void HAL_Config_FactoryReset(void)
{
#if defined(ARDUINO)
    Preferences p;
    if (p.begin(NS, false)) { p.clear(); p.end(); }
#endif
    printf("[HAL_CFG] Factory reset — NVS cleared\n");
}

extern "C" int32_t HAL_Config_GetInt(const char *key, int32_t def)
{
#if defined(ARDUINO)
    Preferences p;
    if (!p.begin(NS, true)) return def;
    const int32_t v = p.getInt(key, def);
    p.end();
    return v;
#else
    (void)key;
    return def;
#endif
}

extern "C" void HAL_Config_SetInt(const char *key, int32_t value)
{
#if defined(ARDUINO)
    Preferences p;
    if (p.begin(NS, false)) { p.putInt(key, value); p.end(); }
#else
    (void)key; (void)value;
#endif
}
