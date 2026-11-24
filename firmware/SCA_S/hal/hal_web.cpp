/**
 * @file hal_web.cpp
 * @brief Setup portal + Local HTTP API + mDNS (WebServer / DNSServer / ESPmDNS)
 */

#include "hal_web.h"
#include "hal_config.h"
#include "hal_network.h"
#include <stdio.h>
#include <string.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <WiFi.h>
  #include <WebServer.h>
  #include <DNSServer.h>
  #include <ESPmDNS.h>

  static WebServer s_server(80);
  static DNSServer s_dns;
  static bool      s_dns_on = false;
  static bool      s_mdns_on = false;
  static char      s_local_key[33] = "";
  static char      s_device_id[24] = "";
  static HAL_WebStatusCb  s_on_status = NULL;
  static HAL_WebCommandCb s_on_command = NULL;
  static uint32_t  s_reboot_at = 0;

  static const char PORTAL_HTML[] PROGMEM = R"HTML(<!doctype html>
<html lang="ko"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Smart Clean Ashtray 설정</title>
<style>
body{font-family:system-ui,sans-serif;background:#f1f5f9;margin:0;padding:16px;color:#1e293b}
.c{max-width:420px;margin:0 auto;background:#fff;border-radius:16px;padding:24px;box-shadow:0 1px 3px #0002}
h1{font-size:20px;margin:0 0 4px}p{color:#64748b;font-size:14px}
label{display:block;font-size:14px;font-weight:600;margin:14px 0 4px}
input,textarea{width:100%;box-sizing:border-box;padding:10px;border:1px solid #cbd5e1;border-radius:8px;font-size:15px}
textarea{height:90px;font-family:monospace;font-size:12px}
button{width:100%;margin-top:18px;padding:12px;border:0;border-radius:8px;background:#059669;color:#fff;font-size:16px;font-weight:600}
.w{background:#fef3c7;color:#92400e;padding:8px 10px;border-radius:8px;font-size:13px}
.r{background:none;color:#94a3b8;font-size:13px;font-weight:400}
</style></head><body><div class="c">
<h1>🚭 %DEVICE% 설정</h1>
<p>집 WiFi 정보와 웹/앱의 <b>기기 등록</b> 화면에서 발급한 연동 토큰을 입력하세요.</p>
<div class="w">⚠️ 2.4GHz WiFi만 지원합니다 (5GHz 불가)</div>
<form method="post" action="/setup">
<label>WiFi 이름 (SSID)</label><input name="ssid" maxlength="32" required>
<label>WiFi 비밀번호</label><input name="pass" type="password" maxlength="64">
<label>연동 토큰</label><textarea name="token" maxlength="511" required></textarea>
<button>저장하고 연결</button></form>
<form method="post" action="/factory-reset" onsubmit="return confirm('모든 설정을 지울까요?')">
<button class="r">초기화</button></form>
</div></body></html>)HTML";

  static bool on_ap_side(void)
  {
      return HAL_Network_IsAPActive() && s_server.client().localIP() == WiFi.softAPIP();
  }

  static bool key_ok(void)
  {
      if (!s_local_key[0] || !s_server.hasHeader("X-Local-Key")) return false;
      String k = s_server.header("X-Local-Key");
      if (k.length() != strlen(s_local_key)) return false;
      uint8_t diff = 0;   /* constant-time compare */
      for (size_t i = 0; i < k.length(); i++) diff |= (uint8_t)(k[i] ^ s_local_key[i]);
      return diff == 0;
  }

  static void send_json(int code, const char *body)
  {
      s_server.send(code, "application/json", body);
  }

  static void handle_root(void)
  {
      if (on_ap_side())
      {
          String html = FPSTR(PORTAL_HTML);
          html.replace("%DEVICE%", s_device_id);
          s_server.send(200, "text/html; charset=utf-8", html);
      }
      else
      {
          s_server.send(404, "text/plain", "not found");
      }
  }

  static void handle_setup(void)
  {
      if (!on_ap_side()) { s_server.send(403, "text/plain", "forbidden"); return; }
      String ssid = s_server.arg("ssid"), pass = s_server.arg("pass"), token = s_server.arg("token");
      ssid.trim(); token.trim();
      if (ssid.isEmpty() || ssid.length() > 32 || pass.length() > 64 || token.isEmpty() || token.length() > 511)
      {
          s_server.send(400, "text/html; charset=utf-8", "<meta charset=utf-8><p>입력값을 확인하세요. <a href=/>돌아가기</a>");
          return;
      }
      HAL_Config_SaveProvisioning(ssid.c_str(), pass.c_str(), token.c_str());
      s_server.send(200, "text/html; charset=utf-8",
                    "<meta charset=utf-8><meta name=viewport content='width=device-width'>"
                    "<h2>저장했습니다 ✅</h2><p>기기가 재시작한 뒤 집 WiFi로 연결하고 계정에 자동 등록됩니다. "
                    "휴대폰 WiFi를 원래 네트워크로 바꾸고 앱/웹에서 확인하세요.</p>");
      s_reboot_at = millis() + 1500;
  }

  static void handle_factory_reset(void)
  {
      if (!on_ap_side()) { s_server.send(403, "text/plain", "forbidden"); return; }
      HAL_Config_FactoryReset();
      s_server.send(200, "text/html; charset=utf-8", "<meta charset=utf-8><h2>초기화했습니다. 재시작합니다.</h2>");
      s_reboot_at = millis() + 1500;
  }

  static void handle_status(void)
  {
      if (!key_ok()) { send_json(401, "{\"detail\":\"invalid local key\"}"); return; }
      static char buf[512];
      if (!s_on_status || s_on_status(buf, sizeof(buf)) == 0) { send_json(500, "{\"detail\":\"status unavailable\"}"); return; }
      send_json(200, buf);
  }

  static void handle_command(void)
  {
      if (!key_ok()) { send_json(401, "{\"detail\":\"invalid local key\"}"); return; }
      String body = s_server.arg("plain");
      char err[96] = "";
      if (s_on_command && s_on_command(body.c_str(), body.length(), err, sizeof(err)))
      {
          send_json(200, "{\"ok\":true}");
      }
      else
      {
          char out[160];
          snprintf(out, sizeof(out), "{\"ok\":false,\"detail\":\"%s\"}", err[0] ? err : "rejected");
          send_json(400, out);
      }
  }

  static void handle_not_found(void)
  {
      /* Captive portal: redirect any host to the setup page while on the AP */
      if (on_ap_side())
      {
          s_server.sendHeader("Location", "http://192.168.4.1/", true);
          s_server.send(302, "text/plain", "");
          return;
      }
      s_server.send(404, "text/plain", "not found");
  }
#endif

extern "C" void HAL_Web_Begin(const char *device_id, HAL_WebStatusCb on_status, HAL_WebCommandCb on_command)
{
#if defined(ARDUINO)
    snprintf(s_device_id, sizeof(s_device_id), "%s", device_id ? device_id : "");
    s_on_status = on_status;
    s_on_command = on_command;
    static const char *hdrs[] = {"X-Local-Key"};
    s_server.collectHeaders(hdrs, 1);
    s_server.on("/", HTTP_GET, handle_root);
    s_server.on("/setup", HTTP_POST, handle_setup);
    s_server.on("/factory-reset", HTTP_POST, handle_factory_reset);
    s_server.on("/api/status", HTTP_GET, handle_status);
    s_server.on("/api/command", HTTP_POST, handle_command);
    s_server.onNotFound(handle_not_found);
    s_server.begin();
    printf("[HAL_WEB] HTTP server started on :80\n");
#else
    (void)device_id; (void)on_status; (void)on_command;
#endif
}

extern "C" void HAL_Web_SetLocalKey(const char *local_key)
{
#if defined(ARDUINO)
    snprintf(s_local_key, sizeof(s_local_key), "%s", local_key ? local_key : "");
#else
    (void)local_key;
#endif
}

extern "C" void HAL_Web_Loop(void)
{
#if defined(ARDUINO)
    /* DNS hijack only while setup AP is up */
    if (HAL_Network_IsAPActive() && !s_dns_on)
    {
        s_dns.start(53, "*", WiFi.softAPIP());
        s_dns_on = true;
    }
    else if (!HAL_Network_IsAPActive() && s_dns_on)
    {
        s_dns.stop();
        s_dns_on = false;
    }
    if (s_dns_on) s_dns.processNextRequest();
    s_server.handleClient();

    if (s_reboot_at && (int32_t)(millis() - s_reboot_at) >= 0)
    {
        printf("[HAL_WEB] Rebooting to apply configuration...\n");
        delay(100);
        ESP.restart();
    }
#endif
}

extern "C" void HAL_Web_StartMDNS(const char *hostname)
{
#if defined(ARDUINO)
    if (s_mdns_on) return;
    if (MDNS.begin(hostname))
    {
        MDNS.addService("http", "tcp", 80);
        s_mdns_on = true;
        printf("[HAL_WEB] mDNS: http://%s.local\n", hostname);
    }
#else
    (void)hostname;
#endif
}
