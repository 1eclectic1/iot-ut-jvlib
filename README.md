# jvlib

Clean reusable library for ESP8266 / ESP32 IoT sensors and controllers.

Replaces the older monolithic `jvcommon`.

## Features

- WiFi + MQTT with retained per-device LWT/status on `sensors/<id>/status`
- Multi-bus DS18B20, BME280, BMP280, DHT (all optional)
- MQTT callback, subscribe, and raw publish helpers
- Hardware-oriented watchdog (default 2 min timeout)
- Daily reboot at configurable hour (default 03:00 local)
- Optional LittleFS web file manager
- Logging with levels
- Telemetry helpers that keep existing graphing keys where possible

## Device ID

`hostname` + `-` + full MAC (no colons), e.g. `SN02-AABBCCDDEEFF`

## Typical sensor sketch

```cpp
#define me "SN02"
#define OW_PIN1 7
#define BME
#include <jvlib.h>

void setup() {
  jv.begin();
  jvWatchdogSetup();
  jvDailyRebootSetup();
}

void loop() {
  jv.loop();
  jvWatchdogFeed();
  jvCheckDailyReboot();

  static unsigned long last = 0;
  if (millis() - last > 120000) {
    last = millis();
    jv.readSensors();
    jv.publish();
  }
}
```

## Typical controller sketch (dimmer, garage, etc.)

```cpp
#define me "FTxx"
// no sensor defines → no sensor code or delays
#include <jvlib.h>

void callback(char* topic, byte* payload, unsigned int length) {
  // handle commands
}

void setup() {
  jv.begin();
  jv.setCallback(callback);
  jv.subscribe("fish/dimtest");
  jvWatchdogSetup();
}

void loop() {
  jv.loop();
  jvWatchdogFeed();
  // your actuator logic here
  // jv.publishRaw("fish/FTDS", jsonBuf, true);
}
```

## Key API additions

```cpp
jv.setCallback(callback);
jv.subscribe("topic");
jv.publishRaw("topic", payload, retained);
jv.connected();       // WiFi + MQTT
jv.wifiConnected();
jv.mqttConnected();
```

Subscriptions are remembered and automatically re-subscribed after MQTT reconnects.

## Required libraries

- PubSubClient
- ArduinoJson
- ezTime
- OneWire + DallasTemperature (only if using DS18B20)
- Adafruit BME280 / BMP280 (if used)
- dhtnew (if using DHT)

Plus `credentials.h` with `mySSID`, `myPASSWORD`, `mqtt_server`, `mqttID`, `mqttPASS`.

## Notes

- BAT / battery support removed
- Offline buffering deliberately omitted
- Clean break from jvcommon – existing sketches need a small rewrite
