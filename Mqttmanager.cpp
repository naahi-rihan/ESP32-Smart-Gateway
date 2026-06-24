#include "MQTTManager.h"
#include "Config.h"
#include "RTCManager.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>

static WiFiClient   espClient;
static PubSubClient mqtt(espClient);

// Helper to determine the gateway ID safely if global variable isn't ready
static const char* getValidGwId() {
  if (strlen(gw_id) > 0) return gw_id;
  static char fallbackId[13] = "";
  if (strlen(fallbackId) == 0) {
    uint8_t mac[6];
    WiFi.macAddress(mac);
    snprintf(fallbackId, sizeof(fallbackId), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  }
  return fallbackId;
}

// ─── Status payload ───────────────────────────────────────────
static String buildStatusPayload(const char* availability) {
  StaticJsonDocument<256> doc;
  doc["gw_id"]        = getValidGwId();
  doc["type"]         = "SEN";
  doc["addr"]         = cfg_device_address;
  doc["availability"] = availability;
  doc["fw"]           = FW_VERSION;
  doc["timestamp"]    = getRTCTimestamp();
  String out;
  serializeJson(doc, out);
  return out;
}

// ─── ACK payload ─────────────────────────────────────────────
static void publishAck(const char* cmdName, const char* result) {
  if (!mqtt.connected()) return;
  StaticJsonDocument<300> doc;
  doc["gw_id"]      = getValidGwId();
  doc["cmdName"]    = "cmd_ack";
  doc["source_cmd"] = cmdName;
  doc["status"]     = result;
  doc["timestamp"]  = getRTCTimestamp();
  doc["interval_s"] = (uint32_t)(cfg_telemetry_interval_ms / 1000UL);
  JsonObject cp = doc.createNestedObject("cust_par");
  cp["data_int"] = cfg_cust_data_int;
  cp["test_par"] = cfg_cust_test_par;
  String out;
  serializeJson(doc, out);
  
  // Forces routing to the main pub topic
  mqtt.publish(cfg_pub_topic, out.c_str());
  Serial.print(F("[MQTT ACK] ")); Serial.println(out);
}

// ─── Command handler ──────────────────────────────────────────
static void handleCommand(const char* rawJson, unsigned int length) {
  StaticJsonDocument<300> doc;
  DeserializationError err = deserializeJson(doc, rawJson, length);
  if (err) {
    Serial.print(F("[CMD] JSON parse error: ")); Serial.println(err.c_str());
    return;
  }

  const char* cmd = doc["cmdName"] | "";
  bool changed = false;

  if (strcmp(cmd, "set_time") == 0) {
    const char* dt = doc["datetime"] | "";
    if (strlen(dt) > 0) {
      if (setRTCFromString(dt)) {
        publishAck("set_time", "applied");
      } else {
        publishAck("set_time", "error_bad_format");
      }
    } else {
      Serial.println(F("[CMD] set_time: missing 'datetime' field."));
      publishAck("set_time", "error_missing_datetime");
    }
    return;
  }

  if (strcmp(cmd, "set_custom") == 0 || doc.containsKey("cust_par")) {
    JsonObject cp = doc["cust_par"];
    if (!cp.isNull()) {
      if (cp.containsKey("data_int")) {
        cfg_cust_data_int = cp["data_int"].as<int>();
        Serial.printf("[CMD] data_int → %d\n", cfg_cust_data_int);
        changed = true;
      }
      if (cp.containsKey("test_par")) {
        cfg_cust_test_par = cp["test_par"].as<float>();
        Serial.printf("[CMD] test_par → %.2f\n", cfg_cust_test_par);
        changed = true;
      }
    }
  }

  if (strcmp(cmd, "set_interval") == 0 || doc.containsKey("interval_s")) {
    uint32_t s = doc["interval_s"] | 0;
    if (s >= 1 && s <= 3600) {
      cfg_telemetry_interval_ms = (unsigned long)s * 1000UL;
      Serial.printf("[CMD] interval → %u s (%lu ms)\n", s, cfg_telemetry_interval_ms);
      changed = true;
    } else {
      Serial.println(F("[CMD] interval_s out of range (1-3600), ignored."));
    }
  }

  if (strcmp(cmd, "get_custom") == 0) {
    Serial.println(F("[CMD] get_custom → publishing now"));
    publishCustomParams();
    return;
  }

  if (changed) {
    Preferences prefs;
    prefs.begin(NVS_NAMESPACE, false);
    prefs.putInt  ("cust_data_int",  cfg_cust_data_int);
    prefs.putFloat("cust_test_par",  cfg_cust_test_par);
    prefs.putUInt ("telem_interval", (uint32_t)(cfg_telemetry_interval_ms / 1000UL));
    prefs.end();
    Serial.println(F("[CMD] Changes saved to NVS."));
    publishAck(cmd, "applied");
  }
}

static void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print(F("[MQTT CMD IN] "));
  Serial.print(topic);
  Serial.print(F(" → "));
  for (unsigned int i = 0; i < length; i++) Serial.print((char)payload[i]);
  Serial.println();

  if (strcmp(topic, cfg_sub_topic) == 0) {
    handleCommand((const char*)payload, length);
  }
}

