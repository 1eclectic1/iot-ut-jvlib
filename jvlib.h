#ifndef JVLIB_H
#define JVLIB_H

// =============================================================================
// jvlib - Clean reusable library for ESP8266 / ESP32 IoT sensors & controllers
// =============================================================================
//
// Typical sketch:
//
//   #define me "SN02"
//   #define OW_PIN1 7
//   #define BME
//   // #define JV_ENABLE_FS_MANAGER
//   #include <jvlib.h>
//
//   void myCallback(char* topic, byte* payload, unsigned int length) { ... }
//
//   void setup() {
//     jv.begin();
//     jv.setCallback(myCallback);
//     jv.subscribe("garage/SN01");
//     jvWatchdogSetup();
//     jvDailyRebootSetup();
//   }
//
//   void loop() {
//     jv.loop();
//     jvWatchdogFeed();
//     jvCheckDailyReboot();
//     if (timeToPublish) {
//       jv.readSensors();
//       jv.publish();                 // generic telemetry
//       // or jv.publishRaw("my/topic", buf, true);
//     }
//   }
//
// =============================================================================

// MQTT remote logging (optional):
//   Publish to sensors/<deviceId>/log/control
//     {"enable":true,"level":"INFO"}  or  {"enable":false}
//   Logs appear on sensors/<deviceId>/log  (not retained, not for Influx)
//   Default: off. Override: #define JV_MQTT_LOG_DEFAULT_ON
//            #define JV_MQTT_LOG_DEFAULT_LEVEL LOG_INFO

#include <Arduino.h>
#include <PubSubClient.h>   // needed for MQTT_CALLBACK_SIGNATURE

// -----------------------------------------------------------------------------
// Version
// -----------------------------------------------------------------------------
#ifndef JVLIB_VERSION
#define JVLIB_VERSION "2026.09.05b"

#ifndef JV_ALTITUDE_M
#define JV_ALTITUDE_M 0.0
#endif
#ifndef JV_BME_PRES_BIAS
#define JV_BME_PRES_BIAS 0.0
#endif
#ifndef JV_OUTDOOR_TEMP_TIMEOUT_MS
#define JV_OUTDOOR_TEMP_TIMEOUT_MS 300000UL
#endif
#ifndef JV_TIMEZONE
#define JV_TIMEZONE "America/New_York"
#endif

#endif

// -----------------------------------------------------------------------------
// User-overridable defaults
// -----------------------------------------------------------------------------
#ifndef serialclock
#define serialclock 115200
#endif

#ifndef me
#define me "sensor"
#define JV_ME_DEFAULT 1   // sketch did not #define me
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

// MQTT / LWT
#ifndef LWT_BASE_TOPIC
#define LWT_BASE_TOPIC "sensors"
#endif

#ifndef MQTT_PUBLISH_TOPIC
#define MQTT_PUBLISH_TOPIC "fish/SNFT"
#endif

// Watchdog (milliseconds)
#ifndef JV_WDT_TIMEOUT_MS
#define JV_WDT_TIMEOUT_MS (2UL * 60 * 1000)   // 2 minutes
#endif

// Daily reboot hour (0-23, local time)
#ifndef JV_REBOOT_HOUR
#define JV_REBOOT_HOUR 3
#endif

// -----------------------------------------------------------------------------
// Logging
// -----------------------------------------------------------------------------
enum LogLevel {
  LOG_ERROR = 0,
  LOG_WARN  = 1,
  LOG_INFO  = 2,
  LOG_DEBUG = 3
};

#define LOG_ERROR(...) if (LOG_ENABLED && LOG_LEVEL >= LOG_ERROR) jvLog(LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...)  if (LOG_ENABLED && LOG_LEVEL >= LOG_WARN)  jvLog(LOG_WARN,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...)  if (LOG_ENABLED && LOG_LEVEL >= LOG_INFO)  jvLog(LOG_INFO,  __FILE__, __LINE__, __VA_ARGS__)
#define LOG_DEBUG(...) if (LOG_ENABLED && LOG_LEVEL >= LOG_DEBUG) jvLog(LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

void jvLog(LogLevel level, const char* file, int line, const char* format, ...);

// -----------------------------------------------------------------------------
// Core API
// -----------------------------------------------------------------------------
namespace jv {

