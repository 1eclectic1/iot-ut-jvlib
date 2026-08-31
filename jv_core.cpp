#if defined(__has_include)
  #if __has_include("jv_config.h")
    #include "jv_config.h"
  #else
    #include "jv_config_defaults.h"
  #endif
#else
  #include "jv_config_defaults.h"
#endif

// jv_core.cpp
// jv_core.cpp - initialisation, logging, WiFi, MQTT, LWT

#include "jvlib.h"
#include "jv_internal.h"
#include <credentials.h>

#include <Wire.h>
#ifdef ESP32
  #include <esp_mac.h>
#endif
#include <stdarg.h>
#include <strings.h>
#include <functional>

void jvSensorsBegin();  // defined in jv_sensors.cpp (global)

// -----------------------------------------------------------------------------
// Shared objects
// -----------------------------------------------------------------------------
namespace jv_internal {

  WiFiClient   wifiClient;
  PubSubClient mqtt(wifiClient);
  Timezone     myTZ;

  String hostname     = me;
  String deviceIdStr;
  String myIp;
  long   currentRssi  = 0;
  String combinedVersion;
  String resetReasonStr;

  bool mqttWasConnected = false;

  // Remote MQTT logging (default off; enable via control topic)
  bool mqttLogEnabled = false;
  LogLevel mqttLogLevel = LOG_WARN;
  std::function<void(char*, uint8_t*, unsigned int)> userCallback;
  bool inMqttLogPublish = false;  // prevent recursion

  String logTopic() {
    return String(LWT_BASE_TOPIC) + "/" + deviceIdStr + "/log";
  }
  String logControlTopic() {
    return String(LWT_BASE_TOPIC) + "/" + deviceIdStr + "/log/control";
  }

  void handleLogControl(char* topic, byte* payload, unsigned int length) {
    if (strcmp(topic, logControlTopic().c_str()) != 0) return;
    StaticJsonDocument<192> doc;
    if (deserializeJson(doc, payload, length)) return;

    if (doc.containsKey("enable")) {
      mqttLogEnabled = doc["enable"] | false;
    }
    if (doc.containsKey("level")) {
      const char* lv = doc["level"] | "WARN";
      if (!strcasecmp(lv, "ERROR")) mqttLogLevel = LOG_ERROR;
      else if (!strcasecmp(lv, "WARN")) mqttLogLevel = LOG_WARN;
      else if (!strcasecmp(lv, "INFO")) mqttLogLevel = LOG_INFO;
      else if (!strcasecmp(lv, "DEBUG")) mqttLogLevel = LOG_DEBUG;
    }
    // Serial only here — avoid MQTT log recursion
    Serial.printf("[jvlib] MQTT log %s level=%d\n",
                  mqttLogEnabled ? "ON" : "OFF", (int)mqttLogLevel);
  }

  void mqttDispatch(char* topic, byte* payload, unsigned int length) {
    handleLogControl(topic, payload, length);
    if (userCallback) {
      userCallback(topic, payload, length);
    }
  }

  // Topics the sketch has asked us to (re)subscribe after reconnect
  static const int MAX_SUBS = 8;
  String subTopics[MAX_SUBS];
  uint8_t subQos[MAX_SUBS];
  int subCount = 0;

  String statusTopic() {
    return String(LWT_BASE_TOPIC) + "/" + deviceIdStr + "/status";
  }

  void publishStatus(bool online) {
    if (!mqtt.connected()) return;

    StaticJsonDocument<256> doc;
    doc["id"]     = deviceIdStr;
    doc["status"] = online ? "online" : "offline";
    doc["ip"]     = myIp;
    doc["rssi"]   = currentRssi;
    doc["ver"]    = combinedVersion;
    doc["ts"]     = myTZ.dateTime(ISO8601);
    if (resetReasonStr.length()) doc["reset"] = resetReasonStr;

    char buf[256];
    serializeJson(doc, buf, sizeof(buf));
    mqtt.publish(statusTopic().c_str(), buf, true);
    LOG_INFO("Status %s -> %s", online ? "online" : "offline", statusTopic().c_str());
  }

  void resubscribe() {
    for (int i = 0; i < subCount; i++) {
      bool ok = mqtt.subscribe(subTopics[i].c_str(), subQos[i]);
      LOG_INFO("Resubscribe %s → %s", subTopics[i].c_str(), ok ? "OK" : "FAIL");
    }
  }

