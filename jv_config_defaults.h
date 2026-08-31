#pragma once
// Fallback defaults if sketch has no jv_config.h
// Prefer placing jv_config.h in the sketch folder.

#ifndef serialclock
#define serialclock 115200
#endif
#ifndef me
#define me "sensor"
#endif
#ifndef LOG_ENABLED
#define LOG_ENABLED true
#endif
#ifndef LOG_LEVEL
#define LOG_LEVEL LOG_INFO
#endif
#ifndef OW_PIN1
#define OW_PIN1 -1
#endif
#ifndef OW_PIN2
#define OW_PIN2 -1
#endif
#ifndef OW_PIN3
#define OW_PIN3 -1
#endif
#ifndef MAX_SENSORS_PER_BUS
#define MAX_SENSORS_PER_BUS 10
#endif
#ifndef MAX_PUBLISHED_SENSORS
#define MAX_PUBLISHED_SENSORS (MAX_SENSORS_PER_BUS * 3)
#endif
#ifndef DHTPIN
#define DHTPIN -1
#endif
#ifndef i2cdata
  #ifdef ESP32
    #define i2cdata 27
    #define i2cclock 22
  #else
    #define i2cdata D6
    #define i2cclock D5
  #endif
#endif
#ifndef LWT_BASE_TOPIC
#define LWT_BASE_TOPIC "sensors"
#endif
#ifndef MQTT_PUBLISH_TOPIC
#define MQTT_PUBLISH_TOPIC "fish/SNFT"
#endif
#ifndef JV_WDT_TIMEOUT_MS
#define JV_WDT_TIMEOUT_MS (2UL * 60 * 1000)
#endif
#ifndef JV_REBOOT_HOUR
#define JV_REBOOT_HOUR 3
#endif

#ifndef JV_ALTITUDE_M
#define JV_ALTITUDE_M 0.0
#endif
#ifndef JV_BME_PRES_BIAS
#define JV_BME_PRES_BIAS 0.0
#endif
#ifndef JV_OUTDOOR_TEMP_TIMEOUT_MS
#define JV_OUTDOOR_TEMP_TIMEOUT_MS 300000UL
#endif