  // Call once from setup() — uses #define me from the sketch for id/topics
  void beginWithName(const char* deviceName);
  inline void begin() { beginWithName(me); }

  // Call regularly from loop()
  void loop();

  // ----- MQTT -----
  void setCallback(MQTT_CALLBACK_SIGNATURE);
  bool subscribe(const char* topic, uint8_t qos = 0);
  bool publishRaw(const char* topic, const char* payload, bool retained = false);
  bool publishRaw(const char* topic, const uint8_t* payload, unsigned int len, bool retained = false);

  bool connected();        // WiFi + MQTT both up
  bool wifiConnected();
  bool mqttConnected();

  // ----- Sensors -----
  // Read all compiled-in sensors (no-op if none defined)
  void readSensors();

  // Publish generic telemetry (preserves common graphing keys)
  void publish();

  // Accessors
  const String& deviceId();  // unique: me-MAC (topics, MQTT client id)
  const String& id();        // payload id: #define me if set, else deviceId
  const String& ip();
  long          rssi();
  const String& version();

} // namespace jv

// -----------------------------------------------------------------------------
// Optional Watchdog (hardware-oriented)
// -----------------------------------------------------------------------------
void jvWatchdogSetup();   // call once after jv.begin()
void jvWatchdogFeed();    // call regularly from loop()

// -----------------------------------------------------------------------------
// Optional Daily Reboot (NTP / ezTime based)
// -----------------------------------------------------------------------------
void jvDailyRebootSetup();    // call once after jv.begin()
void jvCheckDailyReboot();    // call regularly from loop()

// -----------------------------------------------------------------------------
// Optional LittleFS File Manager
// -----------------------------------------------------------------------------
#ifdef JV_ENABLE_FS_MANAGER
  void jvFsManagerBegin();
  void jvFsManagerLoop();
#else
  inline void jvFsManagerBegin() {}
  inline void jvFsManagerLoop()  {}
#endif



// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// Sensor data + implementation (HEADER-ONLY so sketch #defines are visible)
// Requires C++17 inline variables (ESP8266 core 3.x / ESP32 OK)
// -----------------------------------------------------------------------------

#if (OW_PIN1 >= 0) || (OW_PIN2 >= 0) || (OW_PIN3 >= 0)
  #include <OneWire.h>
  #include <DallasTemperature.h>
  inline double publishedTemps[MAX_PUBLISHED_SENSORS];
  inline DeviceAddress publishedAddrs[MAX_PUBLISHED_SENSORS];
  inline int publishedSensorCount = 0;

  #if OW_PIN1 >= 0
    inline OneWire jv_ow1(OW_PIN1);
    inline DallasTemperature jv_ds1(&jv_ow1);
    inline DeviceAddress jv_addr1[MAX_SENSORS_PER_BUS];
    inline double jv_t1[MAX_SENSORS_PER_BUS];
    inline int jv_n1 = 0;
  #endif
  #if OW_PIN2 >= 0
    inline OneWire jv_ow2(OW_PIN2);
    inline DallasTemperature jv_ds2(&jv_ow2);
    inline DeviceAddress jv_addr2[MAX_SENSORS_PER_BUS];
    inline double jv_t2[MAX_SENSORS_PER_BUS];
    inline int jv_n2 = 0;
  #endif
  #if OW_PIN3 >= 0
    inline OneWire jv_ow3(OW_PIN3);
    inline DallasTemperature jv_ds3(&jv_ow3);
    inline DeviceAddress jv_addr3[MAX_SENSORS_PER_BUS];
    inline double jv_t3[MAX_SENSORS_PER_BUS];
    inline int jv_n3 = 0;
  #endif
#endif

#ifdef BME
  #include <Adafruit_BME280.h>
  inline Adafruit_BME280 jv_bme;
  inline bool jv_bmeOk = false;
  inline double bmetemp = -999.9, bmehum = 0.0, bmepres = 0.0, bmedew = -999.9, bmehi = -999.9;
  // Optional outdoor column temp for SLP (sketch can set these)
  inline double jv_outdoorTempC = -999.0;
  inline unsigned long jv_outdoorTempMs = 0;
#endif

#ifdef BMP
  #include <Adafruit_BMP280.h>
  inline Adafruit_BMP280 jv_bmp;
  inline double bmptemp = -999.9, bmppres = 0.0;
