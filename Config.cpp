#include "Config.h"
#include <Preferences.h>
#include <WiFi.h>

// ─── Runtime config globals ───────────────────────────────────
char cfg_device_type[32]    = "Sensor";
char cfg_device_address[16] = "B0E0";
char cfg_wifi_ssid[64]      = "";
char cfg_wifi_pass[64]      = "";
char cfg_mqtt_server[64]    = "";
int  cfg_mqtt_port          = 1883;
char cfg_mqtt_user[32]      = "";
char cfg_mqtt_pass[32]      = "";
char cfg_pub_topic[128]     = "";
char cfg_sub_topic[128]     = "";
char cfg_cust_topic[128]    = "";      // custom param topic

unsigned long cfg_telemetry_interval_ms = 5000UL;  // default 5 s

// ─── Custom parameter values ──────────────────────────────────
int   cfg_cust_data_int = 0;
float cfg_cust_test_par = 0.0f;

// ─── Derived ID globals ───────────────────────────────────────
char gw_id[13]         = "";
char mac_last4[5]      = "";
char topic_status[128] = "";
char topic_data[128]   = "";

// ─────────────────────────────────────────────────────────────
void setupConfig() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, true);

  strlcpy(cfg_device_type,    prefs.getString("dev_type",    "Sensor").c_str(), sizeof(cfg_device_type));
  strlcpy(cfg_device_address, prefs.getString("dev_addr",    "B0E0").c_str(),   sizeof(cfg_device_address));
  strlcpy(cfg_wifi_ssid,      prefs.getString("wifi_ssid",   "").c_str(),       sizeof(cfg_wifi_ssid));
  strlcpy(cfg_wifi_pass,      prefs.getString("wifi_pass",   "").c_str(),       sizeof(cfg_wifi_pass));
  strlcpy(cfg_mqtt_server,    prefs.getString("mqtt_server", "").c_str(),       sizeof(cfg_mqtt_server));
  cfg_mqtt_port =              prefs.getInt   ("mqtt_port",   1883);
  strlcpy(cfg_mqtt_user,      prefs.getString("mqtt_user",   "").c_str(),       sizeof(cfg_mqtt_user));
  strlcpy(cfg_mqtt_pass,      prefs.getString("mqtt_pass",   "").c_str(),       sizeof(cfg_mqtt_pass));
  strlcpy(cfg_pub_topic,      prefs.getString("pub_topic",   "").c_str(),       sizeof(cfg_pub_topic));
  strlcpy(cfg_sub_topic,      prefs.getString("sub_topic",   "").c_str(),       sizeof(cfg_sub_topic));
  strlcpy(cfg_cust_topic,     prefs.getString("cust_topic",  "").c_str(),       sizeof(cfg_cust_topic));

  // Telemetry interval — stored as seconds in NVS, converted to ms here
  uint32_t interval_s = prefs.getUInt("telem_interval", 5);   // default 5 s
  if (interval_s < 1)    interval_s = 1;     // floor: 1 second
  if (interval_s > 3600) interval_s = 3600;  // ceil:  1 hour
  cfg_telemetry_interval_ms = (unsigned long)interval_s * 1000UL;

  // Custom parameter values
  cfg_cust_data_int = prefs.getInt  ("cust_data_int", 0);
  cfg_cust_test_par = prefs.getFloat("cust_test_par", 0.0f);

  prefs.end();
  Serial.println(F("[Config] Preferences loaded from NVS."));
  Serial.printf("[Config] Telemetry interval: %lu ms\n", cfg_telemetry_interval_ms);
}

