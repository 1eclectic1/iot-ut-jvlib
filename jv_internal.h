// jv_internal.h - shared symbols between jvlib translation units
#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <ezTime.h>
#include <ArduinoJson.h>

#ifdef ESP32
  #include <WiFi.h>
#else
  #include <ESP8266WiFi.h>
#endif

namespace jv_internal {

  extern WiFiClient   wifiClient;
  extern PubSubClient mqtt;
  extern Timezone     myTZ;

  extern String hostname;
  extern String deviceIdStr;
  extern String myIp;
  extern long   currentRssi;
  extern String combinedVersion;

  void ensureMqtt();
  void publishStatus(bool online);
  String statusTopic();

} // namespace jv_internal
