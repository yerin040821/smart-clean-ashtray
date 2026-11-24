/**
 * @file app_protocol.h
 * @brief Server protocol (docs/protocol.md) for SCA_S — telemetry builder & command dispatcher
 *
 * Commands (MQTT ashtray/SCA_S/command or POST /api/command):
 *   spray        {duration_ms 100~10000}   manual extinguish pump (ignored during FIRE: auto loop active)
 *   deodorize    {duration_ms 100~10000}   deodorizer pump
 *   clean        {}                        run cleaning cycle (FSM CLEANING)
 *   door         {open: bool}              lid override (ignored during FIRE / CLEANING / DEODORIZING)
 *   sleep        {enable: bool}            true → sleep now & auto-sleep on, false → auto-sleep off
 *   led          {color "#rrggbb", brightness 0~255}
 *   ota          {url, version, sha256}
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

#ifndef SCA_DEVICE_ID
  #define SCA_DEVICE_ID  "SCA_S"
#endif
#ifndef SCA_TYPE_ID
  #define SCA_TYPE_ID    "SCA_S"
#endif
#ifndef SCA_FW_VERSION
  #define SCA_FW_VERSION "1.0.0"
#endif

typedef enum {
    REMOTE_CMD_SPRAY = 0,
    REMOTE_CMD_DEODORIZE,
    REMOTE_CMD_DOOR,
    REMOTE_CMD_LED,
} RemoteCmdType_t;

typedef struct {
    RemoteCmdType_t type;
    uint16_t duration_ms;
    bool     open;
    uint8_t  r, g, b, brightness;
} RemoteCmd_t;

typedef struct {
    char url[192];
    char version[24];
    char sha256[65];
} OtaRequest_t;

size_t App_BuildTelemetry(char *buf, size_t len, uint32_t uptime_sec);
size_t App_BuildEvent(char *buf, size_t len, SystemState_t from, SystemState_t to);
size_t App_BuildStatus(char *buf, size_t len, const char *status, const char *ip);

bool App_Command_Dispatch(const char *json, size_t json_len, char *err, size_t err_len);
bool App_Ota_TakePending(OtaRequest_t *out);

#ifdef __cplusplus
}
#endif

#endif /* APP_PROTOCOL_H */
