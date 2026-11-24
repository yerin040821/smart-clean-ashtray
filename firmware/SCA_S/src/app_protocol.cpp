/**
 * @file app_protocol.cpp
 * @brief SCA_S server protocol (docs/protocol.md)
 *
 * Telemetry keys → capabilities
 *   fire (0/1), flame (0/1)   fire_detect
 *   pump_on (0/1)             water_spray (extinguish pump)
 *   door_open (0/1)           door_control
 *   ash_level (%)             ash_level
 *   water_level (%), water_ok water_level (ESTIMATE from pump run-time; power-cycle after manual refill)
 *   sleeping (0/1)            sleep_mode
 *   led_color "#rrggbb"       rgb_led
 *   gas_raw, gas_diff         air_quality_monitor — same keys & algorithm as SCA_Y
 *   presence, state, uptime
 */

#include "app_protocol.h"
#include "tasks.h"
#include "hal_actuator.h"
#include "water_model.h"
#include <stdio.h>
#include <string.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <ArduinoJson.h>
#endif

static OtaRequest_t s_ota;
static volatile bool s_ota_pending = false;

extern "C" size_t App_BuildTelemetry(char *buf, size_t len, uint32_t uptime_sec)
{
    const SystemState_t st = Task_State_GetCurrentState();
    SensorData_t d;
    Task_State_GetLatestData(&d);
    char led[8];
    HAL_LED_GetColorHex(led);
    const uint8_t water = Water_GetPct();

    const int n = snprintf(buf, len,
        "{\"device_id\":\"%s\",\"state\":\"%s\",\"fire\":%d,\"flame\":%d,\"pump_on\":%d,"
        "\"door_open\":%d,\"ash_level\":%u,\"water_level\":%u,\"water_ok\":%d,\"sleeping\":%d,"
        "\"presence\":%d,\"led_color\":\"%s\",\"gas_raw\":%u,\"gas_diff\":%d,\"auto_sleep\":%d,\"uptime\":%lu}",
        SCA_DEVICE_ID, FSM_StateName(st), st == SYS_STATE_FIRE_DETECTED ? 1 : 0, d.flame ? 1 : 0,
        HAL_Pump_Get(PUMP_EXTINGUISH) ? 1 : 0, HAL_Lid_IsOpen() ? 1 : 0, d.ash_pct, water,
        water > WATER_LOW_PCT ? 1 : 0, st == SYS_STATE_SLEEP ? 1 : 0, d.presence ? 1 : 0, led,
        d.gas_raw, d.gas_diff, Task_Scheduler_GetAutoSleep() ? 1 : 0, (unsigned long)uptime_sec);
    return (n > 0 && (size_t)n < len) ? (size_t)n : 0;
}

extern "C" size_t App_BuildEvent(char *buf, size_t len, SystemState_t from, SystemState_t to)
{
    SensorData_t d;
    Task_State_GetLatestData(&d);
    const int n = snprintf(buf, len,
        "{\"device_id\":\"%s\",\"from\":\"%s\",\"to\":\"%s\",\"flame\":%d,\"gas_diff\":%d,\"ash_level\":%u,\"water_level\":%u}",
        SCA_DEVICE_ID, FSM_StateName(from), FSM_StateName(to), d.flame ? 1 : 0, d.gas_diff, d.ash_pct, Water_GetPct());
    return (n > 0 && (size_t)n < len) ? (size_t)n : 0;
}

extern "C" size_t App_BuildStatus(char *buf, size_t len, const char *status, const char *ip)
{
    const int n = (ip && ip[0])
        ? snprintf(buf, len, "{\"status\":\"%s\",\"ip\":\"%s\",\"fw\":\"%s\"}", status, ip, SCA_FW_VERSION)
        : snprintf(buf, len, "{\"status\":\"%s\"}", status);
    return (n > 0 && (size_t)n < len) ? (size_t)n : 0;
}

#if defined(ARDUINO)
static bool fail(char *err, size_t len, const char *msg)
{
    if (err && len) snprintf(err, len, "%s", msg);
    return false;
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

static bool enqueue(const RemoteCmd_t *c, char *err, size_t len)
{
    if (!g_remote_queue || xQueueSend(g_remote_queue, c, 0) != pdTRUE) return fail(err, len, "device busy");
    return true;
}

static bool duration_param(JsonVariantConst p, uint16_t def, uint16_t *out, char *err, size_t len)
{
    const int ms = p["duration_ms"] | (int)def;
    if (ms < 100 || ms > 10000) return fail(err, len, "duration_ms 100~10000");
    *out = (uint16_t)ms;
    return true;
}
#endif

extern "C" bool App_Command_Dispatch(const char *json, size_t json_len, char *err, size_t err_len)
{
#if defined(ARDUINO)
    JsonDocument doc;
    if (deserializeJson(doc, json, json_len) != DeserializationError::Ok) return fail(err, err_len, "invalid json");
    const char *action = doc["action"] | "";
    JsonVariantConst p = doc["params"];
    printf("[APP_CMD] action=%s req_id=%s\n", action, (const char *)(doc["req_id"] | "-"));

    RemoteCmd_t c;
    memset(&c, 0, sizeof(c));

    if (strcmp(action, "spray") == 0)
    {
        c.type = REMOTE_CMD_SPRAY;
        return duration_param(p, 1000, &c.duration_ms, err, err_len) && enqueue(&c, err, err_len);
    }
    if (strcmp(action, "deodorize") == 0)
    {
        c.type = REMOTE_CMD_DEODORIZE;
        return duration_param(p, 1500, &c.duration_ms, err, err_len) && enqueue(&c, err, err_len);
    }
    if (strcmp(action, "door") == 0)
    {
        c.type = REMOTE_CMD_DOOR;
        c.open = p["open"] | true;
        return enqueue(&c, err, err_len);
    }
    if (strcmp(action, "led") == 0)
    {
        c.type = REMOTE_CMD_LED;
        if (!parse_hex_color(p["color"] | "#00ff88", &c.r, &c.g, &c.b)) return fail(err, err_len, "color #rrggbb");
        const int br = p["brightness"] | 128;
        if (br < 0 || br > 255) return fail(err, err_len, "brightness 0~255");
        c.brightness = (uint8_t)br;
        return enqueue(&c, err, err_len);
    }
    if (strcmp(action, "clean") == 0)
    {
        Task_State_Request(FSM_REQ_CLEAN);   /* accepted only from IDLE / ASH_FULL by the FSM */
        return true;
    }
    if (strcmp(action, "sleep") == 0)
    {
        const bool enable = p["enable"] | true;
        Task_Scheduler_SetAutoSleep(enable);
        Task_State_Request(enable ? FSM_REQ_SLEEP : FSM_REQ_WAKE);
        return true;
    }
    if (strcmp(action, "ota") == 0)
    {
        const char *url = p["url"] | "";
        const char *ver = p["version"] | "";
        const char *sha = p["sha256"] | "";
        if (strncmp(url, "https://", 8) != 0 || strlen(url) >= sizeof(s_ota.url)) return fail(err, err_len, "bad url");
        if (strlen(sha) != 64) return fail(err, err_len, "bad sha256");
        snprintf(s_ota.url, sizeof(s_ota.url), "%s", url);
        snprintf(s_ota.version, sizeof(s_ota.version), "%s", ver);
        snprintf(s_ota.sha256, sizeof(s_ota.sha256), "%s", sha);
        s_ota_pending = true;
        return true;
    }
    return fail(err, err_len, "unsupported action");
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
