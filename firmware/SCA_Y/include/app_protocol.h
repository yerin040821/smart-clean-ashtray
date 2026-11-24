/**
 * @file app_protocol.h
 * @brief Server Protocol Layer (docs/protocol.md) - telemetry JSON builder & remote command dispatcher
 * @details Shared by MQTT (ashtray/{id}/command) and Local HTTP API (POST /api/command).
 *          Hardware commands are forwarded to Task_Actuator through g_remote_queue so that the
 *          actuator task remains the single owner of pump / buzzer / LED hardware.
 */

#ifndef APP_PROTOCOL_H
#define APP_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "state_machine.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── Device identity (override in platformio.ini build_flags) ─────────────── */
#ifndef SCA_DEVICE_ID
  #define SCA_DEVICE_ID   "SCA_Y"
#endif
#ifndef SCA_TYPE_ID
  #define SCA_TYPE_ID     "SCA_Y"
#endif
#ifndef SCA_FW_VERSION
  #define SCA_FW_VERSION  "1.1.0"
#endif

/** @brief Remote command types executed by Task_Actuator */
typedef enum {
    REMOTE_CMD_SPRAY = 0,   /**< Manual mist spray for duration_ms (blocked when water empty) */
    REMOTE_CMD_BUZZER,      /**< Beep for duration_ms */
    REMOTE_CMD_LED          /**< Set idle LED color override */
} RemoteCmdType_t;

typedef struct {
    RemoteCmdType_t type;
    uint16_t duration_ms;   /**< SPRAY 100~10000, BUZZER 100~5000 */
    uint8_t  r, g, b;       /**< LED color */
    uint8_t  brightness;    /**< LED brightness 0~255 */
} RemoteCmd_t;

/** @brief Pending OTA request (executed by network task) */
typedef struct {
    char url[192];
    char version[24];
    char sha256[65];
} OtaRequest_t;

/** Queue: network task / local API → Task_Actuator */
extern QueueHandle_t g_remote_queue;

/**
 * @brief Build telemetry JSON (protocol keys) from latest FSM state & sensor cache
 * @return number of bytes written (excluding NUL), 0 on overflow
 */
size_t App_BuildTelemetry(char *buf, size_t len, uint32_t uptime_sec);

/** @brief Build state transition event JSON {"from","to",...} */
size_t App_BuildEvent(char *buf, size_t len, SystemState_t from, SystemState_t to);

/** @brief Build retained status JSON {"status":"online","ip","fw"} */
size_t App_BuildStatus(char *buf, size_t len, bool online, const char *ip);

/**
 * @brief Parse & dispatch a command JSON {"action":"spray","params":{...}}
 * @param json      command payload (not necessarily NUL terminated)
 * @param json_len  payload length
 * @param err       optional error message buffer
 * @return true if accepted
 */
bool App_Command_Dispatch(const char *json, size_t json_len, char *err, size_t err_len);

/** @brief Take pending OTA request (set by "ota" command). Returns false if none. */
bool App_Ota_TakePending(OtaRequest_t *out);

/** @brief Convert FSM state to protocol string */
const char *App_StateName(SystemState_t state);

#ifdef __cplusplus
}
#endif

#endif /* APP_PROTOCOL_H */
