#if defined(__has_include)
  #if __has_include("jv_config.h")
    #include "jv_config.h"
  #else
    #include "jv_config_defaults.h"
  #endif
#else
  #include "jv_config_defaults.h"
#endif

// jv_mqtt_publish.cpp - generic telemetry publish

#include "jvlib.h"
#include "jv_internal.h"

namespace jv {

void publish() {
  StaticJsonDocument<2048> doc;

  doc["name"] = me;
  doc["id"]   = deviceId();

#if (OW_PIN1 >= 0) || (OW_PIN2 >= 0) || (OW_PIN3 >= 0)
  for (int i = 0; i < publishedSensorCount; i++) {
    if (!isnan(publishedTemps[i])) {
      char key[20];
      sprintf(key, "%02X%02X%02X%02X%02X%02X%02X%02X",
              publishedAddrs[i][0], publishedAddrs[i][1],
              publishedAddrs[i][2], publishedAddrs[i][3],
              publishedAddrs[i][4], publishedAddrs[i][5],
              publishedAddrs[i][6], publishedAddrs[i][7]);
      doc[key] = publishedTemps[i];
    }
  }
#endif

#ifdef BME
  if (bmetemp > -900.0) {
    doc["bmetemp"] = bmetemp;
    doc["bmehum"]  = bmehum;
    doc["bmepres"] = bmepres;
    doc["bmedew"]  = bmedew;
    doc["bmehi"]   = bmehi;
  }
#endif

#ifdef BMP
  if (bmptemp > -900.0) {
    doc["bmptemp"] = bmptemp;
    doc["bmppres"] = bmppres;
  }
#endif

#if DHTPIN >= 0
  if (dhttemp > -900.0) {
    doc["dhttemp"] = dhttemp;
    doc["dhthum"]  = dhthum;
    doc["dhtdew"]  = dhtdew;
    doc["dhthi"]   = dhthi;
  }
#endif

  doc["IP"]   = ip();
  doc["RSSI"] = rssi();
  doc["Ver"]  = version();

  char buf[1536];
  serializeJson(doc, buf, sizeof(buf));

  bool ok = publishRaw(MQTT_PUBLISH_TOPIC, buf, false);
  LOG_INFO("publish to %s → %s", MQTT_PUBLISH_TOPIC, ok ? "OK" : "FAIL");

  serializeJsonPretty(doc, Serial);
  Serial.println();
}

} // namespace jv
