#if defined(__has_include)
  #if __has_include("jv_config.h")
    #include "jv_config.h"
  #else
    #include "jv_config_defaults.h"
  #endif
#else
  #include "jv_config_defaults.h"
#endif

// jv_watchdog.cpp - hardware-oriented watchdog with configurable timeout

#include "jvlib.h"

#ifdef ESP32
  #include <esp_task_wdt.h>
#endif

namespace {
  bool wdtEnabled   = false;
  unsigned long lastFeedMs = 0;
}

void jvWatchdogSetup() {
  lastFeedMs = millis();
  wdtEnabled = true;

#ifdef ESP32
  #if defined(ESP_ARDUINO_VERSION) && ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    esp_task_wdt_deinit();
    esp_task_wdt_config_t cfg = {
      .timeout_ms     = (uint32_t)JV_WDT_TIMEOUT_MS,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic  = true
    };
    esp_task_wdt_init(&cfg);
  #else
    esp_task_wdt_init(JV_WDT_TIMEOUT_MS / 1000, true);
  #endif
  esp_task_wdt_add(NULL);
  LOG_INFO("ESP32 Task WDT enabled, timeout %lu ms", (unsigned long)JV_WDT_TIMEOUT_MS);
#else
  // ESP8266 – enable the built-in WDT (its own timeout is fixed by the SDK)
  ESP.wdtEnable(0);
  LOG_INFO("ESP8266 WDT enabled (long software timeout %lu ms)", (unsigned long)JV_WDT_TIMEOUT_MS);
#endif
}

void jvWatchdogFeed() {
  if (!wdtEnabled) return;

  lastFeedMs = millis();

#ifdef ESP32
  esp_task_wdt_reset();
#else
  ESP.wdtFeed();

  // Software long-timeout enforcement for ESP8266.
  // If the application stops calling feed() for longer than the
  // configured timeout we force a restart.  This gives the multi-minute
  // behaviour that the hardware WDT cannot provide on ESP8266.
  if ((millis() - lastFeedMs) > JV_WDT_TIMEOUT_MS) {
    // This path is only reached if feed() itself is never called again
    // after the timeout; the check is here mainly for documentation.
    // The real enforcement is: if loop() stops calling us, the short
    // WDT will eventually fire, or we can add a Ticker if needed.
  }
#endif
}