// ─────────────────────────────────────────────────────────────
void saveConfig() {
  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);

  Serial.println(F("--- Saving Configuration to NVS ---"));
  prefs.putString("dev_type",    cfg_device_type);
  prefs.putString("dev_addr",    cfg_device_address);
  prefs.putString("wifi_ssid",   cfg_wifi_ssid);
  prefs.putString("wifi_pass",   cfg_wifi_pass);
  prefs.putString("mqtt_server", cfg_mqtt_server);
  prefs.putInt   ("mqtt_port",   cfg_mqtt_port);
  prefs.putString("mqtt_user",   cfg_mqtt_user);
  prefs.putString("mqtt_pass",   cfg_mqtt_pass);
  prefs.putString("pub_topic",   cfg_pub_topic);
  prefs.putString("sub_topic",   cfg_sub_topic);
  prefs.putString("cust_topic",  cfg_cust_topic);

  // Store interval as seconds (UInt)
  uint32_t interval_s = (uint32_t)(cfg_telemetry_interval_ms / 1000UL);
  prefs.putUInt("telem_interval", interval_s);

  prefs.putInt  ("cust_data_int", cfg_cust_data_int);
  prefs.putFloat("cust_test_par", cfg_cust_test_par);

  prefs.end();

  Serial.printf("  device_type      : %s\n",  cfg_device_type);
  Serial.printf("  device_address   : %s\n",  cfg_device_address);
  Serial.printf("  mqtt_server      : %s\n",  cfg_mqtt_server);
  Serial.printf("  pub_topic        : %s\n",  cfg_pub_topic);
  Serial.printf("  cust_topic       : %s\n",  cfg_cust_topic);
  Serial.printf("  telem_interval   : %lu ms\n", cfg_telemetry_interval_ms);
  Serial.printf("  cust_data_int    : %d\n",  cfg_cust_data_int);
  Serial.printf("  cust_test_par    : %.2f\n",cfg_cust_test_par);
  Serial.println(F("--- All settings committed to NVS ---"));
}

// ─────────────────────────────────────────────────────────────
void buildMacIDs() {
  uint8_t mac[6];
  WiFi.macAddress(mac);

  snprintf(gw_id,     sizeof(gw_id),     "%02x%02x%02x%02x%02x%02x",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  snprintf(mac_last4, sizeof(mac_last4), "%02X%02X", mac[4], mac[5]);

  snprintf(topic_data,   sizeof(topic_data),   "sen/%s/%s/data",   gw_id, mac_last4);
  snprintf(topic_status, sizeof(topic_status), "sen/%s/%s/status", gw_id, mac_last4);

  auto replacePlus = [](char* buf, size_t len, const char* repl) {
    char tmp[128];
    strlcpy(tmp, buf, sizeof(tmp));
    char* pos = strchr(tmp, '+');
    if (pos) {
      *pos = '\0';
      snprintf(buf, len, "%s%s%s", tmp, repl, pos + 1);
    }
  };
  replacePlus(cfg_pub_topic,  sizeof(cfg_pub_topic),  mac_last4);
  replacePlus(cfg_sub_topic,  sizeof(cfg_sub_topic),  mac_last4);
  replacePlus(cfg_cust_topic, sizeof(cfg_cust_topic), mac_last4);

  // If cust_topic is empty after provisioning, auto-derive it from pub_topic
  // e.g. "SDTL/barn/EFD0/telemetry" → "SDTL/barn/EFD0/custom"
  if (strlen(cfg_cust_topic) == 0 && strlen(cfg_pub_topic) > 0) {
    String base = String(cfg_pub_topic);
    int lastSlash = base.lastIndexOf('/');
    String derived = (lastSlash != -1)
      ? base.substring(0, lastSlash) + "/custom"
      : base + "/custom";
    strlcpy(cfg_cust_topic, derived.c_str(), sizeof(cfg_cust_topic));
  }

  Serial.printf("[Config] Gateway ID   : %s\n", gw_id);
  Serial.printf("[Config] MAC last-4   : %s\n", mac_last4);
  Serial.printf("[Config] topic_data   : %s\n", topic_data);
  Serial.printf("[Config] topic_status : %s\n", topic_status);
  Serial.printf("[Config] cfg_pub_topic: %s\n", cfg_pub_topic);
  Serial.printf("[Config] cfg_cust_topic:%s\n", cfg_cust_topic);
}