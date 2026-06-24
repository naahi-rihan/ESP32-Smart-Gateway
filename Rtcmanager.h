#pragma once
#include <Arduino.h>

// ─── RTC Manager ─────────────────────────────────────────────
// Uses ESP32 built-in RTC (no external hardware needed).
// On WiFi connect → syncs from NTP automatically.
// Via MQTT command → set_time can update it manually.
//
// NTP sync writes to ESP32 RTC via configTime().
// All subsequent reads use getLocalTime() from the RTC.
// ─────────────────────────────────────────────────────────────

// Call once after WiFi connects — syncs ESP32 RTC from NTP
void setupRTC();

// Returns current timestamp string: "2026-06-23 17:10:47"
// Reads from ESP32 built-in RTC (valid after setupRTC or setRTCFromString)
String getRTCTimestamp();

// Set ESP32 RTC manually from a datetime string "YYYY-MM-DD HH:MM:SS"
// Called when MQTT command {"cmdName":"set_time","datetime":"..."} arrives
bool setRTCFromString(const char* datetime);

// Returns true if RTC has been synced (NTP or manual set)
bool isRTCSynced();