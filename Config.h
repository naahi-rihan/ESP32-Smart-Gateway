#pragma once
#include <Arduino.h>

// ─── Firmware version ────────────────────────────────────────
#define FW_VERSION "1.0.4"

// ─── Boot button ─────────────────────────────────────────────
#define BOOT_BUTTON 0

// ─── NVS namespace ───────────────────────────────────────────
#define NVS_NAMESPACE "sdtl_cfg"

// ─── Runtime config (loaded from NVS) ────────────────────────
extern char cfg_device_type[32];
extern char cfg_device_address[16];
extern char cfg_wifi_ssid[64];
extern char cfg_wifi_pass[64];
extern char cfg_mqtt_server[64];
extern int  cfg_mqtt_port;
extern char cfg_mqtt_user[32];
extern char cfg_mqtt_pass[32];
extern char cfg_pub_topic[128];      // telemetry publish topic
extern char cfg_sub_topic[128];      // command subscribe topic
extern char cfg_cust_topic[128];     // custom parameter publish topic (NEW)

// ─── Telemetry interval (ms) — configurable by user ──────────
// Default 5000 ms. User sets via web portal (5s … 3600s).
extern unsigned long cfg_telemetry_interval_ms;

// ─── Custom parameter fields (saved in NVS) ──────────────────
// Two user-defined fields sent under cfg_cust_topic.
// data_int  → integer  (e.g. 28)
// test_par  → float    (e.g. 78.2)
extern int   cfg_cust_data_int;
extern float cfg_cust_test_par;

// ─── Derived IDs (built after WiFi radio is on) ──────────────
extern char gw_id[13];
extern char mac_last4[5];
extern char topic_status[128];
extern char topic_data[128];

// ─── Functions ───────────────────────────────────────────────
void setupConfig();
void saveConfig();
void buildMacIDs();