/**
 * @file task_mqtt.c
 * @brief Network Task (Priority 1 - Lowest): provisioning, registration, MQTT/TLS, Local API, OTA
 * @details Never blocks the fire-extinguishing path — FSM (P4) and actuators (P3) run independently,
 *          so the device keeps protecting itself even with no network at all.
 *
 * FLOW
 *   boot ─ NVS has Wi-Fi? ─no→ Setup AP "AshtraySetup_<ID>" (http://192.168.4.1)
 *            │yes
 *            ├→ STA connect (60s timeout → AP+STA so the user can fix credentials)
 *            ├→ pending pairing token? → POST https://api/devices → save local_key
 *            ├→ mDNS ashtray-<id>.local + Local HTTP API (X-Local-Key)
 *            └→ MQTT TLS 8883 (client cert CN = device_id)
 *                 pub  ashtray/<id>/telemetry  every 2s
 *                 pub  ashtray/<id>/event      on FSM transition
 *                 pub  ashtray/<id>/status     retained online / LWT offline
 *                 sub  ashtray/<id>/command    → App_Command_Dispatch
 *
 * TOPICS follow docs/protocol.md (legacy ashtray/v1/... topics removed).
 */

#include "task_mqtt.h"
#include "task_state.h"
#include "app_protocol.h"
#include "hal_network.h"
#include "hal_config.h"
#include "hal_web.h"
#include "hal_cloud.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if defined(ESP_PLATFORM) || defined(ARDUINO)
  #include "freertos/FreeRTOS.h"
  #include "freertos/task.h"
  #include "esp_system.h"
#else
  #include "FreeRTOS.h"
  #include "task.h"
#endif

/* --------------------------------------------------------------------------
 * SERVER CONFIGURATION (override with platformio.ini build_flags)
 * -------------------------------------------------------------------------- */
#ifndef SCA_API_HOST
  #define SCA_API_HOST       "api.smart-clean-ashtray.com"
#endif
#ifndef SCA_MQTT_HOST
  #define SCA_MQTT_HOST      "mqtt.smart-clean-ashtray.com"
#endif
#ifndef SCA_MQTT_PORT
  #define SCA_MQTT_PORT      8883
#endif
#ifndef SCA_AP_PASSWORD
  #define SCA_AP_PASSWORD    ""        /**< empty = open setup AP; >= 8 chars = WPA2 */
#endif

#define NET_TICK_MS              50U
#define TELEMETRY_PERIOD_MS    2000U
#define STA_TIMEOUT_MS        60000U
#define REGISTER_RETRY_MS     30000U

#define TOPIC_PREFIX "ashtray/" SCA_DEVICE_ID
static const char *TOPIC_TELEMETRY = TOPIC_PREFIX "/telemetry";
static const char *TOPIC_EVENT     = TOPIC_PREFIX "/event";
static const char *TOPIC_STATUS    = TOPIC_PREFIX "/status";
static const char *TOPIC_COMMAND   = TOPIC_PREFIX "/command";

static DeviceConfig_t s_cfg;
static char s_payload[512];

