/**
 * @file app_protocol.cpp
 * @brief Server protocol implementation (docs/protocol.md)
 *
 * Telemetry keys (SCA_Y capabilities):
 *   temperature, humidity      temperature_humidity
 *   fire (0/1)                 fire_detect  (FSM state == FIRE_DETECTED)
 *   pump_on (0/1)              water_spray
 *   ash_level (%)              ash_level
 *   water_ok (0/1)             water_level  (float switch)
 *   led_color "#rrggbb"        rgb_led
 *   state                      FSM state name
 *   gas_raw, gas_diff          MQ-135 raw ADC / diff from adaptive baseline (not calibrated ppm)
 */

#include "app_protocol.h"
#include "task_state.h"
#include "hal_actuator.h"
#include <stdio.h>
#include <string.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <ArduinoJson.h>
  #include "freertos/FreeRTOS.h"
  #include "freertos/semphr.h"
#endif

static OtaRequest_t s_ota;
static volatile bool s_ota_pending = false;

extern "C" const char *App_StateName(SystemState_t state)
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

extern "C" size_t App_BuildTelemetry(char *buf, size_t len, uint32_t uptime_sec)
{
    const SystemState_t st = Task_State_GetCurrentState();
    SensorData_t d;
    Task_State_GetLatestData(&d);
    char led[8];
    HAL_LED_GetColorHex(led);

    const int n = snprintf(buf, len,
        "{\"device_id\":\"%s\",\"state\":\"%s\",\"temperature\":%.1f,\"humidity\":%.1f,"
        "\"fire\":%d,\"pump_on\":%d,\"ash_level\":%u,\"water_ok\":%d,\"led_color\":\"%s\","
        "\"gas_raw\":%u,\"gas_diff\":%d,\"uptime\":%lu}",
        SCA_DEVICE_ID, App_StateName(st), d.temp_current, d.humidity,
        st == SYS_STATE_FIRE_DETECTED ? 1 : 0, HAL_Pump_GetOutput() ? 1 : 0,
        d.ash_level_pct, d.water_empty ? 0 : 1, led,
        d.mq135_raw, d.mq135_diff, (unsigned long)uptime_sec);
    return (n > 0 && (size_t)n < len) ? (size_t)n : 0;
}

extern "C" size_t App_BuildEvent(char *buf, size_t len, SystemState_t from, SystemState_t to)
{
    SensorData_t d;
    Task_State_GetLatestData(&d);
    const int n = snprintf(buf, len,
        "{\"device_id\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"temperature\":%.1f,\"ash_level\":%u,\"gas_diff\":%d}",
        SCA_DEVICE_ID, App_StateName(from), App_StateName(to), d.temp_current, d.ash_level_pct, d.mq135_diff);
    return (n > 0 && (size_t)n < len) ? (size_t)n : 0;
}

extern "C" size_t App_BuildStatus(char *buf, size_t len, bool online, const char *ip)
{
    const int n = online
        ? snprintf(buf, len, "{\"status\":\"online\",\"ip\":\"%s\",\"fw\":\"%s\"}", ip ? ip : "", SCA_FW_VERSION)
        : snprintf(buf, len, "{\"status\":\"offline\"}");
    return (n > 0 && (size_t)n < len) ? (size_t)n : 0;
}

#if defined(ARDUINO)
static void set_err(char *err, size_t len, const char *msg)
{
    if (err && len) snprintf(err, len, "%s", msg);
}

static bool parse_hex_color(const char *s, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (!s || strlen(s) != 7 || s[0] != '#') return false;
    unsigned v = 0;
    for (int i = 1; i < 7; i++)
    {
        const char c = s[i];
        v <<= 4;
        if (c >= '0' && c <= '9') v |= (unsigned)(c - '0');
        else if (c >= 'a' && c <= 'f') v |= (unsigned)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') v |= (unsigned)(c - 'A' + 10);
        else return false;
    }
    *r = (uint8_t)(v >> 16); *g = (uint8_t)(v >> 8); *b = (uint8_t)v;
    return true;
}

static bool enqueue(const RemoteCmd_t *cmd, char *err, size_t err_len)
{
    if (!g_remote_queue || xQueueSend(g_remote_queue, cmd, 0) != pdTRUE)
    {
        set_err(err, err_len, "device busy");
        return false;
    }
    return true;
}
#endif

extern "C" bool App_Command_Dispatch(const char *json, size_t json_len, char *err, size_t err_len)
{
#if defined(ARDUINO)
    JsonDocument doc;
    if (deserializeJson(doc, json, json_len) != DeserializationError::Ok)
    {
        set_err(err, err_len, "invalid json");
        return false;
    }
    const char *action = doc["action"] | "";
    JsonVariantConst p = doc["params"];
    printf("[APP_CMD] action=%s req_id=%s\n", action, (const char *)(doc["req_id"] | "-"));

    if (strcmp(action, "spray") == 0)
    {
        const int ms = p["duration_ms"] | 1000;
        if (ms < 100 || ms > 10000) { set_err(err, err_len, "duration_ms 100~10000"); return false; }
        RemoteCmd_t c = {REMOTE_CMD_SPRAY, (uint16_t)ms, 0, 0, 0, 0};
        return enqueue(&c, err, err_len);
    }
    if (strcmp(action, "buzzer") == 0)
    {
        const int ms = p["duration_ms"] | 500;
        if (ms < 100 || ms > 5000) { set_err(err, err_len, "duration_ms 100~5000"); return false; }
        RemoteCmd_t c = {REMOTE_CMD_BUZZER, (uint16_t)ms, 0, 0, 0, 0};
        return enqueue(&c, err, err_len);
    }
    if (strcmp(action, "led") == 0)
    {
        RemoteCmd_t c = {REMOTE_CMD_LED, 0, 0, 0, 0, 0};
        if (!parse_hex_color(p["color"] | "#00ff88", &c.r, &c.g, &c.b)) { set_err(err, err_len, "color #rrggbb"); return false; }
        const int br = p["brightness"] | 128;
        if (br < 0 || br > 255) { set_err(err, err_len, "brightness 0~255"); return false; }
        c.brightness = (uint8_t)br;
        return enqueue(&c, err, err_len);
    }
    if (strcmp(action, "ota") == 0)
    {
        const char *url = p["url"] | "";
        const char *ver = p["version"] | "";
        const char *sha = p["sha256"] | "";
        if (strncmp(url, "https://", 8) != 0 || strlen(url) >= sizeof(s_ota.url)) { set_err(err, err_len, "bad url"); return false; }
        if (strlen(sha) != 64) { set_err(err, err_len, "bad sha256"); return false; }
        snprintf(s_ota.url, sizeof(s_ota.url), "%s", url);
        snprintf(s_ota.version, sizeof(s_ota.version), "%s", ver);
        snprintf(s_ota.sha256, sizeof(s_ota.sha256), "%s", sha);
        s_ota_pending = true;
        return true;
    }
    set_err(err, err_len, "unsupported action");
    return false;
#else
    (void)json; (void)json_len; (void)err; (void)err_len;
    return false;
#endif
}

extern "C" bool App_Ota_TakePending(OtaRequest_t *out)
{
    if (!s_ota_pending || !out) return false;
    *out = s_ota;
    s_ota_pending = false;
    return true;
}