// ─── Publish status ───────────────────────────────────────────
void publishStatus(const char* availability) {
  if (!mqtt.connected()) return;
  String msg = buildStatusPayload(availability);
  
  // Forces status/availability to publish on the shared pub topic
  const char* pubTo = strlen(cfg_pub_topic) ? cfg_pub_topic : topic_data;
  mqtt.publish(pubTo, msg.c_str(), true);  // retained
  Serial.print(F("[MQTT STATUS] ")); Serial.print(pubTo);
  Serial.print(F(" → ")); Serial.println(msg);
}

// ─── Publish sensor telemetry ─────────────────────────────────
void publishTelemetry(float temp, float humid, float nh3, float co2,
                      float h2s,  float pm25,  float pm10, float thi) {
  if (!mqtt.connected()) return;

  StaticJsonDocument<350> doc;
  doc["gw_id"]     = getValidGwId();
  doc["type"]      = "SEN";
  doc["addr"]      = cfg_device_address;
  doc["cmdName"]   = "Sensor_Data";
  doc["timestamp"] = getRTCTimestamp();

  JsonObject data = doc.createNestedObject("data");
  data["temp"]  = temp;   data["humid"] = humid;
  data["nh3"]   = nh3;    data["co2"]   = co2;
  data["h2s"]   = h2s;    data["pm25"]  = pm25;
  data["pm10"]  = pm10;   data["thi"]   = thi;

  String payload;
  serializeJson(doc, payload);
  const char* pubTo = strlen(cfg_pub_topic) ? cfg_pub_topic : topic_data;
  mqtt.publish(pubTo, payload.c_str());
  Serial.print(F("[MQTT TELEM] ")); Serial.print(pubTo);
  Serial.print(F(" → ")); Serial.println(payload);
}

// ─── Publish custom parameters ────────────────────────────────
void publishCustomParams() {
  if (!mqtt.connected()) return;

  // FIX: Explicitly routes payload to cfg_pub_topic instead of cfg_cust_topic
  const char* pubTo = strlen(cfg_pub_topic) ? cfg_pub_topic : topic_data;

  StaticJsonDocument<300> doc;
  doc["gw_id"]   = getValidGwId();
  doc["type"]    = "SEN";
  doc["addr"]    = cfg_device_address;
  doc["cmdName"] = "custom_par";

  JsonObject cust = doc.createNestedObject("cust_par");
  cust["data_int"]  = cfg_cust_data_int;
  cust["test_par"]  = cfg_cust_test_par;
  cust["timestamp"] = getRTCTimestamp();

  String payload;
  serializeJson(doc, payload);
  mqtt.publish(pubTo, payload.c_str());
  Serial.print(F("[MQTT CUSTOM] ")); Serial.print(pubTo);
  Serial.print(F(" → ")); Serial.println(payload);
}

// ─── Connect with LWT ─────────────────────────────────────────
static void mqttConnect() {
  String lwtPayload = buildStatusPayload("offline");
  
  // Safe validation check of the publish topic target
  const char* pubTo = strlen(cfg_pub_topic) ? cfg_pub_topic : topic_data;
  const char* currentGw = getValidGwId();

  while (!mqtt.connected()) {
    Serial.print(F("[MQTT] Connecting..."));
    const char* user = strlen(cfg_mqtt_user) ? cfg_mqtt_user : nullptr;
    const char* pass = strlen(cfg_mqtt_pass) ? cfg_mqtt_pass : nullptr;

    // FIX: Set the Last Will and Testament (LWT) topic directly to the primary pubTo topic
    bool ok = mqtt.connect(currentGw, user, pass,
                           pubTo, 1, true, lwtPayload.c_str());
    if (ok) {
      Serial.println(F(" connected."));

      if (strlen(cfg_sub_topic) > 0) {
        mqtt.subscribe(cfg_sub_topic, 1);
        Serial.print(F("[MQTT] Subscribed (cmd): "));
        Serial.println(cfg_sub_topic);
      } else {
        Serial.println(F("[MQTT] WARNING: no sub_topic — commands disabled."));
      }
      publishStatus("online");

    } else {
      Serial.printf(" failed rc=%d, retry in 3s\n", mqtt.state());
      delay(3000);
    }
  }
}

void setupMQTT() {
  mqtt.setServer(cfg_mqtt_server, cfg_mqtt_port);
  mqtt.setCallback(mqttCallback);
  mqtt.setBufferSize(512);
  mqttConnect();
}

void loopMQTT() {
  if (!mqtt.connected()) {
    Serial.println(F("[MQTT] Disconnected — reconnecting..."));
    mqttConnect();
  }
  mqtt.loop();
}