#endif

#if DHTPIN >= 0
  #include <dhtnew.h>
  inline DHTNEW jv_dht(DHTPIN);
  inline double dhttemp = -999.9, dhthum = 0.0, dhtdew = -999.9, dhthi = -999.9;
#endif

inline double jv_roundTemp(double v) {
  return (int)(v * 100.0 + 0.5) / 100.0;
}
inline double jv_heatIndex(double tf, double rh) {
  double hi = -42.379 + 2.04901523*tf + 10.14333127*rh
            - 0.22475541*tf*rh - 6.83783e-3*tf*tf
            - 5.481717e-2*rh*rh + 1.22874e-3*tf*tf*rh
            + 8.5282e-4*tf*rh*rh - 1.99e-6*tf*tf*rh*rh;
  return jv_roundTemp(hi);
}

#if (OW_PIN1 >= 0) || (OW_PIN2 >= 0) || (OW_PIN3 >= 0)
inline void jv_readBus(DallasTemperature& s, DeviceAddress* a, double* t, int& n, int id) {
  int d = s.getDeviceCount();
  if (d != n) {
    LOG_INFO("Bus %d count %d -> %d", id, n, d);
    n = (d < MAX_SENSORS_PER_BUS) ? d : MAX_SENSORS_PER_BUS;
    for (int i = 0; i < n; i++) s.getAddress(a[i], i);
  }
  for (int i = 0; i < n; i++) {
    float c = s.getTempC(a[i]);
    double f = NAN;
    if (c != DEVICE_DISCONNECTED_C && c != 85.0f) {
      f = jv_roundTemp(c * 1.8 + 32.0);
      if (!(f > -100.0 && f < 300.0)) f = NAN;
    }
    t[i] = f;
  }
}
#endif

inline void jvSensorsBegin() {
#if OW_PIN1 >= 0
  jv_ds1.begin(); jv_ds1.setWaitForConversion(false); jv_ds1.setResolution(12);
  jv_n1 = jv_ds1.getDeviceCount();
  if (jv_n1 > MAX_SENSORS_PER_BUS) jv_n1 = MAX_SENSORS_PER_BUS;
  for (int i = 0; i < jv_n1; i++) jv_ds1.getAddress(jv_addr1[i], i);
  LOG_INFO("Bus 1 pin %d: %d DS18B20", OW_PIN1, jv_n1);
#endif
#if OW_PIN2 >= 0
  jv_ds2.begin(); jv_ds2.setWaitForConversion(false); jv_ds2.setResolution(12);
  jv_n2 = jv_ds2.getDeviceCount();
  if (jv_n2 > MAX_SENSORS_PER_BUS) jv_n2 = MAX_SENSORS_PER_BUS;
  for (int i = 0; i < jv_n2; i++) jv_ds2.getAddress(jv_addr2[i], i);
  LOG_INFO("Bus 2 pin %d: %d DS18B20", OW_PIN2, jv_n2);
#endif
#if OW_PIN3 >= 0
  jv_ds3.begin(); jv_ds3.setWaitForConversion(false); jv_ds3.setResolution(12);
  jv_n3 = jv_ds3.getDeviceCount();
  if (jv_n3 > MAX_SENSORS_PER_BUS) jv_n3 = MAX_SENSORS_PER_BUS;
  for (int i = 0; i < jv_n3; i++) jv_ds3.getAddress(jv_addr3[i], i);
  LOG_INFO("Bus 3 pin %d: %d DS18B20", OW_PIN3, jv_n3);
#endif
#ifdef BME
  jv_bmeOk = jv_bme.begin(0x76);
  if (!jv_bmeOk) jv_bmeOk = jv_bme.begin(0x77);
  if (!jv_bmeOk) LOG_WARN("BME280 not found");
  else LOG_INFO("BME280 ready (altitude %.1f m)", (double)JV_ALTITUDE_M);
#endif
#ifdef BMP
  if (!jv_bmp.begin(0x76)) LOG_WARN("BMP280 not found");
  else LOG_INFO("BMP280 ready");
#endif
}

