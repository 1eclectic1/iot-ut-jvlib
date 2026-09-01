// jv_reboot.cpp - daily reboot based on NTP / ezTime

#if defined(__has_include)
  #if __has_include("jv_config.h")
    #include "jv_config.h"
  #else
    #include "jv_config_defaults.h"
  #endif
#else
  #include "jv_config_defaults.h"
#endif

#include "jvlib.h"
#include "jv_internal.h"

namespace {
  bool dailyRebootEnabled = false;
  int  lastRebootDay      = -1;
}

void jvDailyRebootSetup() {
  dailyRebootEnabled = true;
  lastRebootDay = -1;

  if (timeStatus() == timeSet) {
    int h = jv_internal::myTZ.hour();
    int d = jv_internal::myTZ.dayOfYear();
    LOG_INFO("Local time now %02d:%02d dayOfYear=%d", h, jv_internal::myTZ.minute(), d);
    // Already past today's reboot hour → mark done (avoid immediate reboot)
    if (h > JV_REBOOT_HOUR) {
      lastRebootDay = d;
    }
  } else {
    LOG_WARN("Time not set yet; daily reboot will arm after NTP sync");
  }

  LOG_INFO("Daily reboot enabled at %02d:00 local time", JV_REBOOT_HOUR);
}

void jvCheckDailyReboot() {
  if (!dailyRebootEnabled) return;
  if (timeStatus() != timeSet) return;

  int currentHour = jv_internal::myTZ.hour();
  int currentDay  = jv_internal::myTZ.dayOfYear();

  if (currentDay == lastRebootDay) return;

  if (currentHour > JV_REBOOT_HOUR) {
    lastRebootDay = currentDay;
    return;
  }

  if (currentHour == JV_REBOOT_HOUR) {
    lastRebootDay = currentDay;
    LOG_INFO("Daily reboot triggered (local hour=%d dayOfYear=%d)", currentHour, currentDay);
    delay(500);
    ESP.restart();
  }
}
