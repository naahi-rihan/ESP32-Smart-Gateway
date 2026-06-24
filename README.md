# ESP32 Smart Gateway System (v1.0.5)

An industrial-grade, multi-file ESP32 firmware architecture designed for smart environment tracking. This gateway features a local web captive provisioning portal, background asynchronous Wi-Fi recovery, localized hardware fallback systems, built-in real-time clock (RTC) syncing over NTP, and consolidated dual-channel JSON MQTT telemetry tracking.

## 🛠️ Hardware Requirements
* **Microcontroller:** ESP32 Development Board (e.g., ESP32-WROOM-32)
* **Status LED Indicators:** Network & System status loops
* **Physical Function Button (GPIO 0):**
  * **Short Press (< 3s):** Forces an instantaneous "online" status heartbeat broadcast over MQTT.
  * **Long Press (>= 5s):** Erases NVS (`sdtl_cfg`) data blocks and triggers a hardware reset into captive access point provisioning mode.

## 📦 Core Dependencies
Ensure these libraries are installed via your Arduino Library Manager or `platformio.ini`:
* **PubSubClient** (by Nick O'Leary) — High-efficiency MQTT client tracking.
* **ArduinoJson** (v6.x / v7.x) — Standardized serialization for incoming and outbound packets.

## 📡 MQTT Packet Architectures

### 1. Periodic Telemetry Publication (`cfg_pub_topic`)
The system outputs environment telemetry strings tracking raw gas, thermal, and humidity profiles at regular intervals to a single publication topic channel:
```json
{
  "gw_id": "001122334455",
  "type": "SEN",
  "addr": "B0E0",
  "cmdName": "Sensor_Data",
  "timestamp": "2026-06-24 15:40:12",
  "data": {
    "temp": 28.4,
    "humid": 78.2,
    "nh3": 18.5,
    "co2": 1450.0,
    "h2s": 2.1,
    "pm25": 35.0,
    "pm10": 62.0,

This is an ESP32-based multi-file smart gateway using built-in RTC clock timing, background Wi-Fi auto-reconnection mechanics, custom parameters saved to NVS Preferences, and consolidated dual-channel MQTT telemetry.
    "thi": 79.1
  }
}