namespace jv {

inline void readSensors() {
#if (OW_PIN1 >= 0) || (OW_PIN2 >= 0) || (OW_PIN3 >= 0)
  #if OW_PIN1 >= 0
    jv_ds1.requestTemperatures();
  #endif
  #if OW_PIN2 >= 0
    jv_ds2.requestTemperatures();
  #endif
  #if OW_PIN3 >= 0
    jv_ds3.requestTemperatures();
  #endif
  delay(750);
  #if OW_PIN1 >= 0
    jv_readBus(jv_ds1, jv_addr1, jv_t1, jv_n1, 1);
  #endif
  #if OW_PIN2 >= 0
    jv_readBus(jv_ds2, jv_addr2, jv_t2, jv_n2, 2);
  #endif
  #if OW_PIN3 >= 0
    jv_readBus(jv_ds3, jv_addr3, jv_t3, jv_n3, 3);
  #endif
  publishedSensorCount = 0;
  auto add = [&](double* temps, DeviceAddress* addrs, int n) {
    for (int i = 0; i < n && publishedSensorCount < MAX_PUBLISHED_SENSORS; i++) {
      if (!isnan(temps[i])) {
        publishedTemps[publishedSensorCount] = temps[i];
        memcpy(publishedAddrs[publishedSensorCount], addrs[i], sizeof(DeviceAddress));
        publishedSensorCount++;
      }
    }
  };
  #if OW_PIN1 >= 0
    add(jv_t1, jv_addr1, jv_n1);
  #endif
  #if OW_PIN2 >= 0
    add(jv_t2, jv_addr2, jv_n2);
  #endif
  #if OW_PIN3 >= 0
    add(jv_t3, jv_addr3, jv_n3);
  #endif
#endif

#ifdef BME
  
if (jv_bmeOk) {
    bmehum = jv_bme.readHumidity();
    double tC = jv_bme.readTemperature();
    bmetemp = jv_roundTemp(tC * 1.8 + 32.0);

    // Sea-level pressure (hypsometric / ICAO-style correction)
    // station_Pa * (1 - (L*h)/(T+L*h))^(-g*M/(R*L)) ≈ exponent -5.257
    double station_Pa = jv_bme.readPressure();
    double altitude_m = (double)JV_ALTITUDE_M;
    if (altitude_m > 0.5) {
      double lapse = 0.0065;
      double altadj = lapse * altitude_m;
      double column_tC = tC;
      if (jv_outdoorTempC > -100.0 &&
          (millis() - jv_outdoorTempMs) < JV_OUTDOOR_TEMP_TIMEOUT_MS) {
        column_tC = jv_outdoorTempC;
      }
      double T_K = column_tC + altadj + 273.15;
      double sl_Pa = station_Pa * pow((1.0 - (altadj / T_K)), -5.257);
      bmepres = sl_Pa / 3386.39;
    } else {
      bmepres = station_Pa / 3386.39;  // station pressure only
    }
    bmepres = jv_roundTemp(bmepres + (double)JV_BME_PRES_BIAS);

    bmedew  = jv_roundTemp((tC - (100.0 - bmehum) / 5.0) * 1.8 + 32.0);
    bmehi   = jv_heatIndex(bmetemp, bmehum);
    bmehum  = jv_roundTemp(bmehum);
  } else {
    bmetemp = -999.9; bmehum = 0; bmepres = 0; bmedew = -999.9; bmehi = -999.9;
  }
#endif

#ifdef BMP
  if (jv_bmp.begin(0x76)) {
    double tC = jv_bmp.readTemperature();
    bmptemp = jv_roundTemp(tC * 1.8 + 32.0);
    bmppres = jv_roundTemp(jv_bmp.readPressure() / 3386.39);
  } else {
    bmptemp = -999.9; bmppres = 0;
  }
#endif

#if DHTPIN >= 0
  jv_dht.read();
  dhthum = jv_dht.getHumidity();
  if (dhthum > 5.0) {
    double tC = jv_dht.getTemperature();
    dhttemp = jv_roundTemp(tC * 1.8 + 32.0);
    dhtdew  = jv_roundTemp((tC - (100.0 - dhthum) / 5.0) * 1.8 + 32.0);
    dhthi   = jv_heatIndex(dhttemp, dhthum);
    dhthum  = jv_roundTemp(dhthum);
  } else {
    LOG_WARN("DHT invalid");
    dhttemp = -999.9; dhthum = 0; dhtdew = -999.9; dhthi = -999.9;
  }
#endif
}

} // namespace jv

#endif // JVLIB_H
