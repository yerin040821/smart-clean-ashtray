/**
 * @file hal_network.cpp
 * @brief Wi-Fi + MQTT/TLS implementation (WiFiClientSecure + PubSubClient)
 */

#include "hal_network.h"
#include <stdio.h>
#include <string.h>

#if defined(ARDUINO)
  #include <Arduino.h>
  #include <WiFi.h>
  #include <WiFiClientSecure.h>
  #include <PubSubClient.h>
  #include <LittleFS.h>

  static WiFiClientSecure s_tls;
  static PubSubClient     s_mqtt(s_tls);

  static String s_ca, s_cert, s_key, s_api_ca;
  static bool   s_certs_ok = false;
  static bool   s_ap_active = false;

  static char     s_host[64] = "";
  static uint16_t s_port = 8883;
  static char     s_client_id[32] = "";
  static char     s_will_topic[64] = "";
  static char     s_will_payload[64] = "";
  static uint32_t s_last_attempt = 0;

  static HAL_MqttMessageCb s_on_message = NULL;
  static HAL_MqttConnectCb s_on_connect = NULL;

  static String read_file(const char *path)
  {
      File f = LittleFS.open(path, "r");
      if (!f) return String();
      String s = f.readString();
      f.close();
      return s;
  }

  static void mqtt_trampoline(char *topic, uint8_t *payload, unsigned int len)
  {
      if (s_on_message) s_on_message(topic, payload, len);
  }
#endif

/* ─────────────────────────── Wi-Fi ─────────────────────────── */

