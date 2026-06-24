#include "WiFiProvisioning.h"
#include "Config.h"
#include "MQTTManager.h"
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

void handleRoot();
void handleInfo();
void handleScan();
void handleSave();

WebServer server(80);
bool isAPMode = false;

unsigned long buttonPressTime = 0;
bool buttonPressed = false;

// ─── Portal HTML ──────────────────────────────────────────────
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Smart Data Technologies - Gateway Setup</title>
  <style>
    :root { --primary: #0056b3; --bg: #f0f4f8; --text: #333; --border: #ccc; }
    * { box-sizing: border-box; }
    body { font-family: 'Segoe UI', sans-serif; background: var(--bg); color: var(--text); padding: 20px; display: flex; justify-content: center; margin: 0; }
    .container { background: #fff; max-width: 520px; width: 100%; padding: 30px; border-radius: 12px; box-shadow: 0 8px 24px rgba(0,0,0,0.12); }
    .header { text-align: center; margin-bottom: 18px; }
    .logo-text { font-size: 26px; font-weight: 900; color: var(--primary); letter-spacing: 2px; }
    .logo-sub  { font-size: 11px; color: #888; text-transform: uppercase; letter-spacing: 1px; }
    .info-badge { background: #eff6ff; border: 1px solid #bfdbfe; border-radius: 8px; padding: 14px 16px; margin-bottom: 18px; display: grid; grid-template-columns: 1fr 1fr; gap: 8px 12px; }
    .ib-label { font-size: 10px; letter-spacing: 1px; color: #64748b; text-transform: uppercase; }
    .ib-value { font-size: 14px; font-weight: 700; color: #2563eb; margin-top: 2px; word-break: break-all; }
    .ib-status { color: #d97706; }
    .section-title { font-size: 13px; border-bottom: 2px solid var(--primary); padding-bottom: 4px; margin: 20px 0 12px; font-weight: 700; color: var(--primary); text-transform: uppercase; letter-spacing: 1px; }
    .form-group { margin-bottom: 12px; }
    label { display: block; font-weight: 600; margin-bottom: 4px; font-size: 13px; }
    input, select { width: 100%; padding: 10px; border: 1px solid var(--border); border-radius: 6px; font-size: 14px; outline: none; transition: border 0.25s; }
    input:focus, select:focus { border-color: var(--primary); }
    input[readonly] { background: #e2e8f0; color: #64748b; cursor: not-allowed; }
    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .grid-3 { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; }
    .hint { font-size: 11px; color: #94a3b8; margin-top: 3px; }
    .btn { background: var(--primary); color: #fff; border: none; padding: 14px; width: 100%; border-radius: 6px; font-size: 16px; font-weight: bold; cursor: pointer; margin-top: 20px; transition: background 0.25s; }
    .btn:hover { background: #004494; }
    .scanning-text { font-size: 11px; color: #d9534f; margin-left: 6px; display: none; }
    .fw-note { text-align: center; font-size: 11px; color: #94a3b8; margin-top: 14px; }
    .hidden { display: none !important; }
    .unit-label { font-size: 11px; color: #64748b; font-weight: normal; }
  </style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="logo-text">SMART DATA</div>
    <div class="logo-sub">Technologies Ltd.</div>
  </div>

  <form action="/save" method="POST">

    <div class="section-title">Device Identity</div>
    <div class="grid-2">
      <div class="form-group">
        <label>Device Type</label>
        <select name="dev_type" id="sel-dev-type">
          <option value="Sensor">Sensor</option>
          <option value="Gateway">Gateway</option>
          <option value="Controller">Controller</option>
          <option value="Actuator">Actuator</option>
        </select>
      </div>
      <div class="form-group">
        <label>Device Address</label>
        <input type="text" name="dev_addr" id="inp-dev-addr" placeholder="e.g. B0E0" maxlength="16">
      </div>
    </div>

    <div class="section-title">Wi-Fi Setup</div>
    <div class="form-group">
      <label>Network (SSID) <span id="scan-status" class="scanning-text">Scanning...</span></label>
      <select id="ssid" name="ssid" required>
        <option value="">-- Scanning Networks --</option>
      </select>
    </div>
    <div class="form-group" id="pwd-group">
      <label>Wi-Fi Password</label>
      <input type="password" id="password" name="password" placeholder="Enter network password">
    </div>

    <div class="section-title">MQTT Broker</div>
    <div class="form-group">
      <label>Gateway ID (MAC) <span class="unit-label">— read-only</span></label>
      <input type="text" id="gw-id-field" readonly>
    </div>
    <div class="form-group">
      <label>Broker IP / Domain</label>
      <input type="text" name="mqtt_server" placeholder="e.g. 192.168.68.79" required>
    </div>
    <div class="grid-2">
      <div class="form-group">
        <label>Port</label>
        <input type="number" name="mqtt_port" value="1883" required>
      </div>
      <div class="form-group">
        <label>Telemetry Cadence <span class="unit-label">(seconds)</span></label>
        <input type="number" name="telem_interval" id="telem-interval" min="5" max="3600" value="5" required>
      </div>
    </div>
    <div class="grid-2">
      <div class="form-group">
        <label>Username</label>
        <input type="text" name="mqtt_user" placeholder="Optional">
      </div>
      <div class="form-group">
        <label>Password</label>
        <input type="password" name="mqtt_pass" placeholder="Optional">
      </div>
    </div>
    <div class="grid-2">
      <div class="form-group">
        <label>Publish Topic</label>
        <input type="text" name="pub_topic" placeholder="e.g. SDTL/barn/+/telemetry" required>
      </div>
      <div class="form-group">
        <label>Subscribe Topic</label>
        <input type="text" name="sub_topic" placeholder="e.g. SDTL/barn/+/cmd" required>
      </div>
    </div>

    <div class="hidden">
      <input type="text" name="cust_topic" id="inp-cust-topic" value="">
      <input type="number" name="cust_data_int" id="cust-data-int" value="68">
      <input type="number" name="cust_test_par" id="cust-test-par" value="78.2">
    </div>

    <button type="submit" class="btn">&#128190; Save &amp; Connect</button>
  </form>

  <div class="info-badge" style="margin-top: 25px; margin-bottom: 5px;">
    <div>
      <div class="ib-label">Device Type</div>
      <div class="ib-value" id="ib-type">—</div>
    </div>
    <div>
      <div class="ib-label">Device Address</div>
      <div class="ib-value" id="ib-addr">—</div>
    </div>
  </div>

  <div class="fw-note">Firmware v<span id="ib-fw">—</span></div>
</div>

<script>
  let networkData = [];

  // ── Load live device info ──────────────────────────────────
  fetch('/info').then(r => r.json()).then(d => {
    document.getElementById('ib-type').textContent = d.device_type || '—';
    document.getElementById('ib-addr').textContent = d.device_address || '—';
    document.getElementById('ib-fw').textContent = d.fw || '—';
    document.getElementById('gw-id-field').value = d.gw_id || '—';

    let sel = document.getElementById('sel-dev-type');
    for (let i = 0; i < sel.options.length; i++) {
      if (sel.options[i].value === d.device_type) { sel.selectedIndex = i; break; }
    }
    if (d.device_address) document.getElementById('inp-dev-addr').value = d.device_address;
    if (d.telem_interval) document.getElementById('telem-interval').value = d.telem_interval;
  });

  // ── Scan WiFi Networks ─────────────────────────────────────
  function startScan() {
    const statusSpan = document.getElementById('scan-status');
    const selectEl = document.getElementById('ssid');
    statusSpan.style.display = 'inline';
    
    fetch('/scan').then(r => r.json()).then(data => {
      statusSpan.style.display = 'none';
      selectEl.innerHTML = '<option value="">-- Choose Network --</option>';
      
      data.sort((a,b) => b.rssi - a.rssi);
      
      data.forEach(n => {
        let opt = document.createElement('option');
        opt.value = n.ssid;
        let lockSymbol = n.secure ? '🔒' : '🔓';
        opt.textContent = `${n.ssid} (${n.rssi} dBm) ${lockSymbol}`;
        selectEl.appendChild(opt);
      });
    }).catch(err => {
      statusSpan.style.display = 'none';
      selectEl.innerHTML = '<option value="">Error scanning networks</option>';
    });
  }

  window.addEventListener('DOMContentLoaded', startScan);
</script>
</body>
</html>
)rawliteral";

// ─── Handlers ─────────────────────────────────────────────────
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleInfo() {
  uint32_t interval_s = (uint32_t)(cfg_telemetry_interval_ms / 1000UL);
  String json = "{";
  json += "\"gw_id\":\"" + String(gw_id) + "\",";
  json += "\"device_type\":\"" + String(cfg_device_type) + "\",";
  json += "\"device_address\":\"" + String(cfg_device_address) + "\",";
  json += "\"fw\":\"" + String(FW_VERSION) + "\",";
  json += "\"status\":\"Provisioning\",";
  json += "\"telem_interval\":" + String(interval_s) + ",";
  json += "\"cust_data_int\":" + String(cfg_cust_data_int) + ",";
  json += "\"cust_test_par\":" + String(cfg_cust_test_par) + ",";
  json += "\"cust_topic\":\"" + String(cfg_cust_topic) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleScan() {
  int n = WiFi.scanNetworks();
  String json = "[";
  for (int i = 0; i < n; ++i) {
    if (i) json += ",";
    bool sec = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
    json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",\"secure\":" + (sec ? "true":"false") + ",\"rssi\":" + String(WiFi.RSSI(i)) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleSave() {
  if (server.hasArg("dev_type"))       strlcpy(cfg_device_type, server.arg("dev_type").c_str(), sizeof(cfg_device_type));
  if (server.hasArg("dev_addr"))       strlcpy(cfg_device_address, server.arg("dev_addr").c_str(), sizeof(cfg_device_address));
  if (server.hasArg("ssid"))           strlcpy(cfg_wifi_ssid, server.arg("ssid").c_str(), sizeof(cfg_wifi_ssid));
  if (server.hasArg("password"))       strlcpy(cfg_wifi_pass, server.arg("password").c_str(), sizeof(cfg_wifi_pass));
  if (server.hasArg("mqtt_server"))   strlcpy(cfg_mqtt_server, server.arg("mqtt_server").c_str(), sizeof(cfg_mqtt_server));
  if (server.hasArg("mqtt_port"))     cfg_mqtt_port = server.arg("mqtt_port").toInt();
  if (server.hasArg("mqtt_user"))     strlcpy(cfg_mqtt_user, server.arg("mqtt_user").c_str(), sizeof(cfg_mqtt_user));
  if (server.hasArg("mqtt_pass"))     strlcpy(cfg_mqtt_pass, server.arg("mqtt_pass").c_str(), sizeof(cfg_mqtt_pass));
  if (server.hasArg("pub_topic"))     strlcpy(cfg_pub_topic, server.arg("pub_topic").c_str(), sizeof(cfg_pub_topic));
  if (server.hasArg("sub_topic"))     strlcpy(cfg_sub_topic, server.arg("sub_topic").c_str(), sizeof(cfg_sub_topic));
  
  if (server.hasArg("cust_topic"))    strlcpy(cfg_cust_topic, server.arg("cust_topic").c_str(), sizeof(cfg_cust_topic));
  if (server.hasArg("cust_data_int")) cfg_cust_data_int = server.arg("cust_data_int").toInt();
  if (server.hasArg("cust_test_par")) cfg_cust_test_par = server.arg("cust_test_par").toFloat();

  if (server.hasArg("telem_interval")) {
    long sec = server.arg("telem_interval").toInt();
    if (sec < 5) sec = 5;
    if (sec > 3600) sec = 3600;
    cfg_telemetry_interval_ms = sec * 1000UL;
  }

  Preferences prefs;
  prefs.begin(NVS_NAMESPACE, false);
  prefs.putString("dev_type",   cfg_device_type);
  prefs.putString("dev_addr",   cfg_device_address);
  prefs.putString("wf_ssid",    cfg_wifi_ssid);
  prefs.putString("wf_pass",    cfg_wifi_pass);
  prefs.putString("mq_server",  cfg_mqtt_server);
  prefs.putInt("mq_port",       cfg_mqtt_port);
  prefs.putString("mq_user",    cfg_mqtt_user);
  prefs.putString("mq_pass",    cfg_mqtt_pass);
  prefs.putString("pub_topic",  cfg_pub_topic);
  prefs.putString("sub_topic",  cfg_sub_topic);
  prefs.putString("cust_topic", cfg_cust_topic);
  prefs.putInt("c_data_int",    cfg_cust_data_int);
  prefs.putFloat("c_test_par",  cfg_cust_test_par);
  prefs.putLong("tel_interval", cfg_telemetry_interval_ms);
  prefs.end();

  String html = 
    "<html><body style='font-family:Segoe UI,sans-serif;text-align:center;"
    "margin-top:60px;background:#f0f4f8;'>"
    "<div style='background:#fff;display:inline-block;padding:40px 50px;"
    "border-radius:12px;box-shadow:0 8px 24px rgba(0,0,0,.1);'>"
    "<h2 style='color:#0056b3'>&#10003; Configuration Saved</h2>"
    "<p style='color:#555;'>Device parameters updated successfully in local memory.</p>"
    "<p style='font-weight:600;color:#222;'>The gateway will now restart and try connecting...</p>"
    "</div></body></html>";
  
  server.send(200, "text/html", html);
  delay(2000);
  ESP.restart();
}

// ─── External Access Frameworks ──────────────────────────────
bool setupWiFiProvisioning() {
  WiFi.disconnect(true);
  delay(100);
  WiFi.mode(WIFI_AP_STA);

  if (strlen(cfg_wifi_ssid) > 0) {
    Serial.printf("[Wi-Fi] Attempting connect: %s\n", cfg_wifi_ssid);
    WiFi.begin(cfg_wifi_ssid, cfg_wifi_pass);
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
      delay(250);
      Serial.print(".");
    }
    Serial.println();
  }

  if (WiFi.status() == WL_CONNECTED) {
    isAPMode = false;
    Serial.print(F("[Wi-Fi] Connected! IP: "));
    Serial.println(WiFi.localIP());
    return true;
  }

  isAPMode = true;
  char apName[32];
  snprintf(apName, sizeof(apName), "SDTL-Setup-%s", mac_last4);
  WiFi.softAP(apName);
  delay(100);

  Serial.printf("[Wi-Fi] AP: \"%s\" → http://192.168.4.1\n", apName);

  server.on("/",     HTTP_GET,  handleRoot);
  server.on("/info", HTTP_GET,  handleInfo);
  server.on("/scan", HTTP_GET,  handleScan);
  server.on("/save", HTTP_POST, handleSave);
  server.onNotFound([]() {
    server.sendHeader("Location", "/");
    server.send(302, "text/plain", "");
  });
  server.begin();
  return false;
}

void loopWiFiProvisioning() {
  if (isAPMode) server.handleClient();
}

void reconfigButton() {
  if (digitalRead(BOOT_BUTTON) == LOW) {
    if (!buttonPressed) {
      buttonPressTime = millis();
      buttonPressed   = true;
    } else if (millis() - buttonPressTime >= 5000) {
      Serial.println(F("[Button] Factory reset triggered via 5s hold..."));
      Preferences prefs;
      prefs.begin(NVS_NAMESPACE, false);
      prefs.clear();
      prefs.end();
      Serial.println(F("[NVS] Preferences cleared. Re-booting system..."));
      delay(500);
      ESP.restart();
    }
  } else {
    if (buttonPressed) {
      unsigned long duration = millis() - buttonPressTime;
      buttonPressed = false;
      if (duration < 3000) {
        Serial.println(F("[Button] Short press captured. Updating status string layout..."));
        publishStatus("online");
      } else if (duration >= 3000 && duration < 5000) {
        Serial.println(F("[Button] 3-second hold captured. Switching into Configuration Portal..."));
        cfg_wifi_ssid[0] = '\0';
        Preferences prefs;
        prefs.begin(NVS_NAMESPACE, false);
        prefs.putString("wf_ssid", "");
        prefs.end();
        ESP.restart();
      }
    }
  }
}