  void ensureMqtt() {
    if (mqtt.connected()) {
      mqttWasConnected = true;
      return;
    }

    if (WiFi.status() != WL_CONNECTED) return;

    LOG_INFO("MQTT connecting as %s ...", deviceIdStr.c_str());

    String offlinePayload = "{\"id\":\"" + deviceIdStr + "\",\"status\":\"offline\"}";
    bool ok = mqtt.connect(
        deviceIdStr.c_str(),
        mqttID,
        mqttPASS,
        statusTopic().c_str(),
        1, true,
        offlinePayload.c_str()
    );

    if (ok) {
      LOG_INFO("MQTT connected");
      mqtt.setBufferSize(1024);
      publishStatus(true);
      // Always subscribe log control (default logging still off until enabled)
      mqtt.subscribe(logControlTopic().c_str(), 0);
      resubscribe();
      mqttWasConnected = true;
    } else {
      LOG_WARN("MQTT connect failed, rc=%d", mqtt.state());
    }
  }

} // namespace jv_internal

// Public accessors
namespace jv {
  const String& deviceId() { return jv_internal::deviceIdStr; }
  const String& ip()       { return jv_internal::myIp; }
  long          rssi()     { return jv_internal::currentRssi; }
  const String& version()  { return jv_internal::combinedVersion; }
}

// -----------------------------------------------------------------------------
// Logging
// -----------------------------------------------------------------------------
void jvLog(LogLevel level, const char* file, int line, const char* format, ...) {
  if (!LOG_ENABLED) return;

  String ts = jv_internal::myTZ.dateTime("Y-m-d H:i:s");
  const char* lvl =
      (level == LOG_ERROR) ? "ERROR" :
      (level == LOG_WARN)  ? "WARN " :
      (level == LOG_INFO)  ? "INFO " : "DEBUG";

  const char* fn = strrchr(file, '/') ? strrchr(file, '/') + 1 : file;

  char buf[320];
  snprintf(buf, sizeof(buf), "[%s] %s (%s:%d) ", ts.c_str(), lvl, fn, line);

  va_list args;
  va_start(args, format);
  vsnprintf(buf + strlen(buf), sizeof(buf) - strlen(buf), format, args);
  va_end(args);

  Serial.println(buf);
  Serial.flush();

  // Optional MQTT log (enabled remotely; never retained)
  if (jv_internal::mqttLogEnabled
      && level <= jv_internal::mqttLogLevel
      && jv_internal::mqtt.connected()
      && !jv_internal::inMqttLogPublish) {
    jv_internal::inMqttLogPublish = true;
    StaticJsonDocument<384> doc;
    doc["id"] = jv_internal::deviceIdStr;
    doc["lvl"] = (level == LOG_ERROR) ? "ERROR" :
                 (level == LOG_WARN)  ? "WARN"  :
                 (level == LOG_INFO)  ? "INFO"  : "DEBUG";
    doc["msg"] = buf;
    char out[384];
    serializeJson(doc, out, sizeof(out));
    jv_internal::mqtt.publish(jv_internal::logTopic().c_str(), out, false);
    jv_internal::inMqttLogPublish = false;
  }
}

// -----------------------------------------------------------------------------
// WiFi
// -----------------------------------------------------------------------------
static void buildDeviceId();

static bool connectWifi(unsigned long timeoutMs = 45000) {
  if (WiFi.status() == WL_CONNECTED) {
    jv_internal::currentRssi = WiFi.RSSI();
    jv_internal::myIp = WiFi.localIP().toString();
    return true;
  }

  LOG_INFO("Connecting to WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.hostname(jv_internal::hostname.c_str());
  WiFi.begin(mySSID, myPASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(400);
    Serial.print('.');
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    jv_internal::currentRssi = WiFi.RSSI();
    jv_internal::myIp = WiFi.localIP().toString();
    // Re-read MAC after radio is up if we still have a zero id
    if (jv_internal::deviceIdStr.endsWith("-000000000000") ||
        jv_internal::deviceIdStr.endsWith("-000000001500")) {
      buildDeviceId();
      LOG_INFO("Device ID refreshed: %s", jv_internal::deviceIdStr.c_str());
    }
    LOG_INFO("WiFi OK  IP=%s  RSSI=%ld", jv_internal::myIp.c_str(), jv_internal::currentRssi);
    return true;
  }

  LOG_WARN("WiFi connect failed");
  return false;
}

static void buildDeviceId() {
  // MAC is often zeros on ESP32-C3 if WiFi STA mode has not been set yet
  WiFi.mode(WIFI_STA);
  delay(20);

  uint8_t mac[6] = {0,0,0,0,0,0};
#ifdef ESP32
  // eFuse/base MAC — reliable before WiFi.begin()
  if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
    WiFi.macAddress(mac);
  }
#else
  WiFi.macAddress(mac);
#endif

  bool allZero = true;
  for (int i = 0; i < 6; i++) {
    if (mac[i] != 0) { allZero = false; break; }
  }
  if (allZero) {
    WiFi.macAddress(mac);
  }

  char macStr[13];
  sprintf(macStr, "%02X%02X%02X%02X%02X%02X",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  jv_internal::deviceIdStr = jv_internal::hostname + "-" + macStr;
}

// -----------------------------------------------------------------------------
// Public API
// -----------------------------------------------------------------------------
namespace jv {

void begin() {
  Serial.begin(serialclock);
  delay(1200);

#ifndef mainver
  #define mainver "0.0.0"
#endif
  jv_internal::combinedVersion = String(mainver) + ":" + String(JVLIB_VERSION);

  LOG_INFO("Booting %s  %s", jv_internal::hostname.c_str(), jv_internal::combinedVersion.c_str());

#ifdef ESP32
  {
    esp_reset_reason_t rr = esp_reset_reason();
    const char* rrs =
      rr == ESP_RST_POWERON ? "poweron" :
      rr == ESP_RST_SW ? "software" :
      rr == ESP_RST_PANIC ? "panic" :
      rr == ESP_RST_INT_WDT ? "int_wdt" :
      rr == ESP_RST_TASK_WDT ? "task_wdt" :
      rr == ESP_RST_WDT ? "wdt" :
      rr == ESP_RST_DEEPSLEEP ? "deepsleep" :
      rr == ESP_RST_BROWNOUT ? "brownout" :
      rr == ESP_RST_SDIO ? "sdio" : "other";
    jv_internal::resetReasonStr = rrs;
    LOG_INFO("Reset reason: %s (%d)", rrs, (int)rr);
  }
#else
  jv_internal::resetReasonStr = ESP.getResetReason();
  LOG_INFO("Reset reason: %s", jv_internal::resetReasonStr.c_str());
#endif

  buildDeviceId();
  LOG_INFO("Device ID: %s", jv_internal::deviceIdStr.c_str());

  jv_internal::myTZ.setLocation(F("America/New_York"));

  connectWifi();
  waitForSync();

  // Only start I2C if something that needs it is compiled in
#if defined(BME) || defined(BMP) || (i2cdata >= 0)
  Wire.begin(i2cdata, i2cclock);
#endif

  jv_internal::mqtt.setServer(mqtt_server, 1883);
  jv_internal::mqtt.setCallback(jv_internal::mqttDispatch);
#ifdef JV_MQTT_LOG_DEFAULT_ON
  jv_internal::mqttLogEnabled = true;
#endif
#ifdef JV_MQTT_LOG_DEFAULT_LEVEL
  jv_internal::mqttLogLevel = (LogLevel)JV_MQTT_LOG_DEFAULT_LEVEL;
#endif
  jv_internal::ensureMqtt();
  // Sensor init: call jvSensorsBegin() from sketch after jv::begin()

  LOG_INFO("jvlib ready");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt > 15000) {
      lastAttempt = millis();
      connectWifi(12000);
    }
  } else {
    jv_internal::currentRssi = WiFi.RSSI();
  }

  if (!jv_internal::mqtt.connected()) {
    static unsigned long lastMqtt = 0;
    if (millis() - lastMqtt > 8000) {
      lastMqtt = millis();
      jv_internal::ensureMqtt();
    }
  } else {
    jv_internal::mqtt.loop();
  }
}

// ----- MQTT helpers -----

void setCallback(MQTT_CALLBACK_SIGNATURE) {
  jv_internal::userCallback = callback;
  jv_internal::mqtt.setCallback(jv_internal::mqttDispatch);
}

bool subscribe(const char* topic, uint8_t qos) {
  if (jv_internal::subCount >= jv_internal::MAX_SUBS) {
    LOG_WARN("subscribe: too many topics");
    return false;
  }
  // Avoid duplicates
  for (int i = 0; i < jv_internal::subCount; i++) {
    if (jv_internal::subTopics[i] == topic) {
      jv_internal::subQos[i] = qos;
      if (jv_internal::mqtt.connected())
        return jv_internal::mqtt.subscribe(topic, qos);
      return true;
    }
  }
  jv_internal::subTopics[jv_internal::subCount] = topic;
  jv_internal::subQos[jv_internal::subCount] = qos;
  jv_internal::subCount++;

  if (jv_internal::mqtt.connected())
    return jv_internal::mqtt.subscribe(topic, qos);
  return true;   // will be subscribed on next connect
}

bool publishRaw(const char* topic, const char* payload, bool retained) {
  if (!jv_internal::mqtt.connected()) {
    LOG_WARN("publishRaw: MQTT not connected");
    return false;
  }
  bool ok = jv_internal::mqtt.publish(topic, payload, retained);
  LOG_DEBUG("publishRaw %s → %s", topic, ok ? "OK" : "FAIL");
  return ok;
}

bool publishRaw(const char* topic, const uint8_t* payload, unsigned int len, bool retained) {
  if (!jv_internal::mqtt.connected()) {
    LOG_WARN("publishRaw: MQTT not connected");
    return false;
  }
  return jv_internal::mqtt.publish(topic, payload, len, retained);
}

bool wifiConnected()  { return WiFi.status() == WL_CONNECTED; }
bool mqttConnected()  { return jv_internal::mqtt.connected(); }
bool connected()      { return wifiConnected() && mqttConnected(); }

} // namespace jv