static uint32_t now_ms(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

/* ── MQTT callbacks (run inside HAL_Network_MQTT_Loop on this task) ────── */

static void on_mqtt_connect(void)
{
    char ip[16];
    HAL_Network_GetIP(ip, sizeof(ip));
    if (App_BuildStatus(s_payload, sizeof(s_payload), true, ip))
        HAL_Network_MQTT_Publish(TOPIC_STATUS, s_payload, true);
    HAL_Network_MQTT_Subscribe(TOPIC_COMMAND);
}

static void on_mqtt_message(const char *topic, const uint8_t *payload, unsigned int len)
{
    if (strcmp(topic, TOPIC_COMMAND) != 0) return;
    char err[96] = "";
    if (!App_Command_Dispatch((const char *)payload, len, err, sizeof(err)))
        printf("[TASK_NET] command rejected: %s\n", err);
}

/* ── Local HTTP API callbacks (run inside HAL_Web_Loop on this task) ───── */

static uint32_t s_uptime_sec = 0;

static size_t web_status(char *buf, size_t len)
{
    return App_BuildTelemetry(buf, len, s_uptime_sec);
}

/* ── Registration with pairing token ───────────────────────────────────── */

static void try_register(void)
{
    char ip[16];
    char key[33] = "";
    HAL_Network_GetIP(ip, sizeof(ip));
    const int code = HAL_Cloud_Register(SCA_API_HOST, SCA_DEVICE_ID, SCA_TYPE_ID, s_cfg.pairing_token,
                                        ip, SCA_FW_VERSION, key, sizeof(key));
    if (code == 201 && strlen(key) == 32)
    {
        HAL_Config_SaveLocalKey(key);
        snprintf(s_cfg.local_key, sizeof(s_cfg.local_key), "%s", key);
        s_cfg.pairing_token[0] = '\0';
        HAL_Web_SetLocalKey(key);
        printf("[TASK_NET] Registered to account ✔ (local API enabled)\n");
    }
    else if (code >= 400 && code < 500)
    {
        /* 401 expired token / 409 owned by another account → don't retry; re-run setup portal */
        HAL_Config_ClearPairingToken();
        s_cfg.pairing_token[0] = '\0';
        printf("[TASK_NET] Registration rejected (HTTP %d) — issue a new token and run setup again\n", code);
    }
    /* network/5xx errors: keep token, retry later */
}

/* ── OTA (refused during fire-related states) ──────────────────────────── */

static void run_ota(const OtaRequest_t *req)
{
    const SystemState_t st = Task_State_GetCurrentState();
    if (st == SYS_STATE_FIRE_DETECTED || st == SYS_STATE_WARNING || st == SYS_STATE_SMOKING)
    {
        printf("[TASK_NET] OTA %s refused: device in %s state\n", req->version, App_StateName(st));
        return;
    }
    printf("[TASK_NET] OTA → %s (%s)\n", req->version, req->url);
    char err[64] = "";
    if (HAL_Cloud_Ota(req->url, req->sha256, err, sizeof(err)))
    {
        if (App_BuildStatus(s_payload, sizeof(s_payload), false, NULL))
            HAL_Network_MQTT_Publish(TOPIC_STATUS, s_payload, true);
        vTaskDelay(pdMS_TO_TICKS(500));
#if defined(ESP_PLATFORM) || defined(ARDUINO)
        esp_restart();
#endif
    }
    else
    {
        printf("[TASK_NET] OTA failed: %s (keeping current firmware)\n", err);
    }
}

void Task_MQTT(void *pvParameters)
{
    (void)pvParameters;
    printf("[TASK_NET] Network Task started (Priority 1 - Lowest) device=%s fw=%s\n", SCA_DEVICE_ID, SCA_FW_VERSION);

    char ap_ssid[40];
    char mdns_host[40];
    snprintf(ap_ssid, sizeof(ap_ssid), "AshtraySetup_%s", SCA_DEVICE_ID);
    snprintf(mdns_host, sizeof(mdns_host), "ashtray-%s", SCA_DEVICE_ID);
    for (char *c = mdns_host; *c; c++) *c = (char)tolower((unsigned char)*c);

    /* 1. Config + local web server */
    HAL_Config_Load(&s_cfg);
#ifdef SCA_DEFAULT_WIFI_SSID
    /* Simulation build only ([env:wokwi]): join the simulator's virtual Wi-Fi instead of the setup AP */
    if (!s_cfg.wifi_ssid[0])
    {
        snprintf(s_cfg.wifi_ssid, sizeof(s_cfg.wifi_ssid), "%s", SCA_DEFAULT_WIFI_SSID);
        printf("[TASK_NET] SIM: using build-time Wi-Fi \"%s\"\n", s_cfg.wifi_ssid);
    }
#endif
    HAL_Web_Begin(SCA_DEVICE_ID, web_status, App_Command_Dispatch);
    HAL_Web_SetLocalKey(s_cfg.local_key);

    const bool certs_ok = HAL_Network_LoadCerts();
    char will[48];
    App_BuildStatus(will, sizeof(will), false, NULL);
    HAL_Network_MQTT_Config(SCA_MQTT_HOST, SCA_MQTT_PORT, SCA_DEVICE_ID, TOPIC_STATUS, will);
    HAL_Network_MQTT_SetCallbacks(on_mqtt_message, on_mqtt_connect);

    /* 2. Wi-Fi: no credentials → setup AP only */
    const uint32_t boot = now_ms();
    if (!s_cfg.wifi_ssid[0])
        HAL_Network_StartAP(ap_ssid, SCA_AP_PASSWORD);
    else
        HAL_Network_BeginSTA(s_cfg.wifi_ssid, s_cfg.wifi_pass);

    SystemState_t prev_state = Task_State_GetCurrentState();
    uint32_t last_telemetry = 0, last_register = 0, last_sec = now_ms();
    bool was_connected = false;

    while (1)
    {
        const uint32_t now = now_ms();
        if (now - last_sec >= 1000U) { s_uptime_sec++; last_sec += 1000U; }

        HAL_Web_Loop();

        const bool wifi = HAL_Network_IsWifiConnected();
        if (!wifi)
        {
            /* Credentials wrong / router gone: open setup AP (AP+STA keeps retrying) */
            if (s_cfg.wifi_ssid[0] && !HAL_Network_IsAPActive() && now - boot > STA_TIMEOUT_MS)
                HAL_Network_StartAP(ap_ssid, SCA_AP_PASSWORD);
            was_connected = false;
        }
        else
        {
            if (!was_connected)
            {
                char ip[16];
                HAL_Network_GetIP(ip, sizeof(ip));
                printf("[TASK_NET] Wi-Fi connected, IP=%s\n", ip);
                HAL_Network_StopAP();
                HAL_Web_StartMDNS(mdns_host);
                was_connected = true;
            }

            if (s_cfg.pairing_token[0] && (last_register == 0 || now - last_register > REGISTER_RETRY_MS))
            {
                last_register = now ? now : 1;
                try_register();
            }

            if (certs_ok && HAL_Network_MQTT_Loop())
            {
                const SystemState_t st = Task_State_GetCurrentState();
                if (st != prev_state)
                {
                    if (App_BuildEvent(s_payload, sizeof(s_payload), prev_state, st))
                        HAL_Network_MQTT_Publish(TOPIC_EVENT, s_payload, false);
                    prev_state = st;
                }
                if (now - last_telemetry >= TELEMETRY_PERIOD_MS)
                {
                    last_telemetry = now;
                    if (App_BuildTelemetry(s_payload, sizeof(s_payload), s_uptime_sec))
                        HAL_Network_MQTT_Publish(TOPIC_TELEMETRY, s_payload, false);
                }
            }

            OtaRequest_t ota;
            if (App_Ota_TakePending(&ota)) run_ota(&ota);
        }

        vTaskDelay(pdMS_TO_TICKS(NET_TICK_MS));
    }
}