extern "C" void HAL_Network_BeginSTA(const char *ssid, const char *password)
{
#if defined(ARDUINO)
    if (!ssid || !ssid[0]) return;
    printf("[HAL_NET] Connecting to Wi-Fi SSID: %s (2.4GHz only)\n", ssid);
    WiFi.mode(s_ap_active ? WIFI_AP_STA : WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(ssid, password);
#else
    (void)ssid; (void)password;
#endif
}

extern "C" bool HAL_Network_IsWifiConnected(void)
{
#if defined(ARDUINO)
    return WiFi.status() == WL_CONNECTED;
#else
    return false;
#endif
}

extern "C" void HAL_Network_StartAP(const char *ap_ssid, const char *ap_password)
{
#if defined(ARDUINO)
    if (s_ap_active) return;
    WiFi.mode(WiFi.getMode() == WIFI_STA ? WIFI_AP_STA : WIFI_AP);
    const bool has_pw = ap_password && strlen(ap_password) >= 8;  /* WPA2 requires >= 8 chars */
    WiFi.softAP(ap_ssid, has_pw ? ap_password : NULL);
    s_ap_active = true;
    printf("[HAL_NET] Setup AP started: SSID=%s (%s) → http://%s\n", ap_ssid,
           has_pw ? "WPA2" : "open", WiFi.softAPIP().toString().c_str());
#else
    (void)ap_ssid; (void)ap_password;
#endif
}

extern "C" void HAL_Network_StopAP(void)
{
#if defined(ARDUINO)
    if (!s_ap_active) return;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    s_ap_active = false;
    printf("[HAL_NET] Setup AP stopped\n");
#endif
}

extern "C" bool HAL_Network_IsAPActive(void)
{
#if defined(ARDUINO)
    return s_ap_active;
#else
    return false;
#endif
}

extern "C" void HAL_Network_GetIP(char *out, size_t len)
{
    if (!out || !len) return;
    out[0] = '\0';
#if defined(ARDUINO)
    if (WiFi.status() == WL_CONNECTED) snprintf(out, len, "%s", WiFi.localIP().toString().c_str());
#endif
}

/* ─────────────────────────── Certificates ─────────────────────────── */

extern "C" bool HAL_Network_LoadCerts(void)
{
#if defined(ARDUINO)
    if (!LittleFS.begin(false))
    {
        printf("[HAL_NET] LittleFS mount failed — run `pio run -t uploadfs` (see data/README.md)\n");
        return false;
    }
    s_ca     = read_file("/ca.crt");
    s_cert   = read_file("/client.crt");
    s_key    = read_file("/client.key");
    s_api_ca = read_file("/api_ca.pem");
    s_certs_ok = s_ca.length() && s_cert.length() && s_key.length();
    printf("[HAL_NET] Certs: mqtt_ca=%s client=%s key=%s api_ca=%s\n",
           s_ca.length() ? "ok" : "MISSING", s_cert.length() ? "ok" : "MISSING",
           s_key.length() ? "ok" : "MISSING", s_api_ca.length() ? "ok" : "MISSING");
    if (s_certs_ok)
    {
        s_tls.setCACert(s_ca.c_str());
        s_tls.setCertificate(s_cert.c_str());
        s_tls.setPrivateKey(s_key.c_str());
    }
    return s_certs_ok;
#else
    return false;
#endif
}

extern "C" const char *HAL_Network_ApiRootCA(void)
{
#if defined(ARDUINO)
    return s_api_ca.length() ? s_api_ca.c_str() : NULL;
#else
    return NULL;
#endif
}

/* ─────────────────────────── MQTT ─────────────────────────── */

extern "C" void HAL_Network_MQTT_Config(const char *host, uint16_t port, const char *client_id,
                                        const char *will_topic, const char *will_payload)
{
#if defined(ARDUINO)
    snprintf(s_host, sizeof(s_host), "%s", host ? host : "");
    s_port = port;
    snprintf(s_client_id, sizeof(s_client_id), "%s", client_id ? client_id : "");
    snprintf(s_will_topic, sizeof(s_will_topic), "%s", will_topic ? will_topic : "");
    snprintf(s_will_payload, sizeof(s_will_payload), "%s", will_payload ? will_payload : "");

    s_mqtt.setServer(s_host, s_port);
    s_mqtt.setBufferSize(1024);     /* default 256B is too small for telemetry/command JSON */
    s_mqtt.setKeepAlive(30);
    s_mqtt.setSocketTimeout(10);
    s_mqtt.setCallback(mqtt_trampoline);
    printf("[HAL_NET] MQTT target: %s:%u (ClientID: %s, TLS mutual auth)\n", s_host, s_port, s_client_id);
#else
    (void)host; (void)port; (void)client_id; (void)will_topic; (void)will_payload;
#endif
}

extern "C" void HAL_Network_MQTT_SetCallbacks(HAL_MqttMessageCb on_message, HAL_MqttConnectCb on_connect)
{
#if defined(ARDUINO)
    s_on_message = on_message;
    s_on_connect = on_connect;
#else
    (void)on_message; (void)on_connect;
#endif
}

extern "C" bool HAL_Network_MQTT_Loop(void)
{
#if defined(ARDUINO)
    if (WiFi.status() != WL_CONNECTED || !s_certs_ok || !s_host[0]) return false;

    if (!s_mqtt.connected())
    {
        const uint32_t now = millis();
        if (now - s_last_attempt < 5000) return false;
        s_last_attempt = now;
        printf("[HAL_NET] MQTT connecting to %s:%u ...\n", s_host, s_port);
        /* connect(id, user, pass, willTopic, willQos, willRetain, willMessage) */
        if (s_mqtt.connect(s_client_id, NULL, NULL, s_will_topic, 1, true, s_will_payload))
        {
            printf("[HAL_NET] MQTT connected (TLS)\n");
            if (s_on_connect) s_on_connect();
        }
        else
        {
            char err[96] = "";
            s_tls.lastError(err, sizeof(err));
            printf("[HAL_NET] MQTT connect failed rc=%d tls=\"%s\" — retry in 5s\n", s_mqtt.state(), err);
            return false;
        }
    }
    s_mqtt.loop();
    return s_mqtt.connected();
#else
    return false;
#endif
}

extern "C" bool HAL_Network_MQTT_Publish(const char *topic, const char *payload, bool retained)
{
#if defined(ARDUINO)
    if (!s_mqtt.connected()) return false;
    const bool ok = s_mqtt.publish(topic, payload, retained);
    if (!ok) printf("[HAL_MQTT_ERR] publish failed: %s\n", topic);
    return ok;
#else
    printf("[MOCK_MQTT_TX] %s %s\n", topic, payload);
    (void)retained;
    return true;
#endif
}

extern "C" bool HAL_Network_MQTT_Subscribe(const char *topic)
{
#if defined(ARDUINO)
    return s_mqtt.subscribe(topic, 1);
#else
    (void)topic;
    return true;
#endif
}
