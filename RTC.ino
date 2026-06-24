/*
 * ══════════════════════════════════════════════════════════
 *  ESP32 Smart Gateway  —  v1.0.5
 *  + ESP32 built-in RTC with NTP sync
 * ══════════════════════════════════════════════════════════
 *  BOOT BUTTON (GPIO 0):
 *    Short press  → publish "online" status immediately
 *    Hold 5 s     → factory reset (clears NVS, reboots)
 *
 *  MQTT COMMANDS → send to cfg_sub_topic:
 *    Set custom params : {"cmdName":"set_custom","cust_par":{"data_int":42,"test_par":9.9}}
 *    Set interval      : {"cmdName":"set_interval","interval_s":30}
 *    Both at once      : {"cmdName":"set_custom","cust_par":{"data_int":5,"test_par":1.1},"interval_s":15}
 *    Get params now    : {"cmdName":"get_custom"}
 *    Set RTC time      : {"cmdName":"set_time","datetime":"2026-06-23 17:10:47"}
 *
 *  RTC BEHAVIOUR:
 *    - On WiFi connect → auto-syncs from NTP (pool.ntp.org)
 *    - If NTP unavailable → RTC stays at last known time
 *    - MQTT set_time command → sets RTC manually anytime
 *    - Timestamp appears in BOTH telemetry and custom param payloads
 * ══════════════════════════════════════════════════════════
 */

/*
 * ══════════════════════════════════════════════════════════
 * ESP32 Smart Gateway  —  v1.0.5
 * + ESP32 built-in RTC with NTP sync
 * ══════════════════════════════════════════════════════════
 * BOOT BUTTON (GPIO 0):
 * Short press  → publish "online" status immediately
 * Hold 5 s     → factory reset (clears NVS, reboots)
 *
 * MQTT COMMANDS → send to cfg_sub_topic:
 * Set custom params : {"cmdName":"set_custom","cust_par":{"data_int":42,"test_par":9.9}}
 * Set interval      : {"cmdName":"set_interval","interval_s":30}
 * Both at once      : {"cmdName":"set_custom","cust_par":{"data_int":5,"test_par":1.1},"interval_s":15}
 * Get params now    : {"cmdName":"get_custom"}
 * Set RTC time      : {"cmdName":"set_time","datetime":"2026-06-23 17:10:47"}
 *
 * RTC BEHAVIOUR:
 * - On WiFi connect → auto-syncs from NTP (pool.ntp.org)
 * - If NTP unavailable → RTC stays at last known time
 * - MQTT set_time command → sets RTC manually anytime
 * - Timestamp appears in BOTH telemetry and custom param payloads
 * ══════════════════════════════════════════════════════════
 */

#include <WiFi.h>           // Fixes: 'WiFi' and 'WL_CONNECTED' was not declared in this scope
#include <esp_task_wdt.h>   // ESP32 Hardware Watchdog Library
#include "Config.h"
#include "WiFiProvisioning.h"
#include "MQTTManager.h"
#include "RTCManager.h"

// 30-Second Hardware Watchdog Timeout
#define WDT_TIMEOUT_SECONDS 30 

static unsigned long lastPublish = 0;
static bool          stationMode = false;
static unsigned long lastWifiCheck = 0;

// Background FreeRTOS task handling ongoing NTP Resyncs quietly
void ntpSyncTask(void *pvParameters) {
  for(;;) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println(F("[RTOS-NTP] Periodic background time synchronization check..."));
      setupRTC(); // Safely triggers configuration update inside clock matrix
    }
    vTaskDelay(pdMS_TO_TICKS(3600000)); // Sleep background task block for 1 hour
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println(F("\n\n=== ESP32 Smart Gateway Booting ==="));

  // Fixes: esp_task_wdt_init arguments error for ESP32 Arduino Core v3.x
  esp_task_wdt_config_t wdt_config = {
    .timeout_ms = WDT_TIMEOUT_SECONDS * 1000,
    .idle_core_mask = 0,
    .trigger_panic = true
  };
  esp_task_wdt_init(&wdt_config);
  esp_task_wdt_add(NULL); // Add main loop tracking thread

  // STEP 1 — Load all NVS values into cfg_* globals
  setupConfig();

  // STEP 2 — WiFi.mode + MAC read + connect or launch portal
  stationMode = setupWiFiProvisioning();

  if (stationMode) {
    // Synchronize clock instantly on first startup boot
    setupRTC();
    
    // Deploy background timekeeper worker under FreeRTOS scheduler
    xTaskCreatePinnedToCore(ntpSyncTask, "NTP_Task", 4096, NULL, 1, NULL, 0);

    // STEP 4 — MQTT connect with LWT
    setupMQTT();

    // ── Boot summary ────────────────────────────────────────
    Serial.println(F("\n─────────────────────────────────────────"));
    Serial.println(F("  MQTT TOPIC SUMMARY"));
    Serial.printf ("  Telemetry pub : %s\n", cfg_pub_topic);
    Serial.printf ("  Custom pub    : %s\n", cfg_cust_topic);
    Serial.printf ("  Command SUB   : %s  <- send commands HERE\n", cfg_sub_topic);
    Serial.printf ("  Interval      : %lu ms (%lu s)\n",
                   cfg_telemetry_interval_ms,
                   cfg_telemetry_interval_ms / 1000UL);
    Serial.printf ("  cust_data_int : %d\n",   cfg_cust_data_int);
    Serial.printf ("  cust_test_par : %.2f\n", cfg_cust_test_par);
    Serial.printf ("  RTC synced    : %s\n",   isRTCSynced() ? "YES (NTP)" : "NO — send set_time cmd");
    Serial.printf ("  Current time  : %s\n",   getRTCTimestamp().c_str());
    Serial.println(F("─────────────────────────────────────────\n"));

  } else {
    Serial.println(F("[Main] Portal mode — configure via http://192.168.4.1"));
  }
}

void loop() {
  // Service Watchdog timer every execution loop
  esp_task_wdt_reset();

  loopWiFiProvisioning();
  reconfigButton();

  if (stationMode) {
    // ── Structural Reconnection Handlers ──
    if (WiFi.status() != WL_CONNECTED) {
      if (millis() - lastWifiCheck >= 10000) { // Keep thread non-blocking, try reconnecting every 10s
        lastWifiCheck = millis();
        Serial.println(F("[Wi-Fi] Connection lost! Background auto-reconnecting..."));
        WiFi.disconnect();
        WiFi.begin(cfg_wifi_ssid, cfg_wifi_pass);
      }
    } else {
      // Run core loop MQTT tracking state machines only if internet layer link is valid
      loopMQTT(); 

      // Periodic Telemetry Transmission
      if (millis() - lastPublish >= cfg_telemetry_interval_ms) {
        lastPublish = millis();

        // Sensor telemetry → cfg_pub_topic
        publishTelemetry(
          28.4, 78.2, 18.5, 1450.0, 2.1, 35.0, 62.0, 79.1
        );

        // Custom params → cfg_cust_topic
        publishCustomParams();
      }
    }
  }
}