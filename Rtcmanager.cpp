#include "RTCManager.h"
#include <WiFi.h>
#include <time.h>

// ─── NTP settings ─────────────────────────────────────────────
#define NTP_SERVER1     "pool.ntp.org"
#define NTP_SERVER2     "time.google.com"
//#define NTP_GMT_OFFSET  21600   // UTC+6 Bangladesh ✓
#define NTP_GMT_OFFSET  21600      // UTC+6 (Bangladesh) = 6*3600
                                   // Change to your timezone offset in seconds:
                                   // UTC+0=0, UTC+5:30=19800, UTC-5=-18000
#define NTP_DST_OFFSET  0          // Daylight saving offset (0 if not used)
#define NTP_SYNC_TIMEOUT_MS 10000  // Wait up to 10s for NTP sync

static bool rtcSynced = false;

// ─── setupRTC ─────────────────────────────────────────────────
// Syncs ESP32 RTC from NTP. Call after WiFi connects.
// If NTP fails (no internet), RTC stays at epoch (unsynced).
void setupRTC() {
  Serial.println(F("[RTC] Syncing from NTP..."));

  configTime(NTP_GMT_OFFSET, NTP_DST_OFFSET, NTP_SERVER1, NTP_SERVER2);

  struct tm timeinfo;
  unsigned long start = millis();

  while (!getLocalTime(&timeinfo)) {
    if (millis() - start > NTP_SYNC_TIMEOUT_MS) {
      Serial.println(F("[RTC] NTP sync timeout — using RTC as-is."));
      return;
    }
    delay(200);
  }

  rtcSynced = true;
  char buf[32];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  Serial.printf("[RTC] NTP synced: %s (UTC+%.1f)\n",
                buf, (float)NTP_GMT_OFFSET / 3600.0f);
}

// ─── getRTCTimestamp ──────────────────────────────────────────
// Returns "2026-06-23 17:10:47" from ESP32 built-in RTC.
// Returns "1970-01-01 00:00:00" if RTC was never synced.
String getRTCTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return String("1970-01-01 00:00:00");
  }
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

// ─── setRTCFromString ─────────────────────────────────────────
// Parses "YYYY-MM-DD HH:MM:SS" and sets the ESP32 RTC.
// Called when MQTT command {"cmdName":"set_time","datetime":"..."} arrives.
// Returns true on success, false on parse error.
bool setRTCFromString(const char* datetime) {
  // Expected format: "2026-06-23 17:10:47"
  int yr, mo, dy, hr, mn, sc;
  int parsed = sscanf(datetime, "%d-%d-%d %d:%d:%d",
                      &yr, &mo, &dy, &hr, &mn, &sc);
  if (parsed != 6) {
    Serial.printf("[RTC] Bad datetime format: %s\n", datetime);
    Serial.println(F("[RTC] Expected: YYYY-MM-DD HH:MM:SS"));
    return false;
  }

  struct tm t = {};
  t.tm_year = yr - 1900;
  t.tm_mon  = mo - 1;
  t.tm_mday = dy;
  t.tm_hour = hr;
  t.tm_min  = mn;
  t.tm_sec  = sc;
  t.tm_isdst = -1;

  time_t epoch = mktime(&t);
  if (epoch == -1) {
    Serial.println(F("[RTC] mktime() failed — invalid date/time values."));
    return false;
  }

  // Set ESP32 system time (which drives the built-in RTC)
  struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
  settimeofday(&tv, nullptr);

  rtcSynced = true;
  Serial.printf("[RTC] Time set manually: %s\n", datetime);
  return true;
}

// ─── isRTCSynced ──────────────────────────────────────────────
bool isRTCSynced() {
  return rtcSynced;